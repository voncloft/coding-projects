(function () {
  const extensionApi = globalThis.browser ?? globalThis.chrome;
  const {
    LOCAL_TIME_ZONE,
    CUSTOM_TIME_ZONE,
    DEFAULT_SETTINGS,
    hexToRgba,
    isValidTimeZone,
    normalizeSettings,
    hasVisibleOutput,
    createClockModel,
    formatClockModel,
    applyClockStyles,
    getTimeZoneDescription
  } = globalThis.RumbleChatClockCore;

  const timeZoneSelect = document.getElementById("time-zone-select");
  const customTimeZoneInput = document.getElementById("custom-time-zone");
  const showDateInput = document.getElementById("show-date-input");
  const weekdayStyleSelect = document.getElementById("weekday-style-select");
  const dateOrderSelect = document.getElementById("date-order-select");
  const monthStyleSelect = document.getElementById("month-style-select");
  const dayStyleSelect = document.getElementById("day-style-select");
  const yearStyleSelect = document.getElementById("year-style-select");
  const dateSeparatorInput = document.getElementById("date-separator-input");
  const weekdaySeparatorInput = document.getElementById("weekday-separator-input");
  const dateTimeSeparatorInput = document.getElementById("date-time-separator-input");
  const showTimeInput = document.getElementById("show-time-input");
  const hourCycleSelect = document.getElementById("hour-cycle-select");
  const hourStyleSelect = document.getElementById("hour-style-select");
  const minuteStyleSelect = document.getElementById("minute-style-select");
  const secondStyleSelect = document.getElementById("second-style-select");
  const meridiemStyleSelect = document.getElementById("meridiem-style-select");
  const timeZoneNameStyleSelect = document.getElementById("time-zone-name-style-select");
  const timeSeparatorInput = document.getElementById("time-separator-input");
  const timeZoneLabelSeparatorInput = document.getElementById("time-zone-label-separator-input");
  const showSecondsInput = document.getElementById("show-seconds-input");
  const textColorInput = document.getElementById("text-color-input");
  const backgroundColorInput = document.getElementById("background-color-input");
  const backgroundOpacityInput = document.getElementById("background-opacity-input");
  const backgroundOpacityValue = document.getElementById("background-opacity-value");
  const dateControls = document.getElementById("date-controls");
  const timeControls = document.getElementById("time-controls");
  const previewValue = document.getElementById("preview-value");
  const previewZone = document.getElementById("preview-zone");
  const previewPanel = document.querySelector(".preview");
  const previewLabel = document.querySelector(".preview-label");
  const saveButton = document.getElementById("save-button");
  const resetButton = document.getElementById("reset-button");
  const statusMessage = document.getElementById("status-message");

  function getSelectedTimeZone() {
    return timeZoneSelect.value === CUSTOM_TIME_ZONE
      ? customTimeZoneInput.value.trim()
      : timeZoneSelect.value;
  }

  function readDraftSettings() {
    return {
      timeZone: getSelectedTimeZone(),
      showDate: showDateInput.checked,
      weekdayStyle: weekdayStyleSelect.value,
      weekdaySeparator: weekdaySeparatorInput.value,
      dateOrder: dateOrderSelect.value,
      dateSeparator: dateSeparatorInput.value,
      monthStyle: monthStyleSelect.value,
      dayStyle: dayStyleSelect.value,
      yearStyle: yearStyleSelect.value,
      showTime: showTimeInput.checked,
      hourCycle: hourCycleSelect.value,
      hourStyle: hourStyleSelect.value,
      minuteStyle: minuteStyleSelect.value,
      showSeconds: showSecondsInput.checked,
      secondStyle: secondStyleSelect.value,
      timeSeparator: timeSeparatorInput.value,
      meridiemStyle: meridiemStyleSelect.value,
      dateTimeSeparator: dateTimeSeparatorInput.value,
      timeZoneNameStyle: timeZoneNameStyleSelect.value,
      timeZoneLabelSeparator: timeZoneLabelSeparatorInput.value,
      textColor: textColorInput.value,
      backgroundColor: backgroundColorInput.value,
      backgroundOpacity: backgroundOpacityInput.value
    };
  }

  function setStatus(message, state = "") {
    statusMessage.textContent = message;
    statusMessage.dataset.state = state;
  }

  function syncCustomField() {
    customTimeZoneInput.disabled = timeZoneSelect.value !== CUSTOM_TIME_ZONE;
  }

  function syncOpacityLabel() {
    backgroundOpacityValue.value = `${backgroundOpacityInput.value}%`;
    backgroundOpacityValue.textContent = `${backgroundOpacityInput.value}%`;
  }

  function syncDependentControls() {
    syncCustomField();
    syncOpacityLabel();

    dateControls.disabled = !showDateInput.checked;
    timeControls.disabled = !showTimeInput.checked;
    secondStyleSelect.disabled = !showTimeInput.checked || !showSecondsInput.checked;
    meridiemStyleSelect.disabled = !showTimeInput.checked || hourCycleSelect.value !== "12";
    dateTimeSeparatorInput.disabled = !showDateInput.checked || !showTimeInput.checked;
    weekdaySeparatorInput.disabled = !showDateInput.checked || weekdayStyleSelect.value === "off";
    timeZoneNameStyleSelect.disabled = !showTimeInput.checked;
    timeZoneLabelSeparatorInput.disabled = (
      !showTimeInput.checked || timeZoneNameStyleSelect.value === "off"
    );
  }

  function applyPreviewStyle(settings) {
    applyClockStyles(previewPanel, settings);
    previewLabel.style.color = hexToRgba(settings.textColor, 72);
    previewZone.style.color = hexToRgba(settings.textColor, 76);
  }

  function renderPreview() {
    syncDependentControls();

    const draftSettings = readDraftSettings();
    const normalizedSettings = normalizeSettings(draftSettings);
    applyPreviewStyle(normalizedSettings);

    if (!draftSettings.timeZone) {
      previewValue.textContent = "--";
      previewZone.textContent = "Enter an IANA timezone like America/Chicago.";
      return;
    }

    if (!isValidTimeZone(draftSettings.timeZone)) {
      previewValue.textContent = "--";
      previewZone.textContent = "Invalid timezone. Use an IANA name like America/New_York.";
      return;
    }

    if (!hasVisibleOutput(normalizedSettings)) {
      previewValue.textContent = "Nothing selected";
      previewZone.textContent = "Enable date or time to display something on the page.";
      return;
    }

    previewValue.textContent = formatClockModel(
      createClockModel(normalizedSettings),
      new Date()
    );
    previewZone.textContent = getTimeZoneDescription(normalizedSettings.timeZone);
  }

  function applySettingsToForm(rawSettings) {
    const settings = normalizeSettings(rawSettings);
    const presetValues = Array.from(timeZoneSelect.options).map((option) => option.value);

    if (presetValues.includes(settings.timeZone)) {
      timeZoneSelect.value = settings.timeZone;
      customTimeZoneInput.value = "";
    } else {
      timeZoneSelect.value = CUSTOM_TIME_ZONE;
      customTimeZoneInput.value = settings.timeZone;
    }

    showDateInput.checked = settings.showDate;
    weekdayStyleSelect.value = settings.weekdayStyle;
    dateOrderSelect.value = settings.dateOrder;
    monthStyleSelect.value = settings.monthStyle;
    dayStyleSelect.value = settings.dayStyle;
    yearStyleSelect.value = settings.yearStyle;
    dateSeparatorInput.value = settings.dateSeparator;
    weekdaySeparatorInput.value = settings.weekdaySeparator;
    dateTimeSeparatorInput.value = settings.dateTimeSeparator;
    showTimeInput.checked = settings.showTime;
    hourCycleSelect.value = settings.hourCycle;
    hourStyleSelect.value = settings.hourStyle;
    minuteStyleSelect.value = settings.minuteStyle;
    showSecondsInput.checked = settings.showSeconds;
    secondStyleSelect.value = settings.secondStyle;
    timeSeparatorInput.value = settings.timeSeparator;
    meridiemStyleSelect.value = settings.meridiemStyle;
    timeZoneNameStyleSelect.value = settings.timeZoneNameStyle;
    timeZoneLabelSeparatorInput.value = settings.timeZoneLabelSeparator;
    textColorInput.value = settings.textColor;
    backgroundColorInput.value = settings.backgroundColor;
    backgroundOpacityInput.value = String(settings.backgroundOpacity);
    syncDependentControls();
  }

  async function loadSettings() {
    if (!extensionApi?.storage?.local) {
      applySettingsToForm(DEFAULT_SETTINGS);
      renderPreview();
      setStatus("Storage is unavailable. Using defaults.", "error");
      return;
    }

    try {
      const stored = await extensionApi.storage.local.get(DEFAULT_SETTINGS);
      applySettingsToForm(stored);
    } catch {
      applySettingsToForm(DEFAULT_SETTINGS);
      setStatus("Could not load saved settings. Using defaults.", "error");
    }

    renderPreview();
  }

  async function saveSettings() {
    if (!extensionApi?.storage?.local) {
      setStatus("Save failed. Firefox storage is unavailable.", "error");
      return;
    }

    const draftSettings = readDraftSettings();

    if (!isValidTimeZone(draftSettings.timeZone)) {
      setStatus("Save failed. Enter a valid IANA timezone.", "error");
      renderPreview();
      return;
    }

    const normalizedSettings = normalizeSettings(draftSettings);

    try {
      await extensionApi.storage.local.set(normalizedSettings);
      applySettingsToForm(normalizedSettings);
      renderPreview();
      setStatus("Saved.");
    } catch {
      setStatus("Save failed. Firefox could not write the setting.", "error");
    }
  }

  async function resetDefaults() {
    applySettingsToForm(DEFAULT_SETTINGS);
    renderPreview();

    if (!extensionApi?.storage?.local) {
      setStatus("Defaults restored locally. Storage is unavailable.", "error");
      return;
    }

    try {
      await extensionApi.storage.local.set(DEFAULT_SETTINGS);
      setStatus("Defaults restored.");
    } catch {
      setStatus("Defaults loaded, but Firefox could not save them.", "error");
    }
  }

  timeZoneSelect.addEventListener("change", () => {
    setStatus("");
    renderPreview();
  });

  customTimeZoneInput.addEventListener("input", () => {
    setStatus("");
    renderPreview();
  });

  for (const element of [
    showDateInput,
    weekdayStyleSelect,
    dateOrderSelect,
    monthStyleSelect,
    dayStyleSelect,
    yearStyleSelect,
    dateSeparatorInput,
    weekdaySeparatorInput,
    dateTimeSeparatorInput,
    showTimeInput,
    hourCycleSelect,
    hourStyleSelect,
    minuteStyleSelect,
    showSecondsInput,
    secondStyleSelect,
    timeSeparatorInput,
    meridiemStyleSelect,
    timeZoneNameStyleSelect,
    timeZoneLabelSeparatorInput,
    textColorInput,
    backgroundColorInput,
    backgroundOpacityInput
  ]) {
    element.addEventListener("input", () => {
      setStatus("");
      renderPreview();
    });
  }

  saveButton.addEventListener("click", saveSettings);
  resetButton.addEventListener("click", resetDefaults);

  window.setInterval(renderPreview, 250);
  loadSettings();
})();
