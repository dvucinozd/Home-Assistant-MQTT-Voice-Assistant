#!/usr/bin/env python3
"""
Home Assistant MCP Server + local ESPHome YAML helpers.

This server exposes:
- Home Assistant REST tools (states, services)
- Home Assistant WebSocket tools (system_log/list)
- Optional local ESPHome YAML file helpers (read/write/list)

Secrets are provided via environment variables (recommended):
- HOME_ASSISTANT_BASE_URL  e.g. http://kucni.local:9000  (no trailing slash)
- HOME_ASSISTANT_TOKEN     long-lived access token
- HOME_ASSISTANT_VERIFY_SSL  1/0/true/false (default: true)

Run (inspector):
  mcp dev help_scripts/ha_mcp_server.py:server
"""

from __future__ import annotations

import json
import os
import re
import ssl
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import requests
import websocket
from mcp.server.fastmcp import FastMCP

server = FastMCP("home-assistant")


def _to_bool(value: object, default: bool) -> bool:
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    s = str(value).strip().lower()
    if s in ("1", "true", "yes", "y", "on"):
        return True
    if s in ("0", "false", "no", "n", "off"):
        return False
    return default


@dataclass(frozen=True)
class HAConfig:
    base_url: str
    token: str
    verify_ssl: bool

    @property
    def ws_url(self) -> str:
        # Convert http(s)://host[:port] -> ws(s)://host[:port]/api/websocket
        if self.base_url.startswith("https://"):
            return "wss://" + self.base_url.split("://", 1)[1].rstrip("/") + "/api/websocket"
        return "ws://" + self.base_url.split("://", 1)[1].rstrip("/") + "/api/websocket"


def _load_ha_config() -> HAConfig:
    base_url = (os.environ.get("HOME_ASSISTANT_BASE_URL") or os.environ.get("HA_BASE_URL") or "").strip().rstrip("/")
    token = (os.environ.get("HOME_ASSISTANT_TOKEN") or os.environ.get("HA_TOKEN") or "").strip()
    verify_ssl = _to_bool(
        os.environ.get("HOME_ASSISTANT_VERIFY_SSL") or os.environ.get("HA_VERIFY_SSL"),
        default=True,
    )

    if not base_url or not token:
        raise RuntimeError(
            "Missing HA credentials. Set HOME_ASSISTANT_BASE_URL and HOME_ASSISTANT_TOKEN."
        )
    return HAConfig(base_url=base_url, token=token, verify_ssl=verify_ssl)


def _ha_headers(cfg: HAConfig) -> dict[str, str]:
    return {"Authorization": f"Bearer {cfg.token}", "Content-Type": "application/json"}


def _ha_rest_get(path: str) -> Any:
    cfg = _load_ha_config()
    url = cfg.base_url + path
    resp = requests.get(url, headers=_ha_headers(cfg), timeout=20, verify=cfg.verify_ssl)
    resp.raise_for_status()
    return resp.json()


def _ha_rest_post(path: str, payload: dict[str, Any]) -> Any:
    cfg = _load_ha_config()
    url = cfg.base_url + path
    resp = requests.post(
        url,
        headers=_ha_headers(cfg),
        data=json.dumps(payload),
        timeout=20,
        verify=cfg.verify_ssl,
    )
    resp.raise_for_status()
    if not resp.text:
        return None
    try:
        return resp.json()
    except Exception:
        return resp.text


def _ha_ws_call(cmd: dict[str, Any]) -> dict[str, Any]:
    cfg = _load_ha_config()

    sslopt = None
    if cfg.ws_url.startswith("wss://") and not cfg.verify_ssl:
        sslopt = {"cert_reqs": ssl.CERT_NONE, "check_hostname": False}

    ws = websocket.create_connection(cfg.ws_url, timeout=20, sslopt=sslopt)
    try:
        _ = ws.recv()  # auth_required
        ws.send(json.dumps({"type": "auth", "access_token": cfg.token}))
        auth = json.loads(ws.recv())
        if auth.get("type") != "auth_ok":
            raise RuntimeError(f"HA WS auth failed: {auth.get('type')}")

        ws.send(json.dumps(cmd))
        return json.loads(ws.recv())
    finally:
        ws.close()


@server.tool()
def home_assistant_get_state(entity_id: str) -> dict[str, Any]:
    """Get Home Assistant state for an entity_id (REST /api/states/<entity_id>)."""
    if not entity_id or "." not in entity_id:
        raise ValueError("entity_id must look like 'domain.object_id'")
    return _ha_rest_get(f"/api/states/{entity_id}")


@server.tool()
def home_assistant_list_states(
    domain: str | None = None,
    search: str | None = None,
    limit: int = 200,
    include_attributes: bool = False,
) -> list[dict[str, Any]]:
    """
    List Home Assistant states (REST /api/states) with optional filtering.

    domain: e.g. 'sensor', 'switch'
    search: substring match on entity_id (case-insensitive)
    limit: max results returned (default 200)
    include_attributes: include full attributes dict (can be large)
    """
    if limit <= 0:
        return []
    states = _ha_rest_get("/api/states") or []

    domain_l = (domain or "").strip().lower()
    search_l = (search or "").strip().lower()

    out: list[dict[str, Any]] = []
    for st in states:
        eid = str(st.get("entity_id", ""))
        if domain_l and not eid.startswith(domain_l + "."):
            continue
        if search_l and search_l not in eid.lower():
            continue
        item = {
            "entity_id": eid,
            "state": st.get("state"),
        }
        if include_attributes:
            item["attributes"] = st.get("attributes", {})
        out.append(item)
        if len(out) >= limit:
            break
    return out


