/**
 * blender-mcp-bridge —— DSH Blender Gateway 的完整 MCP（Model Context Protocol）适配层。
 *
 * 架构（只读借用，不改对方任何文件）：
 *   MCP 客户端（mcporter / Claude / Cursor）
 *     │ JSON-RPC 2.0 over stdio（逐行）
 *     ▼
 *   本文件（零依赖 Node）
 *     │ HTTP 127.0.0.1:39877  —— dsh-blender-plugin 的 29 条路由契约
 *     ▼
 *   dsh-blender-plugin runtime（DSH 宿主进程 6812 拉起，文件一律不修改）
 *     │ WebSocket（addon 主动反连，按 instanceId 路由）
 *     ▼
 *   DSH Blender Bridge addon → Blender 主线程 bpy
 *
 * 覆盖：/status /who /health /doctor /lease /release /plan /loop /opt /preset /job /headless
 *      /worker /perf /act /cmd /commands /frame.png /view /txn，以及 /host/* 只读查询。
 *
 * 环境变量：
 *   DSH_BLENDER_GATEWAY   Gateway 基址，默认 http://127.0.0.1:39877
 *   BLENDER_MCP_OUT_DIR   截图落盘目录，默认 %TEMP%\blender-mcp
 *   DSH_HOST_ID / DSH_SESSION_ID / DSH_AGENT_ID   调用方身份头（仅 session 归属动作使用）
 *   BLENDER_MCP_IDENTITY=always  所有请求都带身份头（仅调试；会让 /act 吃 409）
 *   BLENDER_MCP_ANON=1    强制不带身份头
 *   BLENDER_MCP_DEBUG=1   往 stderr 打诊断
 */
import { mkdir, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import process from 'node:process';

const GATEWAY = (process.env.DSH_BLENDER_GATEWAY || 'http://127.0.0.1:39877').replace(/\/+$/, '');
const OUT_DIR = process.env.BLENDER_MCP_OUT_DIR || join(tmpdir(), 'blender-mcp');
const SERVER_INFO = { name: 'blender-mcp-bridge', version: '0.2.0' };
const DEFAULT_TIMEOUT_MS = 120000;

/** 调用方身份：Gateway 的 session 动作要求 X-DSH-* 三个头，缺了直接 400。 */
function identityHeaders() {
  if (process.env.BLENDER_MCP_ANON === '1') return {};
  return {
    'x-dsh-host-id': process.env.DSH_HOST_ID || 'doubao-mcp-bridge',
    'x-dsh-session-id': process.env.DSH_SESSION_ID || 'mcp-bridge',
    'x-dsh-agent-id': process.env.DSH_AGENT_ID || 'main',
  };
}

/** 解析响应体：普通 JSON 或 Gateway 的流式回执（每行一条 JSON，取最后一条有效信封）。 */
function parseBody(text) {
  const trimmed = text.trim();
  if (!trimmed) return { ok: false, error: 'empty_response' };
  try {
    return JSON.parse(trimmed);
  } catch {
    /* 落到流式解析 */
  }
  const lines = trimmed.split('\n').map((s) => s.trim()).filter(Boolean);
  const parsed = [];
  for (const line of lines) {
    try {
      parsed.push(JSON.parse(line));
    } catch {
      /* 心跳非 JSON 行直接跳过 */
    }
  }
  if (!parsed.length) return { ok: false, error: 'non_json_response', raw: trimmed.slice(0, 300) };
  const last = parsed[parsed.length - 1];
  if (parsed.length > 1) last.stream = parsed.slice(0, -1);
  return last;
}

/**
 * POST/GET JSON 接口。
 * identity=true 只在 session 动作时打开：带身份头后 /act、/cmd 会进会话归属门，
 * 未认领该会话的调用方会拿到 409 session_unavailable（实测），因此默认匿名。
 */
async function callJson(path, { method = 'GET', body, timeoutMs = DEFAULT_TIMEOUT_MS, headers = {}, identity = false } = {}) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  const idHeaders = identity || process.env.BLENDER_MCP_IDENTITY === 'always' ? identityHeaders() : {};
  try {
    const res = await fetch(GATEWAY + path, {
      method,
      headers: { ...idHeaders, ...(body === undefined ? {} : { 'content-type': 'application/json' }), ...headers },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal: ctrl.signal,
    });
    const text = await res.text();
    return { status: res.status, json: parseBody(text) };
  } finally {
    clearTimeout(timer);
  }
}

