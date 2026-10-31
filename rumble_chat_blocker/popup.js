const STORAGE_KEY = "blockedUsers";

function buildFilter(username) {
  return `rumble.com##button.js-user-tag:has-text(${username}):upward(.chat-history--row)`;
}

function normalizeUsername(username) {
  return username.trim().toLowerCase();
}

async function getBlockedUsers() {
  const stored = await browser.storage.local.get(STORAGE_KEY);
  const list = Array.isArray(stored[STORAGE_KEY]) ? stored[STORAGE_KEY] : [];
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
  await browser.storage.local.set({ [STORAGE_KEY]: usernames });
}

async function removeUser(username) {
  const users = await getBlockedUsers();
  const nextUsers = users.filter((candidate) => normalizeUsername(candidate) !== normalizeUsername(username));
  await setBlockedUsers(nextUsers);
  await render();
}

async function copyFilters() {
  const users = await getBlockedUsers();
  const filters = users.map(buildFilter).join("\n");
  await navigator.clipboard.writeText(filters);
}

function createUserRow(username) {
  const item = document.createElement("li");
  item.className = "user-row";

  const label = document.createElement("span");
  label.textContent = username;

  const removeButton = document.createElement("button");
  removeButton.type = "button";
  removeButton.textContent = "Remove";
  removeButton.addEventListener("click", () => {
    removeUser(username).catch((error) => console.error("Failed to remove user:", error));
  });

  item.append(label, removeButton);
  return item;
}

async function render() {
  const users = await getBlockedUsers();
  const list = document.getElementById("blocked-users");
  const emptyState = document.getElementById("empty-state");

  list.replaceChildren(...users.map(createUserRow));
  emptyState.hidden = users.length > 0;
}

document.getElementById("copy-filters").addEventListener("click", () => {
  copyFilters().catch((error) => console.error("Failed to copy filters:", error));
});

document.getElementById("open-options").addEventListener("click", () => {
  browser.runtime.openOptionsPage().catch((error) => {
    console.error("Failed to open preferences:", error);
  });
});

render().catch((error) => console.error("Failed to render popup:", error));
