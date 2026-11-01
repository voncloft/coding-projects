"use strict";

const browserApi = typeof browser !== "undefined"
  ? browser
  : (typeof chrome !== "undefined" ? chrome : null);
const SETTINGS_KEY = "serverSettings";
const LAST_DEPARTMENT_KEY = "lastDepartment";

const elements = {
  status: document.getElementById("status"),
  item: document.getElementById("item"),
  price: document.getElementById("price"),
  itemId: document.getElementById("itemId"),
  dept: document.getElementById("dept"),
  servings: document.getElementById("servings"),
  calories: document.getElementById("calories"),
  protein: document.getElementById("protein"),
  fat: document.getElementById("fat"),
  carbs: document.getElementById("carbs"),
  fiber: document.getElementById("fiber"),
  url: document.getElementById("url"),
  version: document.getElementById("version"),
  serverSummary: document.getElementById("serverSummary"),
  settings: document.getElementById("settings"),
  refresh: document.getElementById("refresh"),
  saveCurrent: document.getElementById("saveCurrent")
};

let currentPageData = null;

if (!browserApi) {
  throw new Error("Browser extension API is not available.");
}

elements.version.textContent = `v${browserApi.runtime.getManifest().version}`;

function getDefaultSettings() {
  return {
    serverBaseUrl: "http://voncloft.shopping.com",
    addEndpoint: "/scripts/add_food_to_db.php"
  };
}

function setStatus(message, tone) {
  elements.status.textContent = message;
  elements.status.className = "status";
  if (tone) {
    elements.status.classList.add(tone);
  }
}

function buildServerUrl(settings) {
  if (!settings.serverBaseUrl || !settings.addEndpoint) {
    throw new Error("Server URL and add endpoint are required.");
  }

  return new URL(settings.addEndpoint, settings.serverBaseUrl).toString();
}

function summarizeServer(settings) {
  try {
    return `POST -> ${buildServerUrl(settings)}`;
  } catch (error) {
    return "Server not configured";
  }
}

async function getStoredData() {
  return browserApi.storage.local.get({
    [SETTINGS_KEY]: getDefaultSettings(),
    [LAST_DEPARTMENT_KEY]: ""
  });
}

async function renderServerSummary() {
  const stored = await getStoredData();
  elements.serverSummary.textContent = summarizeServer(stored[SETTINGS_KEY]);
}

async function getActiveTab() {
  const tabs = await browserApi.tabs.query({ active: true, currentWindow: true });
  return tabs[0] || null;
}

async function fetchCurrentPageData() {
  const tab = await getActiveTab();
  if (!tab || !tab.id) {
    throw new Error("No active tab found.");
  }

  if (!tab.url || !tab.url.includes("walmart.com")) {
    throw new Error("Open a Walmart product page first.");
  }

  try {
    const data = await browserApi.tabs.sendMessage(tab.id, { type: "getWalmartItemData" });
    if (!data || !data.ok) {
      throw new Error("Could not read Walmart page data. Reload the tab and try again.");
    }
    return data;
  } catch (error) {
    await browserApi.tabs.executeScript(tab.id, { file: "content-script.js" });
  }

  const data = await browserApi.tabs.sendMessage(tab.id, { type: "getWalmartItemData" });
  if (!data || !data.ok) {
    throw new Error("Could not read Walmart page data. Reload the tab and try again.");
  }

  return data;
}

function setFieldValue(element, value) {
  element.value = value || "";
}

function applyDetectedData(data, lastDepartment) {
  const nutrition = data.nutrition || {};

  currentPageData = data;
  setFieldValue(elements.item, data.title);
  setFieldValue(elements.price, data.price);
  setFieldValue(elements.itemId, data.itemId);
  setFieldValue(elements.dept, data.department || lastDepartment);
  setFieldValue(elements.servings, nutrition.servings);
  setFieldValue(elements.calories, nutrition.calories);
  setFieldValue(elements.protein, nutrition.protein);
  setFieldValue(elements.fat, nutrition.fat);
  setFieldValue(elements.carbs, nutrition.carbs);
  setFieldValue(elements.fiber, nutrition.fiber);
  setFieldValue(elements.url, data.url);
}

function normalizeOptionalNumber(value, label, decimalPlaces) {
  const raw = String(value || "").trim();
  if (raw === "") {
    return "";
  }

  const match = raw.replace(/,/g, "").match(/-?\d+(?:\.\d+)?/);
  if (!match) {
    throw new Error(`${label} must be numeric.`);
  }

  const number = Number(match[0]);
  if (!Number.isFinite(number)) {
    throw new Error(`${label} must be numeric.`);
  }

  return typeof decimalPlaces === "number" ? number.toFixed(decimalPlaces) : String(number);
}

function readForm() {
  const item = String(elements.item.value || "").trim();
  const url = String(elements.url.value || currentPageData?.url || "").trim();

  if (!item) {
    throw new Error("Item name is required.");
  }

  const price = normalizeOptionalNumber(elements.price.value, "Price", 2);
  if (!price) {
    throw new Error("Price is required.");
  }

  return {
    item,
    price,
    dept: String(elements.dept.value || "").trim(),
    servings: normalizeOptionalNumber(elements.servings.value, "Servings"),
    protein: normalizeOptionalNumber(elements.protein.value, "Protein"),
    fat: normalizeOptionalNumber(elements.fat.value, "Fat"),
    carbs: normalizeOptionalNumber(elements.carbs.value, "Carbohydrates"),
    calories: normalizeOptionalNumber(elements.calories.value, "Calories"),
    fiber: normalizeOptionalNumber(elements.fiber.value, "Fiber"),
    url
  };
}

async function submitFoodInsert(form, settings) {
  const response = await browserApi.runtime.sendMessage({
    type: "submitFoodInsert",
    form,
    settings,
    context: {
      source: "popup",
      pageUrl: currentPageData?.url || null,
      itemId: currentPageData?.itemId || null,
      item: form.item,
      price: form.price,
      dept: form.dept
    }
  });

  if (!response || response.ok !== true) {
    throw new Error(response?.error || "Server insert failed.");
  }

  return response.data;
}

async function refreshCurrentPage() {
  setStatus("Reading current Walmart tab...");

  try {
    const stored = await getStoredData();
    const data = await fetchCurrentPageData();
    applyDetectedData(data, stored[LAST_DEPARTMENT_KEY]);

    if (data.price) {
      setStatus(`Detected price ${data.price} from ${data.source}.`, "good");
    } else {
      setStatus("Loaded the Walmart page, but no price was detected. You can enter it manually.");
    }
  } catch (error) {
    currentPageData = null;
    setStatus(error.message || String(error), "bad");
  }
}

async function saveCurrentItem() {
  setStatus("Submitting item to the server...");

  try {
    const stored = await getStoredData();
    const form = readForm();
    const result = await submitFoodInsert(form, stored[SETTINGS_KEY]);

    if (form.dept) {
      await browserApi.storage.local.set({
        [LAST_DEPARTMENT_KEY]: form.dept
      });
    }

    await renderServerSummary();

    const insertedId = result?.insert_id ? ` as ID ${result.insert_id}` : "";
    setStatus(`Added ${form.item}${insertedId}.`, "good");
  } catch (error) {
    setStatus(error.message || String(error), "bad");
  }
}

function openSettingsPage() {
  browserApi.runtime.openOptionsPage();
}

elements.settings.addEventListener("click", openSettingsPage);
elements.refresh.addEventListener("click", refreshCurrentPage);
elements.saveCurrent.addEventListener("click", saveCurrentItem);

renderServerSummary().then(refreshCurrentPage);