/** 抓 PNG 二进制（/frame.png 或 /view）。 */
async function capturePng(path, init = {}, timeoutMs = DEFAULT_TIMEOUT_MS) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(GATEWAY + path, { ...init, headers: { ...(init.headers || {}) }, signal: ctrl.signal });
    if (!res.ok) throw new Error(`gateway ${path} -> HTTP ${res.status}: ${(await res.text()).slice(0, 300)}`);
    const buf = Buffer.from(await res.arrayBuffer());
    const raw = res.headers.get('x-dsh-view');
    let meta = null;
    if (raw) {
      try {
        meta = JSON.parse(decodeURIComponent(raw));
      } catch {
        meta = { raw: raw.slice(0, 200) };
      }
    }
    return { png: buf, meta };
  } finally {
    clearTimeout(timer);
  }
}

/** 截图落盘，返回绝对路径。 */
async function savePng(png) {
  await mkdir(OUT_DIR, { recursive: true });
  const stamp = new Date().toISOString().replace(/[:.]/g, '-');
  const file = join(OUT_DIR, `blender-frame-${stamp}.png`);
  await writeFile(file, png);
  return file;
}

/** 文本结果。 */
function text(payload, isError = false) {
  const body = typeof payload === 'string' ? payload : JSON.stringify(payload, null, 2);
  return { content: [{ type: 'text', text: body }], isError };
}

/** 文本 + 图片结果。 */
function textWithImage(payload, png, path, isError = false) {
  return {
    content: [
      { type: 'text', text: JSON.stringify(payload, null, 2) },
      { type: 'image', data: png.toString('base64'), mimeType: 'image/png' },
      { type: 'text', text: `image: ${path}` },
    ],
    isError,
  };
}

/** 异常收口。 */
function fail(err) {
  const msg = err instanceof Error ? err.message : String(err);
  return text({ ok: false, error: msg }, true);
}

/** 统一把 {status,json} 转成 MCP 结果；HTTP 非 2xx 一律判错。 */
function envelope({ status, json }) {
  const ok = status >= 200 && status < 300 && json?.ok !== false;
  return text({ httpStatus: status, ...json }, ok === false);
}

/* ------------------------------- 工具定义 ------------------------------- */

