/**
 * blender-viewer —— Blender MCP 桥的本地可视化台（零依赖 Node）。
 *
 * 作用：把 dsh-blender-plugin Gateway 的活会话变成一个能看的网页 —— 实时视口画面、
 * 就绪状态、写租约、调用计数，外加一个可以直接跑 Python / 打回退点 / 取场景信息的控制条。
 * 它只做本机 127.0.0.1 代理，不新开 Blender 连接、不改插件文件；真正的协议转换仍在 mcp_bridge.mjs。
 *
 * 启动：
 *   node tools/blender/viewer/server.mjs            # 默认 http://127.0.0.1:8765
 *   $env:BLENDER_VIEWER_PORT=8790; node ...
 *
 * 环境变量：
 *   DSH_BLENDER_GATEWAY  Gateway 基址，默认 http://127.0.0.1:39877
 *   BLENDER_VIEWER_PORT  监听端口，默认 8765
 */
import { createServer } from 'node:http';
import process from 'node:process';

const GATEWAY = (process.env.DSH_BLENDER_GATEWAY || 'http://127.0.0.1:39877').replace(/\/+$/, '');
const PORT = Number(process.env.BLENDER_VIEWER_PORT || 8765);
const HOST = '127.0.0.1';
const LOG_LIMIT = 60;

/** 环形日志，页面右下角展示。 */
const logRing = [];

/** 记录一条操作日志。 */
function pushLog(kind, text, ok = true) {
  logRing.unshift({ kind, text: String(text).slice(0, 400), ok, at: new Date().toLocaleTimeString('zh-CN') });
  if (logRing.length > LOG_LIMIT) logRing.pop();
}

/** 代理 Gateway 的 JSON 接口（默认匿名，避免误触会话归属门）。 */
async function gwJson(path, { method = 'GET', body, timeoutMs = 60000 } = {}) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(GATEWAY + path, {
      method,
      headers: body === undefined ? undefined : { 'content-type': 'application/json' },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal: ctrl.signal,
    });
    const text = await res.text();
    /** 流式回执（每行一条 JSON）取最后一条。 */
    try {
      return { status: res.status, data: JSON.parse(text) };
    } catch {
      const lines = text.trim().split('\n').filter(Boolean);
      const last = lines.map((l) => {
        try {
          return JSON.parse(l);
        } catch {
          return null;
        }
      }).filter(Boolean).pop();
      return { status: res.status, data: last || { ok: false, error: 'non_json_response' } };
    }
  } finally {
    clearTimeout(timer);
  }
}

/** 读请求体并解析 JSON。 */
function readJson(req) {
  return new Promise((resolve) => {
    let raw = '';
    req.on('data', (c) => {
      raw += c;
      if (raw.length > 2_000_000) req.destroy();
    });
    req.on('end', () => {
      try {
        resolve(JSON.parse(raw || '{}'));
      } catch {
        resolve({});
      }
    });
  });
}

