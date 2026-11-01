# Walmart Food Import Helper

Load this folder's `manifest.json` as a temporary Firefox extension.

The popup reads the current Walmart page, pre-fills the same fields used by
`php/add_foods.php`, and POSTs them to `scripts/add_food_to_db.php`.

Default server settings:

- Base URL: `http://voncloft.shopping.com`
- Endpoint: `/scripts/add_food_to_db.php`

The extension submits `format=json` so the PHP script can return a clean success
or error response without changing the normal browser form workflow.
# item_adder_for_shopping-list