@server.tool()
def home_assistant_call_service(
    domain: str,
    service: str,
    entity_id: str | None = None,
    service_data: dict[str, Any] | None = None,
) -> Any:
    """
    Call a Home Assistant service (REST /api/services/<domain>/<service>).

    Provide entity_id and/or service_data. Returns HA response JSON.
    """
    d = (domain or "").strip()
    s = (service or "").strip()
    if not d or not s:
        raise ValueError("domain and service are required")

    payload: dict[str, Any] = {}
    if entity_id:
        payload["entity_id"] = entity_id
    if service_data:
        payload.update(service_data)
    return _ha_rest_post(f"/api/services/{d}/{s}", payload)


@server.tool()
def home_assistant_system_log_list(
    limit: int = 200,
    search: str | None = None,
    levels: list[str] | None = None,
) -> list[dict[str, Any]]:
    """
    Fetch HA system logs via WebSocket (system_log/list).

    limit: max entries returned (default 200)
    search: substring match over name/message/source (case-insensitive)
    levels: optional list like ['error','warning','info']
    """
    if limit <= 0:
        return []
    resp = _ha_ws_call({"id": 1, "type": "system_log/list"})
    if not resp.get("success"):
        raise RuntimeError(f"system_log/list failed: {resp}")

    logs = resp.get("result") or []
    search_l = (search or "").strip().lower()
    levels_l = {lvl.strip().lower() for lvl in (levels or []) if lvl and lvl.strip()}

    out: list[dict[str, Any]] = []
    for entry in logs:
        if levels_l:
            lvl = str(entry.get("level", "")).lower()
            if lvl not in levels_l:
                continue
        if search_l:
            blob = " ".join(
                str(entry.get(k, "") or "") for k in ("name", "message", "source")
            ).lower()
            if search_l not in blob:
                continue
        out.append(entry)
        if len(out) >= limit:
            break
    return out


def _resolve_esphome_dir(esphome_dir: str | None) -> Path:
    base = (esphome_dir or os.environ.get("ESPHOME_DIR") or "").strip()
    if not base:
        raise RuntimeError(
            "Missing ESPHome directory. Set ESPHOME_DIR or pass esphome_dir."
        )
    p = Path(base).expanduser().resolve()
    if not p.exists() or not p.is_dir():
        raise RuntimeError(f"ESPHOME_DIR does not exist or is not a dir: {p}")
    return p


def _safe_yaml_path(root: Path, filename: str) -> Path:
    name = (filename or "").strip()
    if not name:
        raise ValueError("filename is required")
    if any(sep in name for sep in ("..", "/", "\\", ":", "\0")):
        raise ValueError("filename must be a simple file name (no paths)")
    if not (name.endswith(".yaml") or name.endswith(".yml")):
        raise ValueError("filename must end with .yaml or .yml")
    p = (root / name).resolve()
    if root not in p.parents:
        raise ValueError("path traversal detected")
    return p


@server.tool()
def esphome_list_yaml(esphome_dir: str | None = None) -> list[str]:
    """List YAML files in ESPHome config directory (local filesystem)."""
    root = _resolve_esphome_dir(esphome_dir)
    out = []
    for p in sorted(root.glob("*.y*ml")):
        if p.is_file():
            out.append(p.name)
    return out


@server.tool()
def esphome_read_yaml(filename: str, esphome_dir: str | None = None) -> str:
    """Read an ESPHome YAML config file (local filesystem)."""
    root = _resolve_esphome_dir(esphome_dir)
    p = _safe_yaml_path(root, filename)
    return p.read_text(encoding="utf-8", errors="replace")


@server.tool()
def esphome_write_yaml(
    filename: str,
    content: str,
    esphome_dir: str | None = None,
    create: bool = False,
) -> dict[str, Any]:
    """
    Write an ESPHome YAML config file (local filesystem).

    create=false refuses to create new files.
    """
    root = _resolve_esphome_dir(esphome_dir)
    p = _safe_yaml_path(root, filename)
    if p.exists() and not p.is_file():
        raise RuntimeError(f"Path exists but is not a file: {p}")
    if not p.exists() and not create:
        raise RuntimeError(f"File does not exist (create=false): {p.name}")
    p.write_text(content or "", encoding="utf-8")
    return {"ok": True, "path": str(p), "bytes": p.stat().st_size}


@server.tool()
def esphome_run_cli(
    args: list[str],
    esphome_dir: str | None = None,
) -> dict[str, Any]:
    """
    Run local `esphome` CLI with arguments in the ESPHome directory.

    Example args: ["config", "my_node.yaml"] or ["compile", "my_node.yaml"].
    """
    if not args:
        raise ValueError("args is required")
    root = _resolve_esphome_dir(esphome_dir)
    cmd = ["esphome", *args]
    proc = subprocess.run(
        cmd,
        cwd=str(root),
        capture_output=True,
        text=True,
        timeout=60 * 20,
    )
    return {
        "returncode": proc.returncode,
        "stdout": proc.stdout[-20000:],
        "stderr": proc.stderr[-20000:],
    }


if __name__ == "__main__":
    server.run()