const TOOLS = [
  {
    name: 'blender_status',
    description:
      '就绪探针：Gateway 存活、addon 连接、bpy 可用、实例 ID、写租约、最近调用指标。开工前先跑这个。',
    inputSchema: { type: 'object', properties: { verbose: { type: 'boolean', description: 'true 时返回 /who 全量（含 config/metrics）' } }, additionalProperties: false },
  },
  {
    name: 'blender_py',
    description: '在已连接的 GUI 会话里执行 Python（bpy），持久内核。see=true 时额外抓一帧。写操作受 Gateway 写租约门控。',
    inputSchema: {
      type: 'object',
      properties: {
        code: { type: 'string', description: 'Python 源码' },
        timeoutMs: { type: 'number', description: '执行超时，默认 120000' },
        see: { type: 'boolean', description: '执行后抓帧' },
        frameSize: { type: 'number', description: '截图边长 120-1600，默认 560' },
        file: { type: 'string', description: '改为执行磁盘上的 .py 文件（与 code 二选一）' },
      },
      additionalProperties: false,
    },
  },
  {
    name: 'blender_see',
    description: '取画面：mode=frame 截视口区域；mode=view 按 viewpoint（viewport/camera/custom）出图，返回 PNG 与元信息（尺寸/覆盖度/告警）。',
    inputSchema: {
      type: 'object',
      properties: {
        mode: { type: 'string', enum: ['frame', 'view'], description: '默认 frame' },
        size: { type: 'number', description: '边长像素，默认 560' },
        full: { type: 'boolean', description: 'frame 模式取整窗' },
        viewpoint: { type: 'string', enum: ['viewport', 'camera', 'custom'], description: 'view 模式视角' },
        view: { type: 'object', description: 'view 模式其余参数原样透传（如 azimuth/elevation/target 等）' },
      },
      additionalProperties: false,
    },
  },
  {
    name: 'blender_inspect',
    description: 'addon 只读命令面：action=help 列出全部命令（含 access/gate/integration 状态）；action=run 执行一条命令。',
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['help', 'run'], description: '默认 help' },
        command: { type: 'string', description: 'action=run 时的命令名' },
        args: { type: 'object', description: '命令参数' },
        timeoutMs: { type: 'number' },
      },
      additionalProperties: false,
    },
  },
  {
    name: 'blender_op',
    description:
      '能力面（34 family / 228 op）唯一入口。action=plan 直接执行契约层 op（如 check_interference / asm_* / blockout_* / img_* / gate_*）；action=catalog 拿清单；action=loop|opt|preset 走内环/优化/配方库。',
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['plan', 'catalog', 'loop', 'opt', 'preset'], description: '默认 catalog' },
        op: { type: 'string', description: 'plan/loop/opt/preset 的子操作名' },
        args: { type: 'object', description: '子操作参数' },
        path: { type: 'string', description: 'loop export 之类的落盘路径' },
        payload: { type: 'object', description: '其余字段原样透传（iterations、spec、top…）' },
      },
      additionalProperties: false,
    },
  },
  {
    name: 'blender_run',
    description:
      '作业入口：action=submit(kind=render|headless) 异步提交；status/wait/collect/cancel/list 管理作业；action=worker 管热无头会话；action=tune 做渲染性能诊断（/perf）。',
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', enum: ['submit', 'status', 'wait', 'collect', 'cancel', 'list', 'worker', 'tune', 'headless'], description: '默认 list' },
        kind: { type: 'string', enum: ['render', 'headless'], description: 'submit 的作业类型，默认 headless' },
        id: { type: 'string', description: '作业 id' },
        timeoutMs: { type: 'number', description: 'wait/headless 超时' },
        tail: { type: 'number', description: 'collect 日志尾行数，默认 4000' },
        workerOp: { type: 'string', enum: ['start', 'stop', 'restart', 'list', 'exec', 'see', 'watch', 'reset', 'status'], description: 'action=worker 的子操作' },
        code: { type: 'string', description: 'worker exec 的 Python' },
        name: { type: 'string', description: 'worker 名字，默认 default' },
        op: { type: 'string', description: 'tune 的子操作（status/analyze/apply/revert）' },
        args: { type: 'object' },
        payload: { type: 'object', description: '其余字段原样透传（scene、output、samples…）' },
      },
      additionalProperties: false,
    },
  },
  {
    name: 'blender_session',
    description:
      '会话归属与写权限：action=list/status/doctor/presence/grants 只读；attach/claim/detach/release/grant/revoke/mark_read 管归属；lease/release-lease 管写租约；pause/resume 仅人类可操作。',
    inputSchema: {
      type: 'object',
      properties: {
        action: { type: 'string', description: 'list/status/doctor/presence/grants/attach/claim/detach/release/grant/revoke/mark_read/lease/release-lease/pause/resume/start/stop/consent/heartbeat/update_view/update_selection' },
        sessionId: { type: 'string' },
        instanceId: { type: 'string', description: 'attach/claim 指定实例' },
        scopes: { type: 'array', items: { type: 'string' }, description: 'grant 的写 scope（exclusive / collection:x / prefix:x / object:x）' },
        grantId: { type: 'string' },
        kind: { type: 'string', description: 'start 的会话类型（user-gui / virtual-gui）' },
        consentToken: { type: 'string', description: 'start virtual-gui 需要宿主签发的一次性 token' },
        holder: { type: 'string', description: 'lease 的持有者标识' },
        ttlMs: { type: 'number' },
        force: { type: 'boolean', description: 'lease 抢占' },
        scope: { type: 'string' },
        payload: { type: 'object', description: '其余字段原样透传' },
      },
      additionalProperties: false,
    },
  },
  {
    name: 'blender_txn',
    description: '事务/回退点：snapshot、mark、marks、restore、revert、list、prune、drop、help。写场景前先 snapshot。',
    inputSchema: {
      type: 'object',
      properties: {
        op: { type: 'string', description: 'snapshot/mark/marks/restore/revert/list/prune/drop/help' },
        args: { type: 'object' },
      },
      additionalProperties: false,
    },
  },
  {
    name: 'blender_gateway',
    description:
      '原路由逃生口（只读白名单）：直接查 Gateway 未单独封装的端点，如 /doctor、/host/state、/host/jobs、/host/scene-items、/host/selection/latest。',
    inputSchema: {
      type: 'object',
      properties: {
        path: { type: 'string', description: '白名单路径，如 /doctor' },
        method: { type: 'string', enum: ['GET', 'POST'], description: '默认 GET' },
        body: { type: 'object', description: 'POST 时的请求体' },
      },
      required: ['path'],
      additionalProperties: false,
    },
  },
];

