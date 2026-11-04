"""Authoring 通道：调用已配置的 Godot MCP（@coding-solo/godot-mcp，stdio JSON-RPC）。

用途（仅 authoring）：项目信息、启动**唯一工作编辑器**、按需运行/停止、场景/节点编辑工具列表。
验证（截图/断言）一律走非编辑器 CLI（tools/verify/run_all.py），不经过本脚本。

用法：
  python -X utf8 tools/authoring/godot_mcp.py info <projectPath>
  python -X utf8 tools/authoring/godot_mcp.py launch <projectPath>
  python -X utf8 tools/authoring/godot_mcp.py run <projectPath> [scene]
  python -X utf8 tools/authoring/godot_mcp.py stop
  python -X utf8 tools/authoring/godot_mcp.py tools
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SERVER = Path(
    r"D:\app_finish\Godot\.mcp-server\node_modules\@coding-solo\godot-mcp\build\index.js"
)
GODOT = r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64.exe"


class McpClient:
    """一次性 MCP stdio 会话（JSON-RPC，按行分隔）。"""

    def __init__(self, server: Path, godot: str) -> None:
        env = dict(os.environ)
        env["GODOT_PATH"] = godot
        node = shutil.which("node")
        if not node:
            raise SystemExit("未找到 node（Godot MCP 需要 Node.js）")
        self.proc = subprocess.Popen(
            [node, str(server)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        self._id = 0
        self._request(
            "initialize",
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "csgo_demo-authoring", "version": "1"},
            },
        )
        self._send({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def _send(self, payload: dict) -> None:
        assert self.proc.stdin is not None
        self.proc.stdin.write(json.dumps(payload) + "\n")
        self.proc.stdin.flush()

    def _request(self, method: str, params: dict | None = None) -> dict:
        self._id += 1
        request_id = self._id
        self._send(
            {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": method,
                "params": params or {},
            }
        )
        return self._read_until(request_id)

    def _read_until(self, target_id: int) -> dict:
        assert self.proc.stdout is not None
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise SystemExit("MCP server 提前退出")
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                continue
            if message.get("id") == target_id:
                return message

    def call(self, name: str, arguments: dict | None = None) -> dict:
        return self._request("tools/call", {"name": name, "arguments": arguments or {}})

    def tools(self) -> dict:
        return self._request("tools/list")

    def close(self) -> None:
        try:
            self.proc.terminate()
        except OSError:
            pass


def project_path(args) -> str:
    return str(Path(args[1]).resolve() if len(args) > 1 else (ROOT / "host").resolve())


def print_tools(client: McpClient) -> int:
    for tool in client.tools().get("result", {}).get("tools", []):
        print(f"- {tool['name']}: {tool.get('description', '')[:100]}")
    return 0


def print_info(client: McpClient, args) -> int:
    print(
        json.dumps(
            client.call("get_project_info", {"projectPath": project_path(args)}),
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


def launch_editor(client: McpClient, args) -> int:
    print(
        json.dumps(
            client.call("launch_editor", {"projectPath": project_path(args)}),
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


def run_project(client: McpClient, args) -> int:
    payload = {"projectPath": project_path(args)}
    if len(args) > 2:
        payload["scene"] = args[2]
    print(json.dumps(client.call("run_project", payload), ensure_ascii=False, indent=2))
    return 0


def stop_project(client: McpClient, args) -> int:
    print(json.dumps(client.call("stop_project", {}), ensure_ascii=False, indent=2))
    return 0


ACTIONS = {
    "tools": print_tools,
    "info": print_info,
    "launch": launch_editor,
    "run": run_project,
    "stop": stop_project,
}


def main() -> int:
    args = sys.argv[1:]
    if not args or args[0] not in ACTIONS:
        print(__doc__)
        return 2
    client = McpClient(SERVER, GODOT)
    try:
        return ACTIONS[args[0]](client, args)
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
