"""GrokWatch voice to-do plugin for xiaozhi-esp32-server.

Voice: "记一下…" / "提醒我…" -> LLM calls add_todo(text). Only the ASR text is stored
(no audio). Storage: a local JSON file (data/gw_todo.json). No secrets, no cloud.
HTTP for the watch (same port as OTA, LAN only):
  GET  /gw/todo            -> {"rev":N,"items":[{"id":..,"text":..,"done":false,"ts":..}]}
  POST /gw/todo/done?id=ID -> marks done, returns the list
"""
import json, os, re, threading, time, uuid
from aiohttp import web
from plugins_func.register import register_function, ToolType, ActionResponse, Action

STORE = os.environ.get("GW_TODO_FILE", os.path.join("data", "gw_todo.json"))
MAX_OPEN = 50
_lock = threading.Lock()


def _load():
    try:
        with open(STORE, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {"rev": 0, "items": []}


def _save(d):
    d["rev"] = d.get("rev", 0) + 1
    os.makedirs(os.path.dirname(STORE) or ".", exist_ok=True)
    tmp = STORE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(d, f, ensure_ascii=False, indent=1)
    os.replace(tmp, STORE)  # atomic


def add_item(text):
    text = " ".join(str(text).split())
    text = re.sub(r"^(请)?(帮我)?(记一下|记下|记一个|记住|提醒我|加个待办|待办)[:：，, ]*", "", text).strip()[:120]
    if not text:
        return None
    with _lock:
        d = _load()
        it = {"id": uuid.uuid4().hex[:8], "text": text, "done": False, "ts": int(time.time())}
        d["items"].insert(0, it)
        opn = [i for i in d["items"] if not i["done"]][:MAX_OPEN]
        d["items"] = opn + [i for i in d["items"] if i["done"]][:20]
        _save(d)
        return it


def set_done(item_id, done=True):
    with _lock:
        d = _load()
        hit = False
        for i in d["items"]:
            if i["id"] == item_id:
                i["done"] = bool(done); hit = True
        if hit:
            _save(d)
        return d


def watch_view(limit=5):
    d = _load()
    return {"rev": d.get("rev", 0), "open": sum(1 for i in d["items"] if not i["done"]),
            "items": [i for i in d["items"] if not i["done"]][:limit]}


add_todo_desc = {
    "type": "function",
    "function": {
        "name": "add_todo",
        "description": "当用户说'记一下…'、'帮我记…'、'提醒我…'、'加个待办…'时调用，把要做的事保存为待办，会显示在手表的待办页。",
        "parameters": {"type": "object", "properties": {
            "text": {"type": "string", "description": "待办内容，简短中文，去掉'记一下''提醒我'等口头语"}},
            "required": ["text"]},
    },
}


@register_function("add_todo", add_todo_desc, ToolType.WAIT)
def add_todo(text=None):
    it = add_item(text or "")
    if not it:
        return ActionResponse(Action.RESPONSE, None, "没听清要记什么，再说一次？")
    n = watch_view()["open"]
    return ActionResponse(Action.RESPONSE, None, f"记好了：{it['text']}。现在有 {n} 条待办。")


async def _get(req):
    return web.json_response(watch_view(int(req.query.get("n", 5))))


async def _done(req):
    set_done(req.query.get("id", ""), req.query.get("v", "1") != "0")
    return web.json_response(watch_view(int(req.query.get("n", 5))))


def add_todo_routes(app):
    app.add_routes([web.get("/gw/todo", _get), web.post("/gw/todo/done", _done)])
