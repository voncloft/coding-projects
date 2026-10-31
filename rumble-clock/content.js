(function () {
  const ROOT_ID = "rumble-chat-clock-addon";
  const extensionApi = globalThis.browser ?? globalThis.chrome;
  const {
    DEFAULT_SETTINGS,
    SETTINGS_KEYS,
    createClockModel,
    formatClockModel,
    applyClockStyles
  } = globalThis.RumbleChatClockCore;

  if (document.getElementById(ROOT_ID)) {
    return;
  }

  const clock = document.createElement("div");
  clock.id = ROOT_ID;
  clock.setAttribute("aria-label", "Current date and time");

  Object.assign(clock.style, {
    position: "fixed",
    top: "12px",
    right: "12px",
    zIndex: "2147483647",
    padding: "8px 12px",
    borderRadius: "8px",
    fontFamily: "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace",
    fontSize: "14px",
    lineHeight: "1.2",
    fontWeight: "600",
    letterSpacing: "0.02em",
    boxShadow: "0 4px 18px rgba(0, 0, 0, 0.28)",
    pointerEvents: "none",
    userSelect: "none",
    whiteSpace: "nowrap"
  });

  let clockModel = createClockModel(DEFAULT_SETTINGS);
  let lastRendered = null;

  function applySettings(nextSettings) {
    clockModel = createClockModel({
      ...clockModel.settings,
      ...nextSettings
    });
    applyClockStyles(clock, clockModel.settings);
    lastRendered = null;
    render();
  }

  function render() {
    const nextValue = formatClockModel(clockModel, new Date());

    if (nextValue !== lastRendered) {
      clock.textContent = nextValue;
      clock.style.display = nextValue ? "block" : "none";
      lastRendered = nextValue;
    }

    if (!clock.isConnected) {
      document.documentElement.appendChild(clock);
    }
  }

  async function loadSettings() {
    if (!extensionApi?.storage?.local) {
      applySettings(DEFAULT_SETTINGS);
      return;
    }

    try {
      const stored = await extensionApi.storage.local.get(DEFAULT_SETTINGS);
      applySettings(stored);
    } catch {
      applySettings(DEFAULT_SETTINGS);
    }
  }

  applyClockStyles(clock, clockModel.settings);
  render();
  window.setInterval(render, 250);

  if (extensionApi?.storage?.onChanged) {
    extensionApi.storage.onChanged.addListener((changes, areaName) => {
      if (areaName !== "local") {
        return;
      }

      const nextSettings = {};

      for (const key of SETTINGS_KEYS) {
        if (changes[key]) {
          nextSettings[key] = changes[key].newValue;
        }
      }

      if (Object.keys(nextSettings).length > 0) {
        applySettings(nextSettings);
      }
    });
  }

  loadSettings();
})();
