(function () {
  const extensionApi = globalThis.browser ?? globalThis.chrome;
  const MATCH_PATTERNS = ["https://rumble.com/chat*"];
  const MATCH_PREFIX = "https://rumble.com/chat";

  async function injectIntoTab(tabId) {
    try {
      await extensionApi.scripting.executeScript({
        target: { tabId },
        files: ["clock-core.js", "content.js"]
      });
    } catch {
    }
  }

  async function injectIntoMatchingTabs() {
    if (!extensionApi?.tabs?.query || !extensionApi?.scripting?.executeScript) {
      return;
    }

    try {
      const tabs = await extensionApi.tabs.query({ url: MATCH_PATTERNS });

      for (const tab of tabs) {
        if (typeof tab.id === "number") {
          await injectIntoTab(tab.id);
        }
      }
    } catch {
    }
  }

  function isMatchingUrl(url) {
    return typeof url === "string" && url.startsWith(MATCH_PREFIX);
  }

  extensionApi.runtime.onInstalled.addListener(() => {
    void injectIntoMatchingTabs();
  });

  extensionApi.runtime.onStartup.addListener(() => {
    void injectIntoMatchingTabs();
  });

  extensionApi.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
    const nextUrl = changeInfo.url ?? tab.url;

    if (
      (changeInfo.status === "complete" || typeof changeInfo.url === "string") &&
      isMatchingUrl(nextUrl)
    ) {
      void injectIntoTab(tabId);
    }
  });
})();
