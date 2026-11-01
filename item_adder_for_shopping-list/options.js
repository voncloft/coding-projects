"use strict";

const browserApi = typeof browser !== "undefined"
  ? browser
  : (typeof chrome !== "undefined" ? chrome : null);
const SETTINGS_KEY = "serverSettings";

const elements = {
  version: document.getElementById("version"),
  status: document.getElementById("status"),
  serverBaseUrl: document.getElementById("serverBaseUrl"),
  addEndpoint: document.getElementById("addEndpoint"),
  saveSettings: document.getElementById("saveSettings"),
  resetSettings: document.getElementById("resetSettings"),
  downloadLogs: document.getElementById("downloadLogs"),
  clearLogs: document.getElementById("clearLogs")
};

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

function readForm() {
  return {
    serverBaseUrl: elements.serverBaseUrl.value.trim(),
    addEndpoint: elements.addEndpoint.value.trim()
  };
}

function writeForm(settings) {
  elements.serverBaseUrl.value = settings.serverBaseUrl || "";
  elements.addEndpoint.value = settings.addEndpoint || "";
}

function buildServerUrl(settings) {
  if (!settings.serverBaseUrl || !settings.addEndpoint) {
    throw new Error("Server URL and add endpoint are required.");
  }

  return new URL(settings.addEndpoint, settings.serverBaseUrl).toString();
}

async function sendRuntimeMessage(message) {
  const response = await browserApi.runtime.sendMessage(message);
  if (!response || response.ok !== true) {
    throw new Error(response?.error || "Background request failed.");
  }
  return response;
}

async function saveSettings() {
  const settings = readForm();
  const fullUrl = buildServerUrl(settings);
  await browserApi.storage.local.set({ [SETTINGS_KEY]: settings });
  setStatus(`Saved ${fullUrl}`, "good");
}

function resetSettings() {
  writeForm(getDefaultSettings());
  setStatus("Defaults restored in the form. Click Save Settings to apply them.", "good");
}

async function downloadLogs() {
  const response = await sendRuntimeMessage({
    type: "getDebugLogExport",
    context: {
      source: "options"
    }
  });
  const blob = new Blob([response.text], { type: "text/plain;charset=utf-8" });
  const objectUrl = URL.createObjectURL(blob);

  try {
    await browserApi.downloads.download({
      url: objectUrl,
      filename: response.filename,
      saveAs: true,
      conflictAction: "uniquify"
    });
  } finally {
    window.setTimeout(() => {
      URL.revokeObjectURL(objectUrl);
    }, 1000);
  }

  setStatus("Downloaded debug log.", "good");
}

async function clearLogs() {
  await sendRuntimeMessage({
    type: "clearDebugLogs",
    context: {
      source: "options"
    }
  });
  setStatus("Cleared debug logs.", "good");
}

async function init() {
  const stored = await browserApi.storage.local.get({ [SETTINGS_KEY]: getDefaultSettings() });
  writeForm(stored[SETTINGS_KEY]);
}

elements.saveSettings.addEventListener("click", () => {
  saveSettings().catch((error) => {
    setStatus(error.message || String(error), "bad");
  });
});

elements.resetSettings.addEventListener("click", resetSettings);

elements.downloadLogs.addEventListener("click", () => {
  downloadLogs().catch((error) => {
    setStatus(error.message || String(error), "bad");
  });
});

elements.clearLogs.addEventListener("click", () => {
  clearLogs().catch((error) => {
    setStatus(error.message || String(error), "bad");
  });
});

init().catch((error) => {
  setStatus(error.message || String(error), "bad");
});
