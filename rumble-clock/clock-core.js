(function () {
  const LOCAL_TIME_ZONE = "LOCAL";
  const CUSTOM_TIME_ZONE = "__custom__";
  const DEFAULT_SETTINGS = {
    timeZone: LOCAL_TIME_ZONE,
    showDate: true,
    weekdayStyle: "off",
    weekdaySeparator: ", ",
    dateOrder: "MDY",
    dateSeparator: "/",
    monthStyle: "2-digit",
    dayStyle: "2-digit",
    yearStyle: "numeric",
    showTime: true,
    hourCycle: "24",
    hourStyle: "2-digit",
    minuteStyle: "2-digit",
    showSeconds: true,
    secondStyle: "2-digit",
    timeSeparator: ":",
    meridiemStyle: "upper",
    dateTimeSeparator: " ",
    timeZoneNameStyle: "off",
    timeZoneLabelSeparator: " ",
    textColor: "#ffffff",
    backgroundColor: "#000000",
    backgroundOpacity: 78
  };

  const VALUE_SETS = {
    weekdayStyle: new Set(["off", "short", "long", "narrow"]),
    dateOrder: new Set(["MDY", "DMY", "YMD", "YDM", "MYD", "DYM"]),
    monthStyle: new Set(["off", "numeric", "2-digit", "short", "long", "narrow"]),
    dayStyle: new Set(["off", "numeric", "2-digit"]),
    yearStyle: new Set(["off", "numeric", "2-digit"]),
    hourCycle: new Set(["12", "24"]),
    hourStyle: new Set(["numeric", "2-digit"]),
    minuteStyle: new Set(["numeric", "2-digit"]),
    secondStyle: new Set(["numeric", "2-digit"]),
    meridiemStyle: new Set(["upper", "lower", "dots", "off"]),
    timeZoneNameStyle: new Set([
      "off",
      "short",
      "long",
      "shortOffset",
      "longOffset",
      "shortGeneric",
      "longGeneric"
    ])
  };

  const HEX_COLOR_PATTERN = /^#[0-9a-f]{6}$/i;
  const SETTINGS_KEYS = Object.keys(DEFAULT_SETTINGS);

  function normalizeTextOption(value, allowedValues, fallback) {
    return allowedValues.has(value) ? value : fallback;
  }

  function normalizeBoolean(value, fallback) {
    return typeof value === "boolean" ? value : fallback;
  }

  function normalizeColor(value, fallback) {
    return HEX_COLOR_PATTERN.test(value) ? value : fallback;
  }

  function normalizeOpacity(value, fallback) {
    const numericValue = Number(value);

    if (!Number.isFinite(numericValue)) {
      return fallback;
    }

    return Math.max(0, Math.min(100, Math.round(numericValue)));
  }

  function normalizeSeparator(value, fallback) {
    if (typeof value !== "string") {
      return fallback;
    }

    return value.replace(/[\r\n\t]+/g, " ").slice(0, 32);
  }

  function isValidTimeZone(value) {
    if (!value) {
      return false;
    }

    if (value === LOCAL_TIME_ZONE) {
      return true;
    }

    try {
      new Intl.DateTimeFormat("en-US", { timeZone: value }).format(new Date());
      return true;
    } catch {
      return false;
    }
  }

  function normalizeSettings(rawSettings) {
    const settings = rawSettings ?? {};

    return {
      timeZone: isValidTimeZone(settings.timeZone)
        ? settings.timeZone
        : DEFAULT_SETTINGS.timeZone,
      showDate: normalizeBoolean(settings.showDate, DEFAULT_SETTINGS.showDate),
      weekdayStyle: normalizeTextOption(
        settings.weekdayStyle,
        VALUE_SETS.weekdayStyle,
        DEFAULT_SETTINGS.weekdayStyle
      ),
      weekdaySeparator: normalizeSeparator(
        settings.weekdaySeparator,
        DEFAULT_SETTINGS.weekdaySeparator
      ),
      dateOrder: normalizeTextOption(
        settings.dateOrder,
        VALUE_SETS.dateOrder,
        DEFAULT_SETTINGS.dateOrder
      ),
      dateSeparator: normalizeSeparator(
        settings.dateSeparator,
        DEFAULT_SETTINGS.dateSeparator
      ),
      monthStyle: normalizeTextOption(
        settings.monthStyle,
        VALUE_SETS.monthStyle,
        DEFAULT_SETTINGS.monthStyle
      ),
      dayStyle: normalizeTextOption(
        settings.dayStyle,
        VALUE_SETS.dayStyle,
        DEFAULT_SETTINGS.dayStyle
      ),
      yearStyle: normalizeTextOption(
        settings.yearStyle,
        VALUE_SETS.yearStyle,
        DEFAULT_SETTINGS.yearStyle
      ),
      showTime: normalizeBoolean(settings.showTime, DEFAULT_SETTINGS.showTime),
      hourCycle: normalizeTextOption(
        settings.hourCycle,
        VALUE_SETS.hourCycle,
        DEFAULT_SETTINGS.hourCycle
      ),
      hourStyle: normalizeTextOption(
        settings.hourStyle,
        VALUE_SETS.hourStyle,
        DEFAULT_SETTINGS.hourStyle
      ),
      minuteStyle: normalizeTextOption(
        settings.minuteStyle,
        VALUE_SETS.minuteStyle,
        DEFAULT_SETTINGS.minuteStyle
      ),
      showSeconds: normalizeBoolean(
        settings.showSeconds,
        DEFAULT_SETTINGS.showSeconds
      ),
      secondStyle: normalizeTextOption(
        settings.secondStyle,
        VALUE_SETS.secondStyle,
        DEFAULT_SETTINGS.secondStyle
      ),
      timeSeparator: normalizeSeparator(
        settings.timeSeparator,
        DEFAULT_SETTINGS.timeSeparator
      ),
      meridiemStyle: normalizeTextOption(
        settings.meridiemStyle,
        VALUE_SETS.meridiemStyle,
        DEFAULT_SETTINGS.meridiemStyle
      ),
      dateTimeSeparator: normalizeSeparator(
        settings.dateTimeSeparator,
        DEFAULT_SETTINGS.dateTimeSeparator
      ),
      timeZoneNameStyle: normalizeTextOption(
        settings.timeZoneNameStyle,
        VALUE_SETS.timeZoneNameStyle,
        DEFAULT_SETTINGS.timeZoneNameStyle
      ),
      timeZoneLabelSeparator: normalizeSeparator(
        settings.timeZoneLabelSeparator,
        DEFAULT_SETTINGS.timeZoneLabelSeparator
      ),
      textColor: normalizeColor(settings.textColor, DEFAULT_SETTINGS.textColor),
      backgroundColor: normalizeColor(
        settings.backgroundColor,
        DEFAULT_SETTINGS.backgroundColor
      ),
      backgroundOpacity: normalizeOpacity(
        settings.backgroundOpacity,
        DEFAULT_SETTINGS.backgroundOpacity
      )
    };
  }

  function hasDateContent(settings) {
    return settings.showDate && (
      settings.weekdayStyle !== "off" ||
      settings.monthStyle !== "off" ||
      settings.dayStyle !== "off" ||
      settings.yearStyle !== "off"
    );
  }

  function hasTimeContent(settings) {
    return settings.showTime;
  }

  function hasVisibleOutput(settings) {
    const normalizedSettings = normalizeSettings(settings);
    return hasDateContent(normalizedSettings) || hasTimeContent(normalizedSettings);
  }

  function createFormatter(settings) {
    if (!hasDateContent(settings) && !hasTimeContent(settings)) {
      return null;
    }

    const options = {};

    if (settings.showDate) {
      if (settings.weekdayStyle !== "off") {
        options.weekday = settings.weekdayStyle;
      }

      if (settings.monthStyle !== "off") {
        options.month = settings.monthStyle;
      }

      if (settings.dayStyle !== "off") {
        options.day = settings.dayStyle;
      }

      if (settings.yearStyle !== "off") {
        options.year = settings.yearStyle;
      }
    }

    if (settings.showTime) {
      options.hour = settings.hourStyle;
      options.minute = settings.minuteStyle;
      options.hourCycle = settings.hourCycle === "12" ? "h12" : "h23";

      if (settings.showSeconds) {
        options.second = settings.secondStyle;
      }

      if (settings.timeZoneNameStyle !== "off") {
        options.timeZoneName = settings.timeZoneNameStyle;
      }
    }

    if (settings.timeZone !== LOCAL_TIME_ZONE) {
      options.timeZone = settings.timeZone;
    }

    return new Intl.DateTimeFormat("en-US", options);
  }

  function createClockModel(rawSettings) {
    const settings = normalizeSettings(rawSettings);
    return {
      settings,
      formatter: createFormatter(settings)
    };
  }

  function partsToValueMap(parts) {
    const values = {};

    for (const part of parts) {
      if (part.type === "literal" || values[part.type] !== undefined) {
        continue;
      }

      values[part.type] = part.value;
    }

    return values;
  }

  function formatDayPeriod(dayPeriod, style) {
    const normalized = dayPeriod.replace(/\./g, "").toLowerCase();

    if (style === "off") {
      return "";
    }

    if (style === "lower") {
      return normalized;
    }

    if (style === "dots") {
      if (normalized === "am") {
        return "a.m.";
      }

      if (normalized === "pm") {
        return "p.m.";
      }

      return `${normalized}.`;
    }

    return normalized.toUpperCase();
  }

  function buildDateString(values, settings) {
    if (!hasDateContent(settings)) {
      return "";
    }

    const tokenMap = {
      M: settings.monthStyle === "off" ? "" : values.month,
      D: settings.dayStyle === "off" ? "" : values.day,
      Y: settings.yearStyle === "off" ? "" : values.year
    };

    const dateTokens = [];

    for (const token of settings.dateOrder) {
      if (tokenMap[token]) {
        dateTokens.push(tokenMap[token]);
      }
    }

    const dateValue = dateTokens.join(settings.dateSeparator);

    if (settings.weekdayStyle !== "off" && values.weekday) {
      return dateValue
        ? `${values.weekday}${settings.weekdaySeparator}${dateValue}`
        : values.weekday;
    }

    return dateValue;
  }

  function buildTimeString(values, settings) {
    if (!hasTimeContent(settings)) {
      return "";
    }

    const timeTokens = [values.hour, values.minute];

    if (settings.showSeconds && values.second) {
      timeTokens.push(values.second);
    }

    let timeValue = timeTokens.filter(Boolean).join(settings.timeSeparator);

    if (settings.hourCycle === "12" && settings.meridiemStyle !== "off" && values.dayPeriod) {
      timeValue += ` ${formatDayPeriod(values.dayPeriod, settings.meridiemStyle)}`;
    }

    if (settings.timeZoneNameStyle !== "off" && values.timeZoneName) {
      timeValue += `${settings.timeZoneLabelSeparator}${values.timeZoneName}`;
    }

    return timeValue;
  }

  function formatClockModel(model, date = new Date()) {
    if (!model.formatter) {
      return "";
    }

    const values = partsToValueMap(model.formatter.formatToParts(date));
    const dateValue = buildDateString(values, model.settings);
    const timeValue = buildTimeString(values, model.settings);

    if (dateValue && timeValue) {
      return `${dateValue}${model.settings.dateTimeSeparator}${timeValue}`;
    }

    return dateValue || timeValue;
  }

  function formatClockValue(rawSettings, date = new Date()) {
    return formatClockModel(createClockModel(rawSettings), date);
  }

  function hexToRgba(hexColor, opacityPercent) {
    const normalizedHex = normalizeColor(hexColor, DEFAULT_SETTINGS.backgroundColor);
    const alpha = normalizeOpacity(opacityPercent, DEFAULT_SETTINGS.backgroundOpacity) / 100;
    const red = Number.parseInt(normalizedHex.slice(1, 3), 16);
    const green = Number.parseInt(normalizedHex.slice(3, 5), 16);
    const blue = Number.parseInt(normalizedHex.slice(5, 7), 16);

    return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
  }

  function applyClockStyles(element, rawSettings) {
    const settings = normalizeSettings(rawSettings);
    element.style.color = settings.textColor;
    element.style.backgroundColor = hexToRgba(
      settings.backgroundColor,
      settings.backgroundOpacity
    );
    element.style.border = `1px solid ${hexToRgba(settings.textColor, 16)}`;
  }

  function getTimeZoneDescription(value) {
    if (value === LOCAL_TIME_ZONE) {
      return `Using browser local time (${Intl.DateTimeFormat().resolvedOptions().timeZone || "system default"})`;
    }

    return value;
  }

  globalThis.RumbleChatClockCore = {
    LOCAL_TIME_ZONE,
    CUSTOM_TIME_ZONE,
    DEFAULT_SETTINGS,
    SETTINGS_KEYS,
    hexToRgba,
    isValidTimeZone,
    normalizeSettings,
    hasVisibleOutput,
    createClockModel,
    formatClockModel,
    formatClockValue,
    applyClockStyles,
    getTimeZoneDescription
  };
})();