/* ------------------------------- 工具实现 ------------------------------- */

const READONLY_RAW_PATHS = new Set([
  '/health',
  '/status',
  '/doctor',
  '/who',
  '/commands',
  '/host/state',
  '/host/jobs',
  '/host/scene-items',
  '/host/selection/latest',
]);

const HANDLERS = {
  async blender_status(args) {
    const status = await callJson('/status', { timeoutMs: 15000 });
    const readiness = status.json?.readiness || {};
    const summary = {
      ok: readiness.ready === true,
      gateway: readiness.gateway,
      addon: readiness.addon,
      bpy: readiness.bpy,
      instanceId: readiness.instanceId,
      pluginVersion: status.json?.plugin?.version,
      families: status.json?.plugin?.provenance?.families,
      opsCount: status.json?.plugin?.provenance?.opsCount,
      addonError: status.json?.addonError ?? null,
      views: status.json?.region?.views?.length ?? 0,
    };
    if (args.verbose) {
      const who = await callJson('/who', { timeoutMs: 15000 });
      return text({ status: summary, who: who.json }, summary.ok !== true);
    }
    return text(summary, summary.ok !== true);
  },

  async blender_py(args) {
    const timeoutMs = Number(args.timeoutMs) || DEFAULT_TIMEOUT_MS;
    if (!args.code && !args.file) throw new Error('blender_py 需要 code 或 file');
    const { json } = await callJson('/act', {
      method: 'POST',
      body: { code: args.code, file: args.file, timeoutMs },
      timeoutMs: timeoutMs + 5000,
    });
    const payload = {
      ok: json?.ok === true && json?.executed !== false,
      ms: json?.ms ?? null,
      executed: json?.executed ?? false,
      stdout: json?.stdout ?? '',
      stderr: json?.stderr ?? '',
      error: json?.error ?? null,
      traceback: json?.traceback ?? null,
      sceneEpoch: json?.sceneEpoch ?? null,
    };
    if (payload.ok && args.see) {
      const size = Math.max(120, Math.min(1600, Number(args.frameSize) || 560));
      const { png } = await capturePng(`/frame.png?size=${size}`);
      const path = await savePng(png);
      return textWithImage({ ...payload, framePath: path }, png, path, false);
    }
    return text(payload, payload.ok !== true);
  },

  async blender_see(args) {
    const mode = args.mode || 'frame';
    const size = Math.max(120, Math.min(1600, Number(args.size) || 560));
    if (mode === 'frame') {
      const { png, meta } = await capturePng(`/frame.png?size=${size}${args.full ? '&full=1' : ''}`);
      const path = await savePng(png);
      return textWithImage({ ok: true, mode, meta, framePath: path }, png, path);
    }
    const body = { ...(args.view || {}), viewpoint: args.viewpoint || 'viewport', size };
    const { png, meta } = await capturePng('/view', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(body),
    });
    const path = await savePng(png);
    return textWithImage({ ok: true, mode, meta, framePath: path }, png, path);
  },

  async blender_inspect(args) {
    if ((args.action || 'help') === 'help') {
      const { json } = await callJson('/commands', { timeoutMs: 20000 });
      return text(
        {
          scene: json?.scene,
          file: json?.file,
          total: json?.total,
          available: json?.available,
          disabled: json?.disabled,
          gates: json?.gates,
          integrations: json?.integrations,
          commands: json?.commands,
        },
        json?.ok !== true,
      );
    }
    const { json } = await callJson('/cmd', {
      method: 'POST',
      body: { name: String(args.command || ''), params: args.args || {}, timeoutMs: Number(args.timeoutMs) || DEFAULT_TIMEOUT_MS },
      timeoutMs: (Number(args.timeoutMs) || DEFAULT_TIMEOUT_MS) + 5000,
    });
    return text(json, json?.ok !== true);
  },

  async blender_op(args) {
    const action = args.action || 'catalog';
    if (action === 'plan' || action === 'catalog') {
      const op = args.op || (action === 'catalog' ? 'catalog' : 'status');
      const { json } = await callJson('/plan', { method: 'POST', body: { op, args: args.args || {} }, timeoutMs: DEFAULT_TIMEOUT_MS });
      return text(json, json?.ok !== true);
    }
    const path = action === 'loop' ? '/loop' : action === 'opt' ? '/opt' : '/preset';
    const body = { ...(args.payload || {}), op: args.op || (action === 'preset' ? 'list' : 'status') };
    if (args.args) body.args = args.args;
    if (args.path) body.path = args.path;
    const { json } = await callJson(path, { method: 'POST', body, timeoutMs: DEFAULT_TIMEOUT_MS });
    return text(json, json?.ok !== true);
  },

  async blender_run(args) {
    const action = args.action || 'list';
    if (action === 'headless') {
      const { json } = await callJson('/headless', {
        method: 'POST',
        body: { ...(args.payload || {}), timeoutMs: Number(args.timeoutMs) || DEFAULT_TIMEOUT_MS },
        timeoutMs: (Number(args.timeoutMs) || DEFAULT_TIMEOUT_MS) + 15000,
      });
      return text(json, json?.ok !== true);
    }
    if (action === 'worker') {
      const body = { ...(args.payload || {}), op: args.workerOp || 'status', name: args.name, code: args.code };
      const { json } = await callJson('/worker', { method: 'POST', body, timeoutMs: DEFAULT_TIMEOUT_MS + 15000 });
      return text(json, json?.ok !== true);
    }
    if (action === 'tune') {
      const { json } = await callJson('/perf', { method: 'POST', body: { op: args.op || 'status', args: args.args || {} }, timeoutMs: 60000 });
      return text(json, json?.ok !== true);
    }
    const opMap = { submit: 'start', status: 'status', wait: 'wait', collect: 'collect', cancel: 'kill', list: 'list' };
    const op = opMap[action] || 'list';
    const body = { ...(args.payload || {}), op };
    if (op === 'start') body.kind = args.kind || body.kind || 'headless';
    if (args.id) body.id = args.id;
    if (op === 'wait') body.timeoutMs = Number(args.timeoutMs) || DEFAULT_TIMEOUT_MS;
    if (op === 'collect') body.tail = Number(args.tail) || 4000;
    const { json } = await callJson('/job', {
      method: 'POST',
      body,
      timeoutMs: (Number(args.timeoutMs) || DEFAULT_TIMEOUT_MS) + 15000,
    });
    return text(json, json?.ok !== true);
  },

  async blender_session(args) {
    const action = args.action || 'list';
    if (action === 'list') {
      const { json } = await callJson('/who', { timeoutMs: 15000 });
      return text({ sessions: json?.sessions ?? [], lease: json?.lease ?? null }, json?.ok !== true);
    }
    if (action === 'lease') {
      const { json } = await callJson('/lease', {
        method: 'POST',
        body: { holder: args.holder, ttlMs: args.ttlMs, force: !!args.force, scope: args.scope },
        timeoutMs: 20000,
      });
      return text(json, json?.ok !== true);
    }
    if (action === 'release-lease') {
      const { json } = await callJson('/release', { method: 'POST', body: { holder: args.holder }, timeoutMs: 20000 });
      return text(json, json?.ok !== true);
    }
    const body = {
      ...(args.payload || {}),
      action,
      sessionId: args.sessionId,
      instanceId: args.instanceId,
      scopes: args.scopes,
      grantId: args.grantId,
      kind: args.kind,
      consentToken: args.consentToken,
      ttlMs: args.ttlMs,
    };
    const { json } = await callJson('/lease', { method: 'POST', body, timeoutMs: 30000, identity: true });
    return text(json, json?.ok !== true);
  },

  async blender_txn(args) {
    const { json } = await callJson('/txn', { method: 'POST', body: { op: args.op || 'list', args: args.args || {} }, timeoutMs: 60000 });
    return text(json, json?.ok !== true);
  },

  async blender_gateway(args) {
    const path = String(args.path || '');
    if (!READONLY_RAW_PATHS.has(path.split('?')[0])) throw new Error(`path 不在只读白名单：${path}`);
    const { json } = await callJson(path, {
      method: args.method || 'GET',
      body: args.method === 'POST' ? args.body || {} : undefined,
      timeoutMs: 30000,
    });
    return text(json, json?.ok !== true);
  },
};