/** 统一 JSON 响应。 */
function sendJson(res, code, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(code, { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' });
  res.end(body);
}

const PAGE = /* html */ `<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8" />
<title>Blender MCP 可视化台</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin: 0; font: 13px/1.5 "Segoe UI", system-ui, sans-serif; background: #12141a; color: #e6e8ee; }
  header { display: flex; gap: 14px; align-items: center; padding: 10px 16px; background: #171a21; border-bottom: 1px solid #262b36; }
  header h1 { font-size: 14px; margin: 0; font-weight: 600; letter-spacing: .3px; }
  .pill { padding: 2px 9px; border-radius: 99px; font-size: 12px; border: 1px solid #2f3644; }
  .ok { color: #67e8a2; border-color: #295c43; background: #14301f; }
  .bad { color: #ff8b8b; border-color: #5c2a2a; background: #301515; }
  main { display: grid; grid-template-columns: minmax(0, 1.35fr) minmax(360px, .65fr); gap: 14px; padding: 14px; }
  .card { background: #171a21; border: 1px solid #262b36; border-radius: 10px; overflow: hidden; }
  .card h2 { font-size: 12px; margin: 0; padding: 8px 12px; color: #9aa4b8; font-weight: 600; border-bottom: 1px solid #262b36; }
  .viewer { display: flex; align-items: center; justify-content: center; background: #0b0d12; min-height: 420px; }
  .viewer img { width: 100%; height: auto; display: block; }
  .meta { padding: 8px 12px; color: #7f8aa0; font-size: 12px; display: flex; gap: 12px; flex-wrap: wrap; }
  .grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 8px; padding: 12px; }
  .kv { background: #12141a; border: 1px solid #232936; border-radius: 8px; padding: 7px 10px; }
  .kv b { display: block; color: #7f8aa0; font-size: 11px; font-weight: 500; margin-bottom: 2px; }
  .kv span { font-variant-numeric: tabular-nums; word-break: break-all; }
  textarea { width: 100%; height: 96px; resize: vertical; background: #0f1218; color: #dbe3f0; border: 1px solid #262b36; border-radius: 8px; padding: 8px; font: 12px/1.5 Consolas, monospace; }
  .row { display: flex; gap: 8px; padding: 0 12px 12px; flex-wrap: wrap; }
  button { background: #232a38; color: #dbe3f0; border: 1px solid #313a4c; border-radius: 8px; padding: 6px 11px; cursor: pointer; font-size: 12px; }
  button:hover { background: #2b3444; }
  button.primary { background: #1d4ed8; border-color: #2f5be0; }
  .log { max-height: 230px; overflow: auto; padding: 6px 12px 12px; font: 12px/1.6 Consolas, monospace; }
  .log div { border-bottom: 1px dashed #222838; padding: 4px 0; word-break: break-word; }
  .log .err { color: #ff9b9b; }
  .log time { color: #5d6880; margin-right: 6px; }
  pre { margin: 0; padding: 10px 12px; background: #0f1218; max-height: 190px; overflow: auto; font: 11.5px/1.55 Consolas, monospace; color: #9fb0c8; }
</style>
</head>
<body>
<header>
  <h1>Blender MCP 可视化台</h1>
  <span id="ready" class="pill">连接中…</span>
  <span id="inst" class="pill">实例 —</span>
  <span id="lease" class="pill">租约 —</span>
  <span style="margin-left:auto;color:#5d6880" id="clock">—</span>
</header>
<main>
  <section>
    <div class="card">
      <h2>实时视口（/frame.png 轮询）</h2>
      <div class="viewer"><img id="shot" alt="Blender viewport" /></div>
      <div class="meta"><span id="shotAt">尚未取帧</span><span id="shotMs"></span><span id="region"></span></div>
    </div>
    <div class="card" style="margin-top:14px">
      <h2>控制条（经 MCP 桥同一条 Gateway）</h2>
      <div style="padding:12px"><textarea id="code">import bpy
print(len(bpy.data.objects))</textarea></div>
      <div class="row">
        <button class="primary" onclick="runPy()">运行 Python</button>
        <button onclick="txn('snapshot')">打回退点</button>
        <button onclick="inspect()">场景概览</button>
        <button onclick="grab()">立即取帧</button>
      </div>
    </div>
  </section>
  <aside>
    <div class="card">
      <h2>就绪与计数</h2>
      <div class="grid" id="kvs"></div>
    </div>
    <div class="card" style="margin-top:14px">
      <h2>最近状态原始回包</h2>
      <pre id="raw">—</pre>
    </div>
    <div class="card" style="margin-top:14px">
      <h2>操作日志</h2>
      <div class="log" id="log"></div>
    </div>
  </aside>
</main>
<script>
const $ = (id) => document.getElementById(id);
let frameMs = null;

/** 刷新实时画面（带时间戳防缓存）。 */
function grab(size = 960) {
  const img = $('shot');
  img.onload = () => {
    $('shotAt').textContent = '取帧 ' + new Date().toLocaleTimeString('zh-CN');
    $('shotMs').textContent = frameMs ? (frameMs + ' ms') : '';
  };
  img.src = '/frame.png?size=' + size + '&t=' + Date.now();
}

/** 定时拉状态。 */
async function poll() {
  try {
    const r = await fetch('/api/status', { cache: 'no-store' });
    const d = await r.json();
    const st = d.status || {};
    const ready = st.readiness || {};
    $('ready').textContent = ready.ready ? '就绪' : '未就绪';
    $('ready').className = 'pill ' + (ready.ready ? 'ok' : 'bad');
    $('inst').textContent = '实例 ' + String(ready.instanceId || '—').slice(0, 8);
    const lease = (d.who && d.who.lease) || {};
    $('lease').textContent = '租约 ' + (lease.active ? lease.holder : '空闲');
    $('lease').className = 'pill ' + (lease.active ? 'bad' : 'ok');
    const m = (d.who && d.who.metrics) || {};
    const s = (d.who && d.who.stats) || {};
    const items = [
      ['addon 连接', (ready.addon || {}).connected ? '已连接' : '断开'],
      ['bpy', (ready.bpy || {}).ok ? '可用' : '不可用'],
      ['插件版本', (st.plugin || {}).version || '—'],
      ['能力面', ((st.plugin || {}).provenance || {}).families + ' family / ' + ((st.plugin || {}).provenance || {}).opsCount + ' op'],
      ['调用/错误/超时', (m.calls ?? 0) + ' / ' + (m.errors ?? 0) + ' / ' + (m.timeouts ?? 0)],
      ['最近命令', (m.lastCmd || '—') + (m.lastCmdAgeMs != null ? ' · ' + m.lastCmdAgeMs + ' ms' : '')],
      ['取帧 / 执行 / 视图', (s.frames ?? 0) + ' / ' + (s.acts ?? 0) + ' / ' + (s.views ?? 0)],
      ['作业 / 事务 / 内环', (s.job ?? 0) + ' / ' + (s.txn ?? 0) + ' / ' + (s.loops ?? 0)]
    ];
    $('kvs').innerHTML = items.map(([k, v]) => '<div class="kv"><b>' + k + '</b><span>' + String(v) + '</span></div>').join('');
    $('raw').textContent = JSON.stringify({ readiness: ready, lease: lease, metrics: m }, null, 2).slice(0, 4000);
    $('clock').textContent = new Date().toLocaleTimeString('zh-CN');
    frameMs = m.lastFrameMs ?? frameMs;
    if (d.log) $('log').innerHTML = d.log.map((e) => '<div class="' + (e.ok ? '' : 'err') + '"><time>' + e.at + '</time>' + e.kind + ' · ' + e.text + '</div>').join('');
  } catch (e) {
    $('ready').textContent = '桥不可达';
    $('ready').className = 'pill bad';
  }
}

/** 调 Python。 */
async function runPy() {
  const code = $('code').value;
  await fetch('/api/py', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ code }) });
  poll(); grab();
}

/** 打回退点。 */
async function txn(op) {
  await fetch('/api/txn', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ op, args: { label: 'viewer-' + Date.now() } }) });
  poll();
}

/** 只读场景概览。 */
async function inspect() {
  await fetch('/api/cmd', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ name: 'get_scene_info', params: {} }) });
  poll();
}

grab();
poll();
setInterval(grab, 1200);
setInterval(poll, 2500);
</script>
</html>`;

const server = createServer(async (req, res) => {
  const url = new URL(req.url || '/', 'http://' + HOST);
  const path = url.pathname;

  if (path === '/' || path === '/index.html') {
    res.writeHead(200, { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store' });
    res.end(PAGE);
    return;
  }

  if (path === '/frame.png') {
    try {
      const size = Math.max(120, Math.min(1600, Number(url.searchParams.get('size') || 960)));
      const up = await fetch(`${GATEWAY}/frame.png?size=${size}`, { signal: AbortSignal.timeout(15000) });
      if (!up.ok) {
        sendJson(res, 502, { ok: false, error: 'gateway frame HTTP ' + up.status });
        return;
      }
      const buf = Buffer.from(await up.arrayBuffer());
      res.writeHead(200, { 'content-type': 'image/png', 'cache-control': 'no-store' });
      res.end(buf);
    } catch (e) {
      sendJson(res, 502, { ok: false, error: e instanceof Error ? e.message : String(e) });
    }
    return;
  }

  if (path === '/api/status') {
    try {
      const [status, who] = await Promise.all([
        gwJson('/status', { timeoutMs: 12000 }),
        gwJson('/who', { timeoutMs: 12000 }),
      ]);
      sendJson(res, 200, { ok: true, status: status.data, who: who.data, log: logRing });
    } catch (e) {
      sendJson(res, 502, { ok: false, error: e instanceof Error ? e.message : String(e), log: logRing });
    }
    return;
  }

  if (path === '/api/py' && req.method === 'POST') {
    const body = await readJson(req);
    const t0 = Date.now();
    const { data } = await gwJson('/act', { method: 'POST', body: { code: String(body.code || ''), timeoutMs: 120000 }, timeoutMs: 130000 });
    pushLog('py', (data?.ok ? 'ok ' + (Date.now() - t0) + 'ms · ' + String(data.stdout || '').trim().slice(0, 200) : 'err ' + (data?.error || 'unknown')), data?.ok === true);
    sendJson(res, 200, data);
    return;
  }

  if (path === '/api/txn' && req.method === 'POST') {
    const body = await readJson(req);
    const { data } = await gwJson('/txn', { method: 'POST', body: { op: String(body.op || 'list'), args: body.args || {} } });
    pushLog('txn', body.op + ' · ' + (data?.ok ? 'ok' : 'err ' + (data?.error || '')), data?.ok === true);
    sendJson(res, 200, data);
    return;
  }

  if (path === '/api/cmd' && req.method === 'POST') {
    const body = await readJson(req);
    const { data } = await gwJson('/cmd', { method: 'POST', body: { name: String(body.name || ''), params: body.params || {} }, timeoutMs: 30000 });
    pushLog('cmd', body.name + ' · ' + (data?.ok ? 'ok' : 'err ' + (data?.error || '')), data?.ok === true);
    sendJson(res, 200, data);
    return;
  }

  sendJson(res, 404, { ok: false, error: 'not found', path });
});

server.listen(PORT, HOST, () => {
  process.stdout.write(`blender-viewer on http://${HOST}:${PORT}  (gateway ${GATEWAY})\n`);
});
