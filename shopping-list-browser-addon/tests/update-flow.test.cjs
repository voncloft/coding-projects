'use strict';
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');
const source = name => readFileSync(path.join(__dirname, '..', name), 'utf8');
const tick = () => new Promise(resolve => setImmediate(resolve));
const settle = async () => { for (let i = 0; i < 6; i++) await tick(); };

function storage() {
  const data = {};
  return {
    async get(defaults) { return { ...defaults, ...data }; },
    async set(values) { Object.assign(data, values); }
  };
}

class Element {
  constructor(tag = 'div', text = '', attrs = {}) {
    this.tagName = tag.toUpperCase();
    this.textContent = text;
    this.attrs = attrs;
    this.style = {};
    this.dataset = {};
    this.children = [];
    this.listeners = {};
    this.value = '';
    this.classList = { add() {} };
  }
  get innerText() { return this.textContent; }
  getAttribute(key) { return this.attrs[key] || null; }
  append(...nodes) {
    nodes.forEach(node => { node.parentElement = this; this.children.push(node); });
  }
  appendChild(node) { this.append(node); }
  addEventListener(name, callback) { this.listeners[name] = callback; }
  closest() {
    for (let n = this; n; n = n.parentElement) {
      if (['walmart-price-updater-panel', 'walmart-food-import-helper-panel'].includes(n.id) ||
          ['SCRIPT', 'STYLE', 'NOSCRIPT'].includes(n.tagName) ||
          'hidden' in n.attrs || n.attrs['aria-hidden'] === 'true') return n;
    }
    return null;
  }
}
function descendants(node) { return [node, ...node.children.flatMap(descendants)]; }
function productData(product) {
  return { props: { pageProps: { initialData: { data: { product } } } } };
}
function makeDocument(state) {
  const root = new Element('html');
  const body = new Element('body');
  root.append(body);
  const next = new Element('script');
  return {
    documentElement: root, body, title: 'Test product',
    createElement: tag => new Element(tag),
    addEventListener() {},
    getElementById(id) {
      if (id === '__NEXT_DATA__') {
        next.textContent = state.next ? JSON.stringify(state.next) : '';
        return next;
      }
      return descendants(root).find(n => n.id === id) || null;
    },
    querySelectorAll(selector) {
      if (selector === 'script[type="application/ld+json"]') {
        return (state.ld || []).map(value => new Element('script', JSON.stringify(value)));
      }
      const match = selector.match(/\[([^*]+)\*="([^"]+)" i\]/);
      return match ? (state.nodes || []).filter(n =>
        (n.getAttribute(match[1]) || '').toLowerCase().includes(match[2])) : [];
    },
    createTreeWalker() {
      const texts = (state.texts || []).map(text => ({ textContent: text, parentElement: body }));
      // Include our injected text to prove it cannot masquerade as current page data.
      texts.push(...descendants(root).filter(n => n.textContent).map(n =>
        ({ textContent: n.textContent, parentElement: n })));
      let index = 0;
      return { nextNode: () => texts[index++] || null };
    }
  };
}
function content(state, sendMessage) {
  let listener;
  const document = makeDocument(state);
  const messages = [];
  const browser = {
    storage: { local: storage() },
    runtime: {
      onMessage: { addListener(fn) { listener = fn; } },
      async sendMessage(message) {
        messages.push(message);
        return sendMessage ? sendMessage(message) :
          { ok: true, data: { ok: true, department: message.department, affected_rows: 1 } };
      }
    }
  };
  vm.runInNewContext(source('content-script.js'), {
    browser, document, URL, URLSearchParams, NodeFilter: { SHOW_TEXT: 4 },
    location: { href: 'https://www.walmart.com/ip/Test/123#id=440', hostname: 'www.walmart.com' },
    window: { setInterval() {}, setTimeout() {}, prompt() { return null; } }
  });
  return {
    document, messages,
    read: () => listener({ type: 'getWalmartData' }),
    button: text => descendants(document.documentElement).find(n => n.tagName === 'BUTTON' && n.textContent === text)
  };
}
function background(responder) {
  let listener;
  const requests = [];
  vm.runInNewContext(source('background.js'), {
    URL, URLSearchParams, console,
    browser: {
      storage: { local: storage() },
      runtime: { onMessage: { addListener(fn) { listener = fn; } } }
    },
    async fetch(url, options) {
      requests.push({ url, ...options });
      return responder ? responder(options) : {
        ok: true, status: 200,
        async text() { return JSON.stringify({ ok: true, affected_rows: 1, department: new URLSearchParams(options.body).get('dept') }); }
      };
    }
  });
  return { requests, send: message => listener(message, {}) };
}

