# Walmart price and aisle updater

Firefox extension, version 1.9. Updates an existing shopping-list item's price
and aisle from the current Walmart product page. It uses the same aisle formats
and product fields as the food importer; aisles are stored in `items.department`.

## Server setup

Copy `server/ajax/edit_price.php` to `ajax/edit_price.php` on the shopping website,
replacing the old price-only endpoint. It uses the site's existing
`include/passwords.php` connection and needs no database schema changes.
This repository does not contain database credentials.

The endpoint accepts `id`, `updated_value` (price), and optional `dept` (aisle).
It updates price and aisle together with a prepared statement. A missing or
blank aisle preserves the existing department. Price-only clients still work.
Invalid fields, unknown item IDs, and database failures return JSON errors.

An old endpoint can still save the price while silently ignoring `dept`. The
extension detects a missing aisle acknowledgement and reports that the server
file needs updating instead of claiming the aisle was saved.

## Load in Firefox

1. Open `about:debugging#/runtime/this-firefox`.
2. Choose **Load Temporary Add-on** and select this folder's `manifest.json`.
3. After an update, reload the addon and reload any open Walmart tabs.

The popup displays the version. Run `python3 build-extension.py` to build
`walmart_price_updater-1.9.xpi` in this folder. Firefox's normal signing
requirements still apply. No addon is installed by building it.

## Use

1. Open a Walmart link from the shopping website with `#id=440` or `#dbid=440`.
   The addon remembers the database ID even if Walmart removes the URL fragment.
2. Select the intended Walmart store and wait for its price and aisle to load.
3. Click **Edit Price / Aisle** on the page, inspect the detected values, then
   click **Update Server**. **Refresh** reads the page again after a store change.
4. Alternatively, open the extension popup and click **Save Current Item**.
   Enable automatic submission in Settings for this popup action to update the
   server; otherwise it only remembers the product's database ID.

The detected aisle is shown in both interfaces. If Walmart does not provide an
aisle, only the price is updated and the saved aisle is kept. Category names,
store IDs, and ZIP codes are not substituted for an aisle. The on-page button
always sends an explicit update regardless of the popup's auto-submit setting.

## Tests

Run `node --test tests/update-flow.test.cjs` for the extension's extraction,
popup, panel, and request/response checks with mocked browser APIs and no network
requests. Run `node --test tests/server.test.cjs` with PHP CLI available for the
endpoint tests; `PHP_BIN` may specify another PHP-compatible CLI. Server tests
use a temporary fake database and never connect to the shopping website.