/* ------------------------------ JSON-RPC 循环 ------------------------------ */

/** 处理一条 JSON-RPC 消息；通知返回 null。 */
async function dispatch(msg) {
  const base = { jsonrpc: '2.0', id: msg.id };
  const method = msg.method;
  if (method === 'initialize') {
    return {
      ...base,
      result: {
        protocolVersion: msg.params?.protocolVersion || '2024-11-05',
        capabilities: { tools: { listChanged: false } },
        serverInfo: SERVER_INFO,
      },
    };
  }
  if (method === 'notifications/initialized' || method === 'initialized') return null;
  if (method === 'ping') return { ...base, result: {} };
  if (method === 'tools/list') return { ...base, result: { tools: TOOLS } };
  if (method === 'tools/call') {
    const handler = HANDLERS[msg.params?.name];
    if (!handler) return { ...base, error: { code: -32601, message: `unknown tool: ${String(msg.params?.name)}` } };
    try {
      return { ...base, result: await handler(msg.params?.arguments || {}) };
    } catch (e) {
      return { ...base, result: fail(e) };
    }
  }
  if (msg.id === undefined) return null;
  return { ...base, error: { code: -32601, message: `unknown method: ${String(method)}` } };
}

/** stderr 诊断。 */
function log(...args) {
  if (process.env.BLENDER_MCP_DEBUG === '1') process.stderr.write(`[blender-mcp] ${args.join(' ')}\n`);
}

let buffer = '';
process.stdin.setEncoding('utf8');
process.stdin.on('data', (chunk) => {
  buffer += chunk;
  let nl = buffer.indexOf('\n');
  while (nl >= 0) {
    const line = buffer.slice(0, nl).trim();
    buffer = buffer.slice(nl + 1);
    nl = buffer.indexOf('\n');
    if (!line) continue;
    let msg;
    try {
      msg = JSON.parse(line);
    } catch {
      log('bad json line');
      continue;
    }
    dispatch(msg)
      .then((resp) => {
        if (resp) process.stdout.write(JSON.stringify(resp) + '\n');
      })
      .catch((e) => log('dispatch error', e instanceof Error ? e.message : String(e)));
  }
});
process.stdin.on('end', () => process.exit(0));
log('bridge started', `gateway=${GATEWAY}`, `outDir=${OUT_DIR}`);
