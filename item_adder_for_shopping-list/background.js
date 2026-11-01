"use strict";

(function () {
  const browserApi = typeof browser !== "undefined"
    ? browser
    : (typeof chrome !== "undefined" ? chrome : null);
  const SETTINGS_KEY = "serverSettings";
  const DEBUG_LOG_KEY = "debugLogs";
  const MAX_DEBUG_LOG_ENTRIES = 200;
  const OFF_PRODUCT_FIELDS = [
    "code",
    "product_name",
    "brands",
    "quantity",
    "nutrition_data_per",
    "serving_size",
    "serving_quantity",
    "nutriments",
    "product_quantity"
  ].join(",");

  if (!browserApi?.runtime?.onMessage) {
    return;
  }

  function getDefaultSettings() {
    return {
      serverBaseUrl: "http://voncloft.shopping.com",
      addEndpoint: "/scripts/add_food_to_db.php"
    };
  }

  function buildServerUrl(settings) {
    if (!settings?.serverBaseUrl || !settings?.addEndpoint) {
      throw new Error("Server URL and add endpoint are required.");
    }

    return new URL(settings.addEndpoint, settings.serverBaseUrl).toString();
  }

  async function getServerSettings() {
    const stored = await browserApi.storage.local.get({ [SETTINGS_KEY]: getDefaultSettings() });
    return stored[SETTINGS_KEY];
  }

  function normalizeText(value) {
    if (value === null || value === undefined) {
      return null;
    }

    const text = String(value).trim();
    return text === "" ? null : text;
  }

  function normalizeBarcode(value) {
    const digits = String(value || "").replace(/\D+/g, "");
    if (digits.length < 8 || digits.length > 14) {
      return null;
    }

    return digits;
  }

  function stripHtml(text) {
    return String(text || "")
      .replace(/<script[\s\S]*?<\/script>/gi, " ")
      .replace(/<style[\s\S]*?<\/style>/gi, " ")
      .replace(/<[^>]+>/g, " ")
      .replace(/\s+/g, " ")
      .trim();
  }

  function parseJson(text) {
    if (!text) {
      return null;
    }

    try {
      return JSON.parse(text);
    } catch (error) {
      return null;
    }
  }

  function sanitizeValue(value, depth = 0) {
    if (value === null || value === undefined) {
      return value;
    }

    if (depth > 4) {
      return "[max-depth]";
    }

    if (Array.isArray(value)) {
      return value.slice(0, 20).map((item) => sanitizeValue(item, depth + 1));
    }

    if (value instanceof Error) {
      return {
        name: value.name,
        message: value.message,
        stack: value.stack || null
      };
    }

    if (typeof value === "object") {
      const result = {};
      for (const [key, nested] of Object.entries(value)) {
        result[key] = sanitizeValue(nested, depth + 1);
      }
      return result;
    }

    if (typeof value === "function") {
      return "[function]";
    }

    return value;
  }

  function getSenderDetails(sender) {
    return {
      senderUrl: sender?.url || null,
      senderOrigin: sender?.origin || null,
      tabUrl: sender?.tab?.url || null,
      tabId: typeof sender?.tab?.id === "number" ? sender.tab.id : null
    };
  }

  async function appendDebugLog(type, details) {
    try {
      const stored = await browserApi.storage.local.get({ [DEBUG_LOG_KEY]: [] });
      const logs = Array.isArray(stored[DEBUG_LOG_KEY]) ? stored[DEBUG_LOG_KEY] : [];
      const nextEntry = {
        timestamp: new Date().toISOString(),
        type,
        details: sanitizeValue(details)
      };
      const trimmed = logs.concat(nextEntry).slice(-MAX_DEBUG_LOG_ENTRIES);
      await browserApi.storage.local.set({ [DEBUG_LOG_KEY]: trimmed });
    } catch (error) {
      console.error("Failed to write debug log", error);
    }
  }

  function formatDebugLogEntry(entry, index) {
    return [
      `#${index + 1} ${entry.timestamp} ${entry.type}`,
      JSON.stringify(entry.details || {}, null, 2)
    ].join("\n");
  }

  async function getDebugLogExport() {
    const stored = await browserApi.storage.local.get({ [DEBUG_LOG_KEY]: [] });
    const entries = Array.isArray(stored[DEBUG_LOG_KEY]) ? stored[DEBUG_LOG_KEY] : [];
    const header = [
      "walmart_food_import_helper debug log",
      `generated_at: ${new Date().toISOString()}`,
      `extension_version: ${browserApi.runtime.getManifest().version}`,
      `user_agent: ${typeof navigator !== "undefined" ? navigator.userAgent : "unknown"}`,
      `entry_count: ${entries.length}`,
      ""
    ];
    const body = entries.length
      ? entries.map((entry, index) => formatDebugLogEntry(entry, index)).join("\n\n")
      : "No log entries recorded.";

    return {
      filename: "walmart_food_import_helper-debug-log.txt",
      text: `${header.join("\n")}${body}`
    };
  }

  async function clearDebugLogs() {
    await browserApi.storage.local.set({ [DEBUG_LOG_KEY]: [] });
  }

  async function submitFoodInsert(form, settingsOverride, context) {
    const settings = settingsOverride || await getServerSettings();
    const targetUrl = buildServerUrl(settings);
    const body = new URLSearchParams();
    const startedAt = Date.now();

    Object.entries(form || {}).forEach(([key, value]) => {
      body.set(key, value);
    });
    body.set("format", "json");

    await appendDebugLog("submit:start", {
      context,
      targetUrl,
      body: body.toString()
    });

    try {
      const response = await fetch(targetUrl, {
        method: "POST",
        headers: {
          "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8",
          "Accept": "application/json"
        },
        body: body.toString(),
        credentials: "include"
      });

      const responseText = await response.text();
      const json = parseJson(responseText);

      if (!response.ok) {
        throw new Error(json?.error || stripHtml(responseText) || `Server responded ${response.status}.`);
      }

      if (json && json.ok === false) {
        throw new Error(json.error || "Server returned ok=false.");
      }

      let result = null;
      if (json && json.ok) {
        result = json;
      } else if (/Imported Ingredient to main food database/i.test(responseText)) {
        result = { ok: true };
      } else {
        throw new Error(stripHtml(responseText) || "Server response did not confirm the insert.");
      }

      await appendDebugLog("submit:success", {
        context,
        targetUrl,
        elapsedMs: Date.now() - startedAt,
        responseStatus: response.status,
        responsePreview: responseText.slice(0, 240)
      });

      return result;
    } catch (error) {
      await appendDebugLog("submit:error", {
        context,
        targetUrl,
        elapsedMs: Date.now() - startedAt,
        error
      });
      throw error;
    }
  }

  async function fetchOpenFoodFactsJson(url, logPrefix, context) {
    const startedAt = Date.now();

    await appendDebugLog(`${logPrefix}:start`, {
      context,
      targetUrl: url
    });

    try {
      const response = await fetch(url, {
        method: "GET"
      });

      const text = await response.text();
      if (!response.ok) {
        throw new Error(`Open Food Facts responded ${response.status}.`);
      }

      const json = parseJson(text);
      if (!json) {
        throw new Error("Open Food Facts returned invalid JSON.");
      }

      await appendDebugLog(`${logPrefix}:success`, {
        context,
        targetUrl: url,
        elapsedMs: Date.now() - startedAt,
        responseStatus: response.status,
        responsePreview: text.slice(0, 240)
      });

      return json;
    } catch (error) {
      await appendDebugLog(`${logPrefix}:error`, {
        context,
        targetUrl: url,
        elapsedMs: Date.now() - startedAt,
        error
      });
      throw error;
    }
  }

  async function fetchOpenFoodFactsProduct(barcode, context) {
    const cleanBarcode = normalizeBarcode(barcode);
    if (!cleanBarcode) {
      return null;
    }

    const url = new URL(`https://world.openfoodfacts.org/api/v2/product/${cleanBarcode}`);
    url.searchParams.set("fields", OFF_PRODUCT_FIELDS);
    return fetchOpenFoodFactsJson(url.toString(), "off:product", {
      ...context,
      barcode: cleanBarcode
    });
  }

  async function searchOpenFoodFacts(title, context) {
    const cleanTitle = normalizeText(title);
    if (!cleanTitle) {
      return null;
    }

    const url = new URL("https://world.openfoodfacts.org/cgi/search.pl");
    url.searchParams.set("action", "process");
    url.searchParams.set("search_terms", cleanTitle);
    url.searchParams.set("search_simple", "1");
    url.searchParams.set("json", "1");
    url.searchParams.set("page_size", "8");
    url.searchParams.set("sort_by", "unique_scans_n");
    url.searchParams.set("fields", OFF_PRODUCT_FIELDS);

    return fetchOpenFoodFactsJson(url.toString(), "off:search", {
      ...context,
      searchTitle: cleanTitle
    });
  }

  function buildOk(payload) {
    return { ok: true, ...payload };
  }

  function buildError(error) {
    return {
      ok: false,
      error: error instanceof Error ? error.message : String(error)
    };
  }

  browserApi.runtime.onMessage.addListener((message, sender) => {
    const context = sanitizeValue({
      ...getSenderDetails(sender),
      ...(message?.context || {})
    });

    if (message?.type === "submitFoodInsert") {
      return submitFoodInsert(message.form, message.settings, context)
        .then((data) => buildOk({ data }))
        .catch((error) => buildError(error));
    }

    if (message?.type === "fetchOpenFoodFactsProduct") {
      return fetchOpenFoodFactsProduct(message.barcode, context)
        .then((json) => buildOk({ json }))
        .catch((error) => buildError(error));
    }

    if (message?.type === "searchOpenFoodFacts") {
      return searchOpenFoodFacts(message.title, context)
        .then((json) => buildOk({ json }))
        .catch((error) => buildError(error));
    }

    if (message?.type === "getDebugLogExport") {
      return getDebugLogExport()
        .then((data) => buildOk(data))
        .catch((error) => buildError(error));
    }

    if (message?.type === "clearDebugLogs") {
      return clearDebugLogs()
        .then(() => buildOk({ cleared: true }))
        .catch((error) => buildError(error));
    }

    return undefined;
  });
})();
