# Voice to-do plugin (gw_todo)
1. Copy `gw_todo.py` to `xiaozhi-server/plugins_func/functions/`.
2. In `core/http_server.py`, after the vision routes add:
   `from plugins_func.functions.gw_todo import add_todo_routes; add_todo_routes(app)`
3. Add `add_todo` to `Intent.*.functions` in `data/.config.yaml`, then restart the server.
4. Store: `data/gw_todo.json` (text only, no audio). Override with env `GW_TODO_FILE`.

`http-todo-routes.patch` contains the route integration for the reviewed server
layout. Check it against your existing server before applying; preserve existing
Grok model and bridge settings. Back up both modified server files first.

Verify with the server virtualenv and its root on `PYTHONPATH`:
`python test_gw_todo.py -q`. Tests use temporary storage; they do not add live tasks.
After restart, `GET /gw/todo?n=5` should return HTTP 200 with `rev`, `open`, and
`items`. Invalid `n` values return 400 before any data changes.

These routes follow the existing trusted-LAN, single-user design and have no
per-user authentication. Do not expose them through a public proxy. The watch
refreshes when the user opens the To-do page; installation does not switch pages.