const baseProduct = { id: '123', name: 'Milk', priceInfo: { currentPrice: { price: 3.25 } } };
for (const [label, fields, aisle] of [
  ['direct aisle', { aisle: 'a12' }, 'A12'],
  ['nested aisle', { aisleLocation: { label: 'Aisle A4' } }, 'A4'],
  ['numeric aisle', { aisle: 12 }, '12'],
  ['location label', { locationText: 'In B9 aisle' }, 'B9'],
  ['bare location code', { locationText: 'B9' }, 'B9'],
  ['location prefix', { shelfLocation: 'Location: B9' }, 'B9'],
  ['fulfillment label', { fulfillmentOptions: [{ fulfillmentLabel: 'Aisle C5' }] }, 'C5'],
  ['no aisle', {}, null],
  ['category is not aisle', { category: 'Dairy' }, null],
  ['store number is not aisle', { location: 1234, storeLocation: { zipCode: '46805' } }, null]
]) {
  test(`extract ${label}`, async () => {
    const data = await content({ next: productData({ ...baseProduct, ...fields }) }).read();
    assert.equal(data.department, aisle);
    assert.equal(data.price, '3.25');
    assert.equal(data.itemId, '123');
    assert.equal(data.dbId, '440');
  });
}

test('live DOM aisle overrides embedded product aisle', async () => {
  const node = new Element('span', 'Aisle B22', { 'data-testid': 'product-aisle' });
  const data = await content({ next: productData({ ...baseProduct, aisle: 'A1' }), nodes: [node] }).read();
  assert.equal(data.department, 'B22');
});
test('aria-label and split page text identify an aisle', async () => {
  const node = new Element('span', '', { 'aria-label': 'Aisle C8' });
  assert.equal((await content({ next: productData(baseProduct), nodes: [node] }).read()).department, 'C8');
  assert.equal((await content({ next: productData(baseProduct), texts: ['Find it in', 'aisle', 'A7'] }).read()).department, 'A7');
});
test('location DOM accepts aisle codes but ignores store numbers', async () => {
  const node = new Element('span', 'B9', { 'data-testid': 'product-location' });
  const state = { next: productData(baseProduct), nodes: [node] };
  const page = content(state);
  assert.equal((await page.read()).department, 'B9');
  node.textContent = '1234';
  assert.equal((await page.read()).department, null);
});
test('only matching search result supplies price and aisle, including a URL fragment', async () => {
  const next = { props: { pageProps: { initialData: { searchResult: { itemStacks: [{ items: [
    { usItemId: '999', aisle: 'Z99', priceInfo: { linePrice: 99 } },
    { usItemId: '123', aisle: 'D4', priceInfo: { linePrice: 2.5 } }
  ] }] } } } } };
  const data = await content({ next }).read();
  assert.equal(data.department, 'D4');
  assert.equal(data.price, '2.50');
});
test('JSON-LD price fallback also carries a DOM aisle', async () => {
  const data = await content({ ld: [{ '@type': 'Product', offers: { price: 1.99 } }], texts: ['Aisle E6'] }).read();
  assert.equal(data.department, 'E6');
  assert.equal(data.source, 'json-ld');
});
test('refresh after a store change does not reuse our own previous aisle display', async () => {
  const state = { next: productData(baseProduct), texts: ['Aisle A7'] };
  const page = content(state);
  page.button('Edit Price / Aisle').listeners.click();
  await settle();
  assert.ok(descendants(page.document.documentElement).some(n => n.textContent === 'Aisle: A7'));
  state.texts = [];
  page.button('Refresh').listeners.click();
  await settle();
  assert.equal((await page.read()).department, null);
  assert.ok(descendants(page.document.documentElement).some(n => n.textContent === 'Aisle: not found (keep existing)'));
});
test('importer panel and hidden nodes cannot supply an aisle', async () => {
  const panel = new Element();
  panel.id = 'walmart-food-import-helper-panel';
  const node = new Element('span', 'Aisle Z99', { 'data-testid': 'aisle' });
  panel.append(node);
  const hidden = new Element('span', 'Aisle Z98', { 'data-testid': 'aisle', hidden: '' });
  assert.equal((await content({ next: productData(baseProduct), nodes: [node, hidden] }).read()).department, null);
});
test('on-page update sends the displayed aisle through background to the server', async () => {
  const bg = background();
  const page = content({ next: productData({ ...baseProduct, aisle: 'a7' }) }, bg.send);
  page.button('Update Server').listeners.click();
  await settle();
  assert.equal(bg.requests.length, 1);
  assert.deepEqual(Object.fromEntries(new URLSearchParams(bg.requests[0].body)), { id: '440', updated_value: '3.25', dept: 'A7' });
  assert.equal(bg.requests[0].credentials, 'include');
});
test('popup displays and submits the aisle', async () => {
  const bg = background();
  const nodes = {};
  const pageData = { ok: true, url: 'https://www.walmart.com/ip/Test/123', itemId: '123', dbId: '440', title: 'Milk', price: '3.25', department: 'B8' };
  vm.runInNewContext(source('popup.js'), {
    URL,
    document: { getElementById(id) { return nodes[id] ||= new Element(); } },
    browser: {
      storage: { local: storage() },
      runtime: { getManifest: () => ({ version: '1.9' }), sendMessage: bg.send, openOptionsPage() {} },
      tabs: { async query() { return [{ id: 1, url: pageData.url }]; }, async sendMessage() { return { ...pageData }; } }
    }
  });
  await settle();
  assert.equal(nodes.aisle.value, 'B8');
  await nodes.saveCurrent.listeners.click();
  assert.equal(new URLSearchParams(bg.requests[0].body).get('dept'), 'B8');
  assert.match(nodes.status.textContent, /Aisle B8 saved/);
  pageData.department = null;
  await nodes.refresh.listeners.click();
  assert.equal(nodes.aisle.value, '');
});
test('missing aisle omits dept and keeps price-only compatibility', async () => {
  const bg = background(() => ({ ok: true, status: 200, async text() { return '{"ok":true}'; } }));
  const response = await bg.send({ type: 'submitServerUpdate', dbId: 440, price: '3.25' });
  assert.equal(response.ok, true);
  assert.equal(new URLSearchParams(bg.requests[0].body).has('dept'), false);
});
for (const text of ['{"ok":true,"affected_rows":1}', '', '{"ok":true,"department":"Z99"}']) {
  test(`old or incorrect server acknowledgement is reported: ${text || 'empty'}`, async () => {
    const bg = background(() => ({ ok: true, status: 200, async text() { return text; } }));
    const result = await bg.send({ type: 'submitServerUpdate', dbId: 440, price: '3.25', department: 'A7' });
    assert.equal(result.ok, false);
    assert.match(result.error, /did not confirm the aisle/);
  });
}
test('invalid aisle is rejected before any network request', async () => {
  const bg = background();
  const result = await bg.send({ type: 'submitServerUpdate', dbId: 440, price: '3.25', department: "A7'; DROP TABLE items;--" });
  assert.equal(result.ok, false);
  assert.equal(bg.requests.length, 0);
});
test('server failure is reported instead of claiming success', async () => {
  const bg = background(() => ({ ok: false, status: 500, async text() { return '{"ok":false}'; } }));
  assert.equal((await bg.send({ type: 'submitServerUpdate', dbId: 440, price: '3.25', department: 'A7' })).ok, false);
});
