"""Run with the server virtualenv and server root on PYTHONPATH."""
import json
import tempfile
import unittest
from pathlib import Path

from aiohttp import web
from aiohttp.test_utils import TestClient, TestServer

import gw_todo as todo


class TodoTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.old_store = todo.STORE
        todo.STORE = str(Path(self.temp.name) / "todo.json")
        app = web.Application()
        todo.add_todo_routes(app)
        self.client = TestClient(TestServer(app))
        await self.client.start_server()

    async def asyncTearDown(self):
        await self.client.close()
        todo.STORE = self.old_store
        self.temp.cleanup()

    async def test_empty_read_does_not_create_store(self):
        response = await self.client.get("/gw/todo")
        self.assertEqual(response.status, 200)
        self.assertEqual(await response.json(), {"rev": 0, "open": 0, "items": []})
        self.assertFalse(Path(todo.STORE).exists())

    async def test_voice_create_complete_and_reopen(self):
        result = todo.add_todo("记一下：隔离测试")
        self.assertIn("记好了", result.response)
        view = await (await self.client.get("/gw/todo")).json()
        item = view["items"][0]
        self.assertEqual(item["text"], "隔离测试")
        response = await self.client.post("/gw/todo/done", params={"id": item["id"]})
        self.assertEqual((await response.json())["open"], 0)
        response = await self.client.post("/gw/todo/done", params={"id": item["id"], "v": "0"})
        self.assertEqual((await response.json())["open"], 1)
        self.assertEqual(json.loads(Path(todo.STORE).read_text())["rev"], 3)

    async def test_invalid_limit_never_mutates(self):
        item = todo.add_item("unchanged")
        before = Path(todo.STORE).read_bytes()
        for n in ("bad", "0", "-1", "51"):
            self.assertEqual((await self.client.get("/gw/todo", params={"n": n})).status, 400)
            response = await self.client.post("/gw/todo/done", params={"id": item["id"], "n": n})
            self.assertEqual(response.status, 400)
        self.assertEqual(Path(todo.STORE).read_bytes(), before)

    async def test_capacity_and_unknown_id(self):
        for i in range(55):
            todo.add_item(str(i))
        view = await (await self.client.get("/gw/todo?n=50")).json()
        self.assertEqual(view["open"], 50)
        self.assertEqual(len(view["items"]), 50)
        before = Path(todo.STORE).read_bytes()
        await self.client.post("/gw/todo/done?id=missing")
        self.assertEqual(Path(todo.STORE).read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
