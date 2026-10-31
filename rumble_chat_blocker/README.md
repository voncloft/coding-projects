# Rumble Chat Blocker

Firefox add-on for `https://rumble.com/chat` that adds a small block button beside each chat username.

## What it does

- Injects a `Block` button beside each `.js-user-tag.chat-history--username` entry on `rumble.com/chat`.
- Stores blocked usernames in the add-on's own local storage.
- Hides the blocked user's chat rows immediately without reloading the page.
- Generates equivalent uBlock Origin rules in this format:
- Adds a Firefox preferences page where you can edit blocked names, remove entries, and export filters.

```text
rumble.com##button.js-user-tag:has-text(name):upward(.chat-history--row)
```

## Important limitation

Firefox extensions cannot directly edit another extension's private storage, and uBlock Origin does not expose a public API for writing into `My filters`. This add-on therefore keeps its own block list and gives you a popup button to copy the equivalent uBO filters for manual paste.

## Load in Firefox

1. Open `about:debugging`.
2. Click `This Firefox`.
3. Click `Load Temporary Add-on`.
4. Select [manifest.json](/home/von/projects/rumble-chat-blocker/manifest.json).

## Preferences page

Open the add-on preferences from Firefox's add-on manager, or click `Preferences` in the popup. The preferences page lets you:

- Edit blocked usernames
- Remove entries
- Add names manually
- Save changes back to extension storage
- Copy the generated uBO filters

## Files

- [manifest.json](/home/von/projects/rumble-chat-blocker/manifest.json)
- [background.js](/home/von/projects/rumble-chat-blocker/background.js)
- [content-script.js](/home/von/projects/rumble-chat-blocker/content-script.js)
- [popup.html](/home/von/projects/rumble-chat-blocker/popup.html)
- [popup.js](/home/von/projects/rumble-chat-blocker/popup.js)
- [options.html](/home/von/projects/rumble-chat-blocker/options.html)
- [options.js](/home/von/projects/rumble-chat-blocker/options.js)
# rumble_chat_blocker
