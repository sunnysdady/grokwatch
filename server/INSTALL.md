# Voice to-do plugin (gw_todo)
1. Copy `gw_todo.py` to `xiaozhi-server/plugins_func/functions/`.
2. In `core/http_server.py`, after the vision routes add:
   `from plugins_func.functions.gw_todo import add_todo_routes; add_todo_routes(app)`
3. Add `add_todo` to `Intent.*.functions` in `data/.config.yaml`, then restart the server.
4. Store: `data/gw_todo.json` (text only, no audio). Override with env `GW_TODO_FILE`.
