'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test, after } = require('node:test');
const root = path.resolve(__dirname, '..');
const php = process.env.PHP_BIN || 'php';
// Keep fixtures under cwd so PHP WebAssembly can mount them without host access.
const temp = fs.mkdtempSync(path.join(root, '.server-test-'));
fs.mkdirSync(path.join(temp, 'ajax'));
fs.mkdirSync(path.join(temp, 'include'));
fs.copyFileSync(path.join(root, 'server/ajax/edit_price.php'), path.join(temp, 'ajax/edit_price.php'));
fs.copyFileSync(path.join(__dirname, 'server-harness.php'), path.join(temp, 'harness.php'));
fs.writeFileSync(path.join(temp, 'include/passwords.php'), '<?php $conn = new TestDatabase();');
after(() => fs.rmSync(temp, { recursive: true, force: true }));
function run(post, extra = {}) {
  fs.writeFileSync(path.join(temp, 'fixture.json'), JSON.stringify({ post, ...extra }));
  const child = spawnSync(php, [path.relative(root, path.join(temp, 'harness.php'))], { cwd: root, encoding: 'utf8' });
  assert.ifError(child.error);
  assert.equal(child.status, 0, child.stderr);
  return JSON.parse(child.stdout);
}
const post = { id: '440', updated_value: '3.25', dept: 'B8' };
test('PHP endpoint syntax', () => {
  const child = spawnSync(php, ['-l', 'server/ajax/edit_price.php'], { cwd: root, encoding: 'utf8' });
  assert.ifError(child.error);
  assert.equal(child.status, 0, child.stderr);
  assert.match(child.stdout, /No syntax errors/);
});
test('saves price and aisle together without changing unrelated fields', () => {
  const result = run(post);
  assert.equal(result.status, 200);
  assert.equal(result.response.department, 'B8');
  assert.equal(result.response.department_updated, true);
  assert.deepEqual(result.row, { ID: 440, price: '3.25', department: 'B8', grocery_item: 'Milk', pantryqty: '2' });
  assert.equal(result.statements.length, 1);
});
for (const dept of [undefined, '', '  ']) {
  test(`keeps saved aisle for ${JSON.stringify(dept)} dept`, () => {
    const result = run({ ...post, dept });
    assert.equal(result.status, 200);
    assert.equal(result.row.department, 'A1');
    assert.equal(result.row.price, '3.25');
    assert.equal(result.response.department_updated, false);
  });
}
test('normalizes aisle and accepts zero price', () => {
  const result = run({ ...post, updated_value: '0', dept: ' b8 ' });
  assert.equal(result.status, 200);
  assert.equal(result.row.price, '0.00');
  assert.equal(result.row.department, 'B8');
});
test('unchanged existing row still succeeds', () => {
  const result = run({ ...post, updated_value: '1.00', dept: 'A1' });
  assert.equal(result.status, 200);
  assert.equal(result.response.affected_rows, 0);
});
test('unknown item is reported', () => {
  const result = run({ ...post, id: '999' });
  assert.equal(result.status, 404);
  assert.equal(result.row.price, '1.00');
});
for (const [label, overrides] of [
  ['missing price', { updated_value: undefined }],
  ['invalid ID', { id: '1 OR 1=1' }],
  ['zero ID', { id: '0' }],
  ['out of range ID', { id: '999999999999999999' }],
  ['array ID', { id: ['440'] }],
  ['blank price', { updated_value: '' }],
  ['invalid price', { updated_value: 'banana' }],
  ['negative price', { updated_value: '-1' }],
  ['array price', { updated_value: ['3.25'] }],
  ['out of range price', { updated_value: '9999999999999.99' }],
  ['category instead of aisle', { dept: 'Dairy' }],
  ['SQL in aisle', { dept: "B8'; DROP TABLE items;--" }],
  ['array aisle', { dept: ['B8'] }]
]) {
  test(`rejects ${label} before connecting`, () => {
    const result = run({ ...post, ...overrides });
    assert.equal(result.status, 400);
    assert.equal(result.response.ok, false);
    assert.equal(result.row, null);
    assert.deepEqual(result.statements, []);
  });
}
test('rejects non-POST requests', () => {
  assert.equal(run(post, { method: 'GET' }).status, 405);
});
test('database failure does not claim success or expose SQL', () => {
  const result = run(post, { fail: true });
  assert.equal(result.status, 500);
  assert.equal(result.response.ok, false);
  assert.equal(result.response.error, 'Database update failed');
  assert.equal(result.row.price, '1.00');
  assert.equal(result.row.department, 'A1');
});
