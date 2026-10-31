const STORAGE_KEY = "blockedUsers";

function normalizeUsername(username) {
  return username.trim().toLowerCase();
}

function buildFilter(username) {
  return `rumble.com##button.js-user-tag:has-text(${username}):upward(.chat-history--row)`;
}

async function getBlockedUsers() {
  const stored = await browser.storage.local.get(STORAGE_KEY);
  const list = Array.isArray(stored[STORAGE_KEY]) ? stored[STORAGE_KEY] : [];
  return cleanUsernames(list);
}

function cleanUsernames(list) {
  const deduped = new Map();

  for (const username of list) {
    const cleanName = typeof username === "string" ? username.trim() : "";
    if (!cleanName) {
      continue;
    }

    deduped.set(normalizeUsername(cleanName), cleanName);
  }

  return Array.from(deduped.values()).sort((left, right) => left.localeCompare(right));
}

async function setBlockedUsers(usernames) {
  await browser.storage.local.set({ [STORAGE_KEY]: cleanUsernames(usernames) });
}

function getRowsContainer() {
  return document.getElementById("rows");
}

function getEmptyState() {
  return document.getElementById("empty-state");
}

function getStatusNode() {
  return document.getElementById("status");
}

function setStatus(message) {
  const status = getStatusNode();

  if (!message) {
    status.hidden = true;
    status.textContent = "";
    return;
  }

  status.hidden = false;
  status.textContent = message;
}

function toggleEmptyState() {
  const isEmpty = getRowsContainer().children.length === 0;
  getEmptyState().hidden = !isEmpty;
}

function createRow(username = "") {
  const row = document.createElement("div");
  row.className = "row";

  const input = document.createElement("input");
  input.type = "text";
  input.value = username;
  input.placeholder = "Rumble username";
  input.autocomplete = "off";
  input.spellcheck = false;

  const removeButton = document.createElement("button");
  removeButton.type = "button";
  removeButton.className = "danger-button";
  removeButton.textContent = "Remove";
  removeButton.addEventListener("click", () => {
    row.remove();
    toggleEmptyState();
    setStatus("Entry removed. Click Save Changes to persist it.");
  });

  row.append(input, removeButton);
  return row;
}

function getNamesFromRows() {
  return Array.from(getRowsContainer().querySelectorAll("input")).map((input) => input.value);
}

function renderRows(usernames) {
  const rows = usernames.map((username) => createRow(username));
  getRowsContainer().replaceChildren(...rows);
  toggleEmptyState();
}

async function saveChanges() {
  const usernames = getNamesFromRows();
  const cleaned = cleanUsernames(usernames);
  await setBlockedUsers(cleaned);
  renderRows(cleaned);
  setStatus(`Saved ${cleaned.length} blocked ${cleaned.length === 1 ? "name" : "names"}.`);
}

async function copyFilters() {
  const usernames = cleanUsernames(getNamesFromRows());
  const filters = usernames.map(buildFilter).join("\n");
  await navigator.clipboard.writeText(filters);
  setStatus(`Copied ${usernames.length} uBO ${usernames.length === 1 ? "filter" : "filters"}.`);
}

async function addNameRow() {
  const row = createRow("");
  getRowsContainer().appendChild(row);
  toggleEmptyState();
  row.querySelector("input")?.focus();
  setStatus("New entry added. Click Save Changes when you're done.");
}

async function init() {
  const usernames = await getBlockedUsers();
  renderRows(usernames);
}

document.getElementById("add-name").addEventListener("click", () => {
  addNameRow().catch((error) => console.error("Failed to add row:", error));
});

document.getElementById("save-changes").addEventListener("click", () => {
  saveChanges().catch((error) => console.error("Failed to save preferences:", error));
});

document.getElementById("copy-filters").addEventListener("click", () => {
  copyFilters().catch((error) => console.error("Failed to copy filters:", error));
});

browser.storage.onChanged.addListener((changes, areaName) => {
  if (areaName !== "local" || !changes[STORAGE_KEY]) {
    return;
  }

  getBlockedUsers()
    .then(renderRows)
    .catch((error) => console.error("Failed to refresh options state:", error));
});

init().catch((error) => console.error("Failed to initialize preferences:", error));
