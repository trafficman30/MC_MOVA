# cm5_web.py
# CM5 Platform — unified web dashboard
#
# Single Flask app serving all tabs via SSE.
# Started from main.py via start_web(), receives live service references.
#
# Routes:
#   /              → redirect to /ug405/
#   /ug405/        → UG405 / SCOOT tab
#   /io/           → IO Bus tab
#   /conditioner/  → Conditioner tab
#   /rtig/         → RTIG / TLP tab
#   /stream        → SSE stream (all tabs share one connection)
#   /login         → login page
#   /logout        → end session
#   /setup         → first-run admin creation
#   /api/users/*   → user management (admin only)

import json
import select as _select
import urllib.request
import urllib.error
import os
import threading
import time
import logging
import configparser
from collections import deque

from flask import Flask, Response, redirect, render_template_string, request, send_file, make_response
import auth as _auth

log = logging.getLogger('WEB')

# ── Shared state — injected by start_web() ────────────────────────────────────
_io        = None   # IOBus
_ug405_cfg = None   # {scns, control, reply}
_live      = None   # live dict (instation, opmode etc)
_op_mode   = None   # {'value': N}
_rtig_svc  = None   # RTIGService instance
_cond      = None   # IOConditioner instance

_flir             = None   # FlirDriver instance
_agd              = None   # AGDDriver instance
_autodim          = None   # AutoDimDriver instance
_subscribers      = []
_subscribers_lock = threading.Lock()
_mem_limit_mb     = 0
_xkop_driver      = None
_rpdb_driver      = None
_offline_plan     = None
_kernel_registry  = None   # pci_mova KernelRegistry (MOVA stream IPC clients)
_mova_stream_count = 0     # licensed streams (set by start_web)
_mova_datasets_dir = None  # path to .mxds directory
_snmp_cfg_state         = {}             # field → {value, ts}  — persistent instation config
_snmp_opmode_trans      = {}             # '1→2' etc. → ts of last occurrence
_snmp_ctrl_log          = deque(maxlen=30)  # rolling Control SET log

app = Flask(__name__)

# ── HTML ──────────────────────────────────────────────────────────────────────

HTML = r'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>CM5 Platform</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
         font-size: 13px; background: #f0f2f5; display: flex;
         flex-direction: column; height: 100vh; overflow: hidden; }

  /* ── Top bar ── */
  .topbar { background: #1a2744; color: #fff; padding: 0 20px;
            display: flex; align-items: center; justify-content: space-between;
            height: 48px; flex-shrink: 0; }
  .topbar-brand { font-size: 16px; font-weight: 600; letter-spacing: 0.5px; }
  .topbar-site  { font-size: 12px; color: rgba(255,255,255,0.55); margin-left: 16px; }
  .topbar-left  { display: flex; align-items: baseline; gap: 0; }
  .status-pills { display: flex; gap: 8px; }
  .pill { padding: 3px 10px; border-radius: 20px; font-size: 11px; font-weight: 500; }
  .pill-ok   { background: #d4edda; color: #155724; }
  .pill-warn { background: #fff3cd; color: #856404; }
  .pill-err  { background: #f8d7da; color: #721c24; }

  /* ── Shell ── */
  .shell { display: flex; flex: 1; overflow: hidden; }

  /* ── Sidebar ── */
  .sidebar { width: 196px; background: #1e2d50; display: flex;
             flex-direction: column; overflow-y: auto; flex-shrink: 0; }
  .nav-group-label { padding: 12px 14px 4px; font-size: 10px; font-weight: 600;
                     color: rgba(255,255,255,0.3); text-transform: uppercase;
                     letter-spacing: 0.9px; }
  .nav-item { display: flex; align-items: center; gap: 9px; padding: 7px 14px;
              cursor: pointer; color: rgba(255,255,255,0.6); font-size: 12px;
              border-left: 2px solid transparent; user-select: none; }
  .nav-item:hover  { background: rgba(255,255,255,0.07); color: #fff; }
  .nav-item.active { background: rgba(0,180,255,0.14); color: #00d4ff;
                     border-left-color: #00d4ff; }
  .nav-icon { font-size: 15px; width: 18px; text-align: center; }
  .sidebar-footer { margin-top: auto; padding: 10px 14px;
                    border-top: 1px solid rgba(255,255,255,0.08);
                    font-size: 10px; color: rgba(255,255,255,0.25); }

  /* ── Content ── */
  .content { flex: 1; overflow: hidden; padding: 16px 20px;
             height: calc(100vh - 48px); }
  .panel   { display: none; }
  .panel.active { display: block; height: 100%; overflow-y: auto; width: 100%; }

  /* ── Auth ── */
  body.viewer-mode .admin-only { display: none !important; }
  .topbar-user { display:flex;align-items:center;gap:8px;margin-left:12px }
  .topbar-user span { font-size:11px;color:rgba(255,255,255,.55) }
  .topbar-user button { background:rgba(255,255,255,.1);border:1px solid rgba(255,255,255,.2);
    color:rgba(255,255,255,.7);border-radius:4px;padding:3px 10px;font-size:11px;cursor:pointer }
  .topbar-user button:hover { background:rgba(255,255,255,.2) }

  /* ── Metric cards ── */
  .cards-row { display: grid; gap: 10px; margin-bottom: 16px; }
  .cards-4   { grid-template-columns: repeat(4, 1fr); }
  .cards-3   { grid-template-columns: repeat(3, 1fr); }
  .metric-card { background: #fff; border: 1px solid #e5e7eb;
                 border-radius: 8px; padding: 12px 14px; }
  .metric-label { font-size: 11px; color: #6b7280; margin-bottom: 4px; }
  .metric-value { font-size: 20px; font-weight: 600; color: #111827; }
  .metric-value.green  { color: #15803d; }
  .metric-value.amber  { color: #b45309; }
  .metric-value.red    { color: #b91c1c; }
  .metric-value.blue   { color: #1d4ed8; }
  .metric-value.sm     { font-size: 14px; padding-top: 3px; }

  /* ── Section title ── */
  .section-title { font-size: 12px; font-weight: 600; color: #374151;
                   margin-bottom: 8px; text-transform: uppercase;
                   letter-spacing: 0.5px; display:flex; align-items:center; gap:8px; }
  .collapse-btn { background:none; border:1px solid #d1d5db; border-radius:4px;
                  cursor:pointer; font-size:11px; color:#6b7280; padding:1px 8px;
                  font-weight:400; text-transform:none; letter-spacing:0;
                  margin-left:auto; white-space:nowrap; }
  .collapse-btn:hover { background:#f3f4f6; color:#374151; }

  /* ── Tables ── */
  .table-card { background: #fff; border: 1px solid #e5e7eb;
                border-radius: 8px; overflow: hidden; margin-bottom: 14px; }
  .table-card table { width: 100%; border-collapse: collapse; }
  .table-card th { background: #f9fafb; color: #6b7280; padding: 7px 12px;
                   text-align: left; font-weight: 600; font-size: 11px;
                   border-bottom: 1px solid #e5e7eb; white-space: nowrap; }
  .table-card td { padding: 6px 12px; border-bottom: 1px solid #f3f4f6;
                   color: #111827; font-size: 12px; }
  .table-card tr:last-child td { border-bottom: none; }
  .table-card tr:hover td { background: #f9fafb; }
  .val-on  { color: #15803d; font-weight: 600; }
  .val-off { color: #9ca3af; }

  /* ── Badges ── */
  .badge { display: inline-block; padding: 2px 8px; border-radius: 10px;
           font-size: 10px; font-weight: 600; white-space: nowrap; }
  .badge-green  { background: #d1fae5; color: #065f46; }
  .badge-red    { background: #fee2e2; color: #991b1b; }
  .badge-blue   { background: #dbeafe; color: #1e40af; }
  .badge-amber  { background: #fef3c7; color: #92400e; }
  .badge-purple { background: #ede9fe; color: #5b21b6; }
  .badge-gray   { background: #f3f4f6; color: #374151; }

  /* ── Two-column layout ── */
  .row2 { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }

  /* ── TLP detail card ── */
  .tlp-card { background: #fff; border: 1px solid #e5e7eb;
              border-radius: 8px; padding: 14px 16px; margin-bottom: 14px; }
  .tlp-row  { display: flex; justify-content: space-between; align-items: center;
              padding: 5px 0; border-bottom: 1px solid #f3f4f6; }
  .tlp-row:last-child { border-bottom: none; }
  .tlp-label { font-size: 12px; color: #6b7280; }
  .tlp-val   { font-size: 12px; font-weight: 600; color: #111827; }
  .tlp-val.green { color: #15803d; }
  .tlp-val.blue  { color: #1d4ed8; }
  .tlp-none  { font-size: 12px; color: #9ca3af; font-style: italic; }

  /* ── Log entries ── */
  .log-entry { font-family: "Courier New", monospace; font-size: 11px;
               padding: 4px 12px; border-bottom: 1px solid #f3f4f6;
               display: flex; gap: 10px; align-items: baseline; }
  .log-entry:last-child { border-bottom: none; }
  .log-ts      { color: #9ca3af; min-width: 56px; }
  .log-sig     { color: #1d4ed8; min-width: 60px; }
  .log-fields  { color: #374151; flex: 1; }
  .log-matched { color: #15803d; font-weight: 600; }
  .log-none    { color: #9ca3af; }

  /* ── Mono text ── */
  .mono { font-family: "Courier New", monospace; font-size: 11px; }

  /* ── Filter input ── */
  .filter-row { display: flex; gap: 8px; margin-bottom: 10px; align-items: center; }
  .filter-row input { padding: 5px 10px; border: 1px solid #d1d5db;
                      border-radius: 6px; font-size: 12px; width: 220px; }
  .filter-row label { font-size: 12px; color: #6b7280; }
  .filter-row select { padding: 5px 8px; border: 1px solid #d1d5db;
                       border-radius: 6px; font-size: 12px; }

  /* ── Priority / SchDev labels ── */
  .prio-1 { color: #b91c1c; font-weight: 600; }
  .prio-2 { color: #b45309; font-weight: 600; }
  .prio-3 { color: #15803d; font-weight: 600; }

  /* ── Auto Dim config card ── */
.ad-config-card { background: #fff; border: 1px solid #e5e7eb;
                    border-radius: 8px; padding: 16px 18px; margin-bottom: 14px; }
.ad-form-grid   { display: grid; grid-template-columns: 1fr 1fr; gap: 10px 16px; }
  .ad-form-group  { display: flex; flex-direction: column; gap: 4px; }
  .ad-label       { font-size: 11px; color: #6b7280; font-weight: 500; }
  .ad-input       { padding: 6px 10px; border: 1px solid #d1d5db; border-radius: 6px;
                    font-size: 12px; width: 100%; }
  .ad-input:focus { outline: 2px solid #3b82f6; border-color: transparent; }
  .ad-btn-save    { padding: 7px 18px; background: #1a2744; color: #fff;
                    border: none; border-radius: 6px; font-size: 12px;
                    cursor: pointer; font-weight: 500; }
  .ad-btn-save:hover { background: #243660; }

  /* ── Config editor ── */
  .cfg-section { background: #fff; border: 1px solid #e5e7eb;
                 border-radius: 8px; padding: 16px 18px; margin-bottom: 14px; }
  .cfg-section-title { font-size: 11px; font-weight: 600; color: #374151;
                       text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 12px; }
  .cfg-services-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
  .cfg-service-row { display: flex; align-items: center; gap: 8px; padding: 8px 10px;
                     border: 1px solid #e5e7eb; border-radius: 6px; background: #f9fafb; }
  .cfg-service-label { font-size: 12px; color: #374151; font-weight: 500; }
  .cfg-toggle { position: relative; display: inline-block;
                width: 36px; height: 20px; flex-shrink: 0; }
  .cfg-toggle input { opacity: 0; width: 0; height: 0; }
  .cfg-slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
                background: #d1d5db; border-radius: 20px; transition: 0.2s; }
  .cfg-slider:before { position: absolute; content: ""; height: 14px; width: 14px;
                       left: 3px; bottom: 3px; background: #fff;
                       border-radius: 50%; transition: 0.2s; }
  input:checked + .cfg-slider { background: #2563eb; }
  input:checked + .cfg-slider:before { transform: translateX(16px); }
  input:disabled + .cfg-slider { background: #9ca3af; cursor: not-allowed; }
  .cfg-form-grid-3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px 16px; }
  .cfg-btn-restart { padding: 7px 18px; background: #b91c1c; color: #fff; border: none;
                     border-radius: 6px; font-size: 12px; cursor: pointer; font-weight: 500; }
  .cfg-btn-restart:hover    { background: #991b1b; }
  .cfg-btn-restart:disabled { background: #9ca3af; cursor: not-allowed; }

  /* ── Log viewer ── */
  .log-toolbar   { display: flex; gap: 8px; margin-bottom: 10px; align-items: center; flex-wrap: wrap; }
  .log-select    { padding: 5px 8px; border: 1px solid #d1d5db; border-radius: 6px;
                   font-size: 12px; min-width: 180px; }
  .log-lvl-btn   { padding: 4px 10px; border: 1px solid #d1d5db; border-radius: 5px;
                   font-size: 11px; font-weight: 600; cursor: pointer;
                   background: #f3f4f6; color: #9ca3af; }
  .log-lvl-btn.active              { background: #f9fafb; color: #374151; border-color: #9ca3af; }
  .log-lvl-btn.active.l-debug      { background: #f3f4f6; color: #6b7280; border-color: #d1d5db; }
  .log-lvl-btn.active.l-info       { background: #dbeafe; color: #1e40af; border-color: #93c5fd; }
  .log-lvl-btn.active.l-warning    { background: #fef3c7; color: #92400e; border-color: #fcd34d; }
  .log-lvl-btn.active.l-error      { background: #fee2e2; color: #991b1b; border-color: #fca5a5; }
  .log-viewer    { background: #fff; border: 1px solid #e5e7eb; border-radius: 8px; }
  .lv-line       { font-family: "Courier New", monospace; font-size: 11px;
                   padding: 2px 12px; border-bottom: 1px solid #f9fafb;
                   white-space: pre-wrap; word-break: break-all; line-height: 1.5; }
  .lv-line:last-child { border-bottom: none; }
  .lv-debug      { color: #9ca3af; }
  .lv-info       { color: #111827; }
  .lv-warning    { color: #b45309; }
  .lv-error      { color: #b91c1c; font-weight: 600; }
  .lv-critical   { color: #7c3aed; font-weight: 700; }
  .lv-match      { background: #fef08a; color: #000; border-radius: 2px; }
  .log-search    { padding: 4px 10px; border: 1px solid #d1d5db; border-radius: 5px;
                   font-size: 12px; flex: 1; min-width: 120px; }
  .log-search:focus { outline: none; border-color: #6366f1; box-shadow: 0 0 0 2px #e0e7ff; }

  /* ── MOVA stream card CSS (matches /opt/MOVA design) ───────────────────── */
  .mova-card          { background:#fff; border:1px solid #e5e7eb; border-radius:8px; overflow:hidden; transition:border-color .2s; margin-bottom:12px; width:100%; box-sizing:border-box; }
  .mova-card.active   { border-color:#00d4ff; }
  .mova-card.warmup   { border-color:#b45309; }
  .mova-card.fault    { border-color:#b91c1c; }
  .mova-card-hdr      { padding:8px 16px; background:#1a2744; display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:6px; }
  .mova-card-title    { font-family:"Courier New",monospace; font-size:13px; font-weight:600; color:#fff; letter-spacing:.06em; }
  .mova-status-badge  { font-size:10px; font-weight:600; padding:2px 10px; border-radius:20px; white-space:nowrap; letter-spacing:.04em; font-family:"Courier New",monospace; }
  .mova-badge-control { background:#d1fae5; color:#065f46; }
  .mova-badge-warmup  { background:#fef3c7; color:#92400e; }
  .mova-badge-off     { background:rgba(255,255,255,.15); color:rgba(255,255,255,.6); }
  .mova-badge-fault   { background:#fee2e2; color:#991b1b; }
  .mova-badge-other   { background:#f0f9ff; color:#0369a1; }
  .mova-status-row    { display:flex; border-bottom:1px solid #e5e7eb; background:#fff; flex-wrap:wrap; }
  .mova-status-cell   { padding:7px 14px; border-right:1px solid #e5e7eb; flex-shrink:0; min-width:80px; }
  .mova-status-cell:last-child { border-right:none; flex:1; min-width:80px; }
  .mova-slabel        { font-family:"Courier New",monospace; font-size:9px; font-weight:600; letter-spacing:.09em; text-transform:uppercase; color:#6b7280; margin-bottom:3px; }
  .mova-sval          { font-family:"Courier New",monospace; font-size:12px; font-weight:600; color:#111827; display:flex; align-items:center; gap:6px; }
  .mova-sval.on       { color:#15803d; }
  .mova-sval.off      { color:#6b7280; }
  .mova-sval.warn     { color:#b45309; }
  .mova-sval.err      { color:#b91c1c; }
  .mova-crb-dot       { width:10px; height:10px; border-radius:50%; background:#d1d5db; flex-shrink:0; transition:all .2s; }
  .mova-crb-dot.on    { background:#15803d; box-shadow:0 0 6px rgba(21,128,61,.4); }
  .mova-sec-hdr       { padding:4px 16px; background:#f9fafb; border-top:1px solid #e5e7eb; font-family:"Courier New",monospace; font-size:9px; font-weight:600; letter-spacing:.1em; text-transform:uppercase; color:#6b7280; }
  .mova-sec-body      { padding:8px 16px; }
  .mova-bit-grid      { display:flex; flex-wrap:wrap; row-gap:6px; }
  .mova-bit-group     { display:flex; margin-right:14px; }
  .mova-bit-cell      { display:flex; flex-direction:column; align-items:center; gap:2px; padding:0 3px; }
  .mova-bit-num       { font-family:"Courier New",monospace; font-size:9px; color:#6b7280; line-height:1; }
  .mova-bit-dot       { width:12px; height:12px; border-radius:50%; background:#d1d5db; border:1px solid #9ca3af; transition:all .1s; }
  .mova-bit-dot.on    { background:#15803d; border-color:#15803d; box-shadow:0 0 3px rgba(21,128,61,.4); }
  .mova-bit-dot.amber { background:#b45309; border-color:#b45309; }
  .mova-bit-dot.red   { background:#b91c1c; border-color:#b91c1c; box-shadow:0 0 3px rgba(185,28,28,.4); }
  .mova-bit-dot.cyan  { background:#0891b2; border-color:#0891b2; box-shadow:0 0 3px rgba(8,145,178,.4); }
  .mova-fault-item    { padding:4px 10px; background:#fee2e2; border:1px solid #fca5a5; border-radius:3px; font-family:"Courier New",monospace; font-size:11px; color:#991b1b; margin-bottom:3px; }
  .mova-ctrl-row      { display:flex; gap:6px; padding:10px 16px; border-top:1px solid #e5e7eb; background:#f9fafb; flex-wrap:wrap; align-items:center; }
  .mova-btn           { padding:5px 14px; border:1px solid #e5e7eb; border-radius:4px; background:#fff; color:#111827; font-family:"Courier New",monospace; font-size:11px; cursor:pointer; transition:all .15s; text-transform:uppercase; font-weight:500; white-space:nowrap; }
  .mova-btn:hover             { background:#f9fafb; border-color:#9ca3af; }
  .mova-btn.mova-on-btn:hover { background:#d1fae5; border-color:#6ee7b7; color:#065f46; }
  .mova-btn.mova-off-btn      { background:#374151; color:#fff; border-color:#374151; }
  .mova-btn.mova-off-btn:hover{ background:#1f2937; }
  .mova-btn.mova-reset-btn:hover { background:#fef3c7; border-color:#fcd34d; color:#92400e; }
  .mova-btn.mova-log-btn      { color:#6b7280; font-size:10px; }
  .mova-btn.mova-log-btn:hover{ background:#eff6ff; border-color:#93c5fd; color:#2563eb; }
</style>
</head>
<body>

<script>window.CM5_ROLE="{{ role }}";</script>

<div class="topbar">
  <div class="topbar-left">
    <span class="topbar-brand">CM5 Platform</span>
    <span class="topbar-site" id="site-name">Loading…</span>
  </div>
  <div class="status-pills" id="status-pills">
    <span class="pill pill-warn">Connecting…</span>
  </div>
  <span class="pill pill-ok" id="mem-pill" style="margin-left:8px">MEM —</span>
  <div class="topbar-user">
    <span>{{ username }}{% if role == 'viewer' %} (read-only){% endif %}</span>
    <form method="POST" action="/logout" style="margin:0">
      <button type="submit">Sign out</button>
    </form>
  </div>
</div>

<div class="shell">
  <div class="sidebar">
    <div class="nav-group-label">UTC / SCOOT</div>
    <div class="nav-item active" onclick="show('ug405',this)">
      <span class="nav-icon">🚦</span> UG405
    </div>

    <div class="nav-group-label">IO</div>
    <div class="nav-item" onclick="show('io',this)">
      <span class="nav-icon">⚡</span> IO Bus
    </div>
    <div class="nav-item" onclick="show('conditioner',this)">
      <span class="nav-icon">⚙</span> Conditioner
    </div>

    <div class="nav-group-label">Services</div>
    <div class="nav-item" onclick="show('rtig',this);loadRTIGCfg()">
      <span class="nav-icon">📡</span> RTIG / TLP
    </div>
    <div class="nav-item" onclick="show('flir',this);loadFLIRMapping()">
      <span class="nav-icon">🔥</span> FLIR
    </div>
    <div class="nav-item" onclick="show('agd',this);loadAGDMapping()">
      <span class="nav-icon">📡</span> AGD650
    </div>
    <div class="nav-item" onclick="show('autodim',this)">
      <span class="nav-icon">🌅</span> DIM Service
    </div>
    <div class="nav-item" onclick="show('offplan',this);loadOffplanCfg()">
      <span class="nav-icon">📅</span> Fixed-Time Plan
    </div>

    {% if mova_stream_count > 0 %}
    <div class="nav-group-label">MOVA</div>
    {% for n in range(mova_stream_count) %}
    <div class="nav-item" id="nav-mova-{{n}}" onclick="showMova({{n}},this)">
      <span class="nav-icon">🚦</span> Stream {{n+1}}
      <span id="mova-pill-{{n}}" style="margin-left:auto;font-size:9px;padding:1px 5px;border-radius:8px;background:#374151;color:#9ca3af">—</span>
    </div>
    {% endfor %}
    {% endif %}

    <div class="nav-group-label">Administration</div>
    <div class="nav-item" onclick="show('config',this);loadPlatformCfg();loadSensorsCfg()">
      <span class="nav-icon">⚙</span> Configuration
    </div>
    <div class="nav-item" onclick="show('scncfg',this);loadSCNConfig()">
      <span class="nav-icon">🗺</span> SCN Config
    </div>
    <div class="nav-item" onclick="show('logs',this);loadLogFiles()">
      <span class="nav-icon">📋</span> Logs
    </div>
    {% if is_admin %}
    <div class="nav-item" onclick="show('users',this);loadUsers()">
      <span class="nav-icon">👤</span> Users
    </div>
    {% endif %}

    <div class="sidebar-footer" id="sidebar-footer">CM5</div>
  </div>

  <div class="content">

    <!-- ── UG405 ── -->
    <div id="panel-ug405" class="panel active">
      <div class="cards-row cards-4">
        <div class="metric-card">
          <div class="metric-label">Op mode</div>
          <div class="metric-value sm amber" id="ug-opmode">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Instation</div>
          <div class="metric-value sm" id="ug-instation">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">SCNs</div>
          <div class="metric-value blue" id="ug-scns">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Last update</div>
          <div class="metric-value sm" id="ug-lastupdate">—</div>
        </div>
      </div>
      <div id="ug-scn-container"></div>
      <div class="section-title" style="margin-top:16px;margin-bottom:6px">Instation Config</div>
      <div id="ug-cfg-state"><div style="color:#9ca3af;font-size:12px;padding:6px">No config received from instation yet.</div></div>
      <div class="section-title" style="margin-top:12px;margin-bottom:6px">opMode Transitions</div>
      <div id="ug-opmode-transitions"></div>
      <div class="section-title" style="margin-top:12px;margin-bottom:6px">Control Activity</div>
      <div id="ug-ctrl-log"><div style="color:#9ca3af;font-size:12px;padding:6px">No control SETs received yet.</div></div>
    </div>

    <!-- ── IO Bus ── -->
    <div id="panel-io" class="panel">
      <div class="cards-row cards-4">
        <div class="metric-card">
          <div class="metric-label">Registered</div>
          <div class="metric-value" id="io-total">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Non-zero</div>
          <div class="metric-value green" id="io-nonzero">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Inputs</div>
          <div class="metric-value" id="io-inputs">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Outputs</div>
          <div class="metric-value" id="io-outputs">—</div>
        </div>
      </div>
      <div id="driver-status-row" style="display:none;margin-bottom:14px">
        <table class="signal-table" style="width:auto;min-width:380px">
          <thead><tr><th>Driver</th><th>Endpoint</th><th>Status</th><th></th></tr></thead>
          <tbody id="driver-status-body"></tbody>
        </table>
      </div>
      <div class="filter-row">
        <label>Filter:</label>
        <input type="text" id="io-filter" placeholder="e.g. xkop.i or virt"
               oninput="renderIOTable()">
        <label>Show:</label>
        <select id="io-show" onchange="renderIOTable()">
          <option value="all">All signals</option>
          <option value="nonzero">Non-zero only</option>
        </select>
      </div>
      <div class="table-card">
        <table>
          <thead>
            <tr>
              <th>Signal</th><th>Owner</th><th>Value</th>
            </tr>
          </thead>
          <tbody id="io-tbody"></tbody>
        </table>
      </div>
    </div>



    <!-- ── Conditioner ── -->
    <div id="panel-conditioner" class="panel">
      <div class="cards-row cards-4">
        <div class="metric-card">
          <div class="metric-label">Rules loaded</div>
          <div class="metric-value" id="cond-rules">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Tick interval</div>
          <div class="metric-value sm" id="cond-tick">50ms</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Active outputs</div>
          <div class="metric-value" id="cond-outputs">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Active (non-zero)</div>
          <div class="metric-value green" id="cond-active">—</div>
        </div>
      </div>

      <!-- Live state table -->
      <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:8px">
        <div class="section-title" style="margin-bottom:0">Rules and output state</div>
        <button onclick="showCondEditor()" class="admin-only" style="padding:5px 14px;border:1px solid #d1d5db;border-radius:6px;background:#1a2744;color:#fff;font-size:12px;cursor:pointer">
          ✎ Edit rules
        </button>
      </div>
      <div class="table-card" id="cond-live-card">
        <table>
          <thead>
            <tr><th>Output</th><th>Expression</th><th>Value</th></tr>
          </thead>
          <tbody id="cond-tbody"></tbody>
        </table>
      </div>

      <!-- Editor (hidden by default) -->
      <div id="cond-editor" style="display:none">
        <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:10px">
          <div class="section-title" style="margin-bottom:0">Rule editor</div>
          <button onclick="hideCondEditor()" style="padding:5px 14px;border:1px solid #d1d5db;border-radius:6px;background:#fff;font-size:12px;cursor:pointer">
            Cancel
          </button>
        </div>

        <div class="table-card" style="margin-bottom:12px">
          <table id="cond-edit-table" style="width:100%;border-collapse:collapse">
            <thead>
              <tr>
                <th style="background:#f9fafb;color:#6b7280;padding:7px 12px;text-align:left;font-size:11px;border-bottom:1px solid #e5e7eb;width:180px">Output signal</th>
                <th style="background:#f9fafb;color:#6b7280;padding:7px 12px;text-align:left;font-size:11px;border-bottom:1px solid #e5e7eb">Expression</th>
                <th style="background:#f9fafb;color:#6b7280;padding:7px 12px;text-align:left;font-size:11px;border-bottom:1px solid #e5e7eb;width:60px"></th>
              </tr>
            </thead>
            <tbody id="cond-edit-tbody"></tbody>
          </table>
        </div>

        <div style="display:flex;gap:10px;margin-bottom:16px">
          <button onclick="condAddRow()" class="admin-only" style="padding:6px 14px;border:1px solid #d1d5db;border-radius:6px;background:#fff;font-size:12px;cursor:pointer">
            + Add rule
          </button>
          <button onclick="condSave()" class="admin-only" style="padding:6px 18px;border:none;border-radius:6px;background:#1a2744;color:#fff;font-size:12px;cursor:pointer;font-weight:500">
            Save &amp; Apply
          </button>
          <span id="cond-save-msg" style="font-size:12px;padding:6px 0;color:#6b7280"></span>
        </div>

        <div style="background:#f9fafb;border:1px solid #e5e7eb;border-radius:8px;padding:12px 16px;font-size:11px;color:#6b7280;line-height:1.9">
          <strong style="color:#374151">Boolean:</strong>
          xkop.i.151 AND xkop.i.152 &nbsp;·&nbsp; NOT xkop.i.153 &nbsp;·&nbsp; virt.0 OR NOT virt.1<br>
          <strong style="color:#374151">Timers:</strong>
          ON_DELAY(sig, 5.0) &nbsp;·&nbsp; OFF_DELAY(sig, 3.0) &nbsp;·&nbsp; PULSE(sig, 1.5)<br>
          <strong style="color:#374151">Alarms:</strong>
          WATCHDOG(sig, 300) &nbsp;·&nbsp; MISSING(sig, 60)<br>
          <strong style="color:#374151">Edge:</strong>
          RISING_EDGE(sig) &nbsp;·&nbsp; FALLING_EDGE(sig)<br>
          <strong style="color:#374151">Memory:</strong>
          LATCH(set=sig, reset=sig) &nbsp;·&nbsp; TOGGLE(sig)<br>
          <strong style="color:#374151">Counters:</strong>
          COUNT_UP(sig) &nbsp;·&nbsp; COUNT_UP(sig, reset=sig) &nbsp;·&nbsp;
          COUNT_DOWN(sig, start=10) &nbsp;·&nbsp;
          COUNT(sig, n=5, reset=sig) &nbsp;·&nbsp;
          N1COUNT(a=sig, b=sig, c=sig) &nbsp;·&nbsp; FACTOR(virt.N, bit=0)<br>
          <strong style="color:#374151">Bidirectional counter:</strong>
          COUNT_UP(sig) OR COUNT_DOWN(sig)<br>
          <strong style="color:#374151">Nesting:</strong>
          PULSE(FALLING_EDGE(sig), 5.0) &nbsp;·&nbsp;
          COUNT_UP(RISING_EDGE(sig)) OR COUNT_DOWN(FALLING_EDGE(sig))<br>
          <strong style="color:#374151">Speed:</strong>
          SPEED(det1=sig, det2=sig, distance_m=3.5)<br>
          <strong style="color:#374151">Outputs:</strong>
          virt.N &nbsp;·&nbsp; xkop.o.N &nbsp;·&nbsp; rpdb.o.N
        </div>
      </div>
    </div>

    <!-- ── RTIG ── -->
    <div id="panel-rtig" class="panel">
      <div class="cards-row cards-4">
        <div class="metric-card">
          <div class="metric-label">Port</div>
          <div class="metric-value" id="rtig-port">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Rules</div>
          <div class="metric-value" id="rtig-rules">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Pulse duration</div>
          <div class="metric-value sm" id="rtig-pulse">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Total received</div>
          <div class="metric-value green" id="rtig-total">0</div>
        </div>
      </div>

      <div class="section-title">Rules</div>
      <div class="table-card" style="margin-bottom:14px">
        <table>
          <thead>
            <tr><th>#</th><th>Traffic sig</th><th>Mov</th><th>Trig</th>
                <th>Match</th><th>Send signal</th><th>State</th></tr>
          </thead>
          <tbody id="rtig-rules-tbody"></tbody>
        </table>
      </div>

      <div class="section-title admin-only">Configuration
        <button class="collapse-btn" data-target="rtig-cfg-editor" onclick="_toggleCollapse(this)">▼ Show</button>
      </div>
      <div id="rtig-cfg-editor" class="admin-only">
        <div style="font-size:12px;color:#9ca3af;margin-bottom:10px">Loading…</div>
      </div>

      <div class="section-title">Recent TLP log</div>
      <div class="table-card">
        <table>
          <thead>
            <tr><th>Time</th><th>Traffic sig</th><th>Mov</th><th>Trig</th>
                <th>Type / Value</th><th>Pulsed signal</th></tr>
          </thead>
          <tbody id="rtig-log-tbody"></tbody>
        </table>
      </div>
    </div>

    <!-- ── AGD650 ── -->
    <div id="panel-agd" class="panel">
      <div class="cards-row cards-3">
        <div class="metric-card">
          <div class="metric-label">Any detected</div>
          <div class="metric-value" id="agd-any-detected">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Any pedestrian</div>
          <div class="metric-value" id="agd-any-pedestrian">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Units</div>
          <div class="metric-value sm" id="agd-units">—</div>
        </div>
      </div>

      <div class="section-title">Zone state</div>
      <div class="table-card" style="margin-bottom:14px">
        <table>
          <thead>
            <tr><th>Zone</th><th>Detected</th><th>Car</th><th>Pedestrian</th><th>Cyclist</th><th>Bus</th><th>Lorry</th></tr>
          </thead>
          <tbody id="agd-zone-tbody"></tbody>
        </table>
      </div>

      <div class="section-title">Global detections</div>
      <div class="table-card" style="margin-bottom:14px">
        <table>
          <thead><tr><th>Type</th><th>Any zone active</th></tr></thead>
          <tbody id="agd-global-tbody"></tbody>
        </table>
      </div>

      <div class="section-title admin-only">Configuration
        <button class="collapse-btn" data-target="agd-cfg-wrap" onclick="_toggleCollapse(this)">▼ Show</button>
      </div>
      <div id="agd-cfg-wrap" class="admin-only">
        <div style="font-size:11px;color:#6b7280;font-weight:600;margin-bottom:6px">Detection Classes</div>
        <div class="table-card" style="margin-bottom:8px">
          <table style="width:100%">
            <thead><tr><th>Class Name</th><th>Signal Suffix</th><th style="width:32px"></th></tr></thead>
            <tbody id="agd-classes-tbody"></tbody>
          </table>
        </div>
        <div style="display:flex;align-items:center;gap:10px;margin-bottom:12px">
          <button class="ad-btn-save admin-only" style="background:#374151" onclick="addAGDClass()">+ Class</button>
          <button class="ad-btn-save admin-only" onclick="rebuildAGDZoneTable()">Apply Classes →</button>
        </div>

        <div style="font-size:11px;color:#6b7280;font-weight:600;margin-bottom:6px">Zone Signal Mapping</div>
        <div id="agd-mapping-container" style="overflow-x:auto"></div>
        <div style="display:flex;align-items:center;gap:12px;margin-bottom:12px">
          <button class="ad-btn-save admin-only" onclick="saveAGDMapping()">Save All</button>
          <span id="agd-mapping-msg" style="font-size:12px"></span>
          <span style="font-size:11px;color:#92400e">&#9888; Restart to apply</span>
        </div>
      </div>

      <div class="section-title" style="margin-top:4px">Recent detections</div>
      <div class="table-card">
        <table>
          <thead>
            <tr><th>Time</th><th>Unit</th><th>Zone</th><th>State</th><th>Classes</th></tr>
          </thead>
          <tbody id="agd-events-tbody"></tbody>
        </table>
      </div>
    </div>

    <!-- ── Auto Dim ── -->
    <div id="panel-autodim" class="panel">
      <div class="cards-row cards-4">
        <div class="metric-card">
          <div class="metric-label">Current state</div>
          <div class="metric-value sm" id="ad-state">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Next event</div>
          <div class="metric-value sm blue" id="ad-next-event">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Sunrise → Bright at</div>
          <div class="metric-value sm" id="ad-times-rise">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Sunset → Dim at</div>
          <div class="metric-value sm" id="ad-times-set">—</div>
        </div>
      </div>

      <div class="section-title">Configuration</div>
      <div class="ad-config-card">
        <div class="ad-form-grid" style="margin-bottom:12px">
          <div class="ad-form-group" style="grid-column:1/-1">
            <label class="ad-label">Location search (requires internet)</label>
            <div style="display:flex;gap:6px">
              <input type="text" id="ad-search" class="ad-input" style="flex:1"
                     placeholder="e.g. London or M25 J10" onkeydown="if(event.key==='Enter')adSearch()">
              <button class="ad-btn-save" onclick="adSearch()" style="white-space:nowrap">Search</button>
            </div>
            <div id="ad-search-result" style="font-size:11px;color:#6b7280;margin-top:3px"></div>
          </div>
        </div>

        <div class="ad-form-grid">
          <div class="ad-form-group">
            <label class="ad-label">Latitude</label>
            <input type="number" id="ad-lat" step="0.0001" class="ad-input"
                   placeholder="e.g. 51.5072">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Longitude</label>
            <input type="number" id="ad-lon" step="0.0001" class="ad-input"
                   placeholder="e.g. -0.1276">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Dim offset (min after sunset)</label>
            <input type="number" id="ad-dim-offset" class="ad-input"
                   min="-120" max="120" step="1" placeholder="e.g. +20">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Bright offset (min rel. sunrise)</label>
            <input type="number" id="ad-bright-offset" class="ad-input"
                   min="-120" max="120" step="1" placeholder="e.g. -30">
          </div>
          <div class="ad-form-group" style="grid-column:1/-1">
            <label class="ad-label">Output signal</label>
            <input type="text" id="ad-signal" list="ad-signal-list" class="ad-input"
                   placeholder="e.g. xkop.o.15 or rpdb.o.3">
            <datalist id="ad-signal-list"></datalist>
            <div style="font-size:11px;color:#9ca3af;margin-top:4px">
              Pick a free output bit not used in the UG405 mapping.
              Signal changes take effect after restart.
            </div>
          </div>
        </div>

        <div style="margin-top:14px;display:flex;align-items:center;gap:12px">
          <button class="ad-btn-save admin-only" onclick="adSave()">Save</button>
          <button class="ad-btn-save" style="background:#1d4ed8"
            onclick="(function(){
              const lat=document.getElementById('ad-lat').value;
              const lon=document.getElementById('ad-lon').value;
              if(lat&&lon) window.open('https://www.google.com/maps?q='+lat+','+lon,'_blank');
              else alert('No location set');
            })()">&#x1F4CD; View on Map</button>
          <span id="ad-save-msg" style="font-size:12px;color:#15803d"></span>
        </div>
      </div>
    </div>

    <!-- ── Fixed-Time Plan ── -->
    <div id="panel-offplan" class="panel">
      <div class="cards-row cards-4">
        <div class="metric-card">
          <div class="metric-label">Service status</div>
          <div class="metric-value sm" id="op-status">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Time mode</div>
          <div class="metric-value sm" id="op-time-mode">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Base time</div>
          <div class="metric-value sm" id="op-base-time">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Active in</div>
          <div class="metric-value sm" id="op-active-modes">—</div>
        </div>
      </div>

      <div id="op-scn-container"></div>

      <div class="section-title" style="margin-bottom:8px">Timetable</div>
      <div id="op-timetable-container"></div>

      <div class="section-title" style="margin-bottom:8px">Plan definitions</div>
      <div id="op-plans-container"></div>

      <div class="section-title admin-only">Plan Configuration Editor
        <button class="collapse-btn" data-target="op-plan-editor" onclick="_toggleCollapse(this)">▼ Show</button>
      </div>
      <div id="op-plan-editor" class="admin-only"></div>
    </div>

    <!-- ── FLIR ── -->
    <div id="panel-flir" class="panel">
      <div class="cards-row cards-3">
        <div class="metric-card">
          <div class="metric-label">Any occupied</div>
          <div class="metric-value" id="flir-any-occupied">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Any dilemma zone</div>
          <div class="metric-value red" id="flir-any-dilemma">—</div>
        </div>
        <div class="metric-card">
          <div class="metric-label">Camera / mode</div>
          <div class="metric-value sm" id="flir-mode">—</div>
        </div>
      </div>

      <div class="section-title">Zone state</div>
      <div class="table-card" style="margin-bottom:14px">
        <table>
          <thead>
            <tr>
              <th>Zone</th>
              <th>Occupied</th>
              <th>Dilemma</th>
              <th>Pedestrian</th>
              <th>Bicycle</th>
              <th>Vehicle</th>
            </tr>
          </thead>
          <tbody id="flir-zone-tbody"></tbody>
        </table>
      </div>

      <div class="section-title">Global detections</div>
      <div class="table-card" style="margin-bottom:14px">
        <table>
          <thead><tr><th>Type</th><th>Any zone active</th></tr></thead>
          <tbody id="flir-global-tbody"></tbody>
        </table>
      </div>

      <div class="section-title admin-only">Zone Signal Mapping
        <button class="collapse-btn" data-target="flir-map-wrap" onclick="_toggleCollapse(this)">▼ Show</button>
      </div>
      <div id="flir-map-wrap" class="admin-only">
        <div class="table-card" style="margin-bottom:8px;overflow-x:auto">
          <table id="flir-mapping-table" style="width:100%">
            <thead id="flir-mapping-thead"></thead>
            <tbody id="flir-mapping-tbody"></tbody>
          </table>
        </div>
        <div style="display:flex;align-items:center;gap:12px;margin-bottom:12px">
          <button class="ad-btn-save admin-only" onclick="saveFLIRMapping()">Save Mapping</button>
          <span id="flir-mapping-msg" style="font-size:12px"></span>
          <span style="font-size:11px;color:#92400e">&#9888; Restart to apply</span>
        </div>
      </div>

      <div class="section-title" style="margin-top:4px">Recent events</div>
      <div class="table-card">
        <table>
          <thead>
            <tr><th>Time</th><th>Camera</th><th>Zone</th><th>Type</th><th>State</th><th>Class</th></tr>
          </thead>
          <tbody id="flir-events-tbody"></tbody>
        </table>
      </div>
    </div>

    <!-- ── Configuration ── -->
    <div id="panel-config" class="panel">
      <div class="section-title" style="display:none" id="cfg-readonly-msg">Configuration is read-only for your account.</div>
      <div class="admin-only">
        <div class="cfg-section-title">Services</div>
        <div class="cfg-services-grid">
          <div class="cfg-service-row">
            <span class="badge badge-green" style="font-size:10px;flex-shrink:0">Always on</span>
            <span class="cfg-service-label">IO Bus</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-ug405"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">UG405</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-xkop"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">XKOP</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-rtig"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">RTIG</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-conditioner"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">Conditioner</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-rpdb"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">RPDB</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-flir"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">FLIR</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-agd"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">AGD650</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-autodim"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">DIM Service</span>
          </div>
          <div class="cfg-service-row">
            <label class="cfg-toggle"><input type="checkbox" id="svc-offline_plan"><span class="cfg-slider"></span></label>
            <span class="cfg-service-label">Fixed-Time Plan</span>
          </div>
        </div>
      </div>

      <div class="cfg-section">
        <div class="cfg-section-title">XKOP Connection</div>
        <div class="cfg-form-grid-3">
          <div class="ad-form-group">
            <label class="ad-label">IP Address</label>
            <input type="text" id="cfg-xkop-ip" class="ad-input" placeholder="192.168.71.101">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Port</label>
            <input type="number" id="cfg-xkop-port" class="ad-input" min="1" max="65535">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Mode</label>
            <select id="cfg-xkop-mode" class="ad-input">
              <option value="client">Client (CM5 connects out)</option>
              <option value="server">Server (CM5 listens)</option>
            </select>
          </div>
        </div>
      </div>

      <div class="cfg-section">
        <div class="cfg-section-title">RPDB Connection</div>
        <div class="cfg-form-grid-3" style="margin-bottom:10px">
          <div class="ad-form-group">
            <label class="ad-label">IP Address</label>
            <input type="text" id="cfg-rpdb-host" class="ad-input" placeholder="192.168.71.101">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Port</label>
            <input type="number" id="cfg-rpdb-port" class="ad-input" min="1" max="65535" placeholder="35340">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Username</label>
            <input type="text" id="cfg-rpdb-username" class="ad-input" autocomplete="off">
          </div>
        </div>
        <div class="ad-form-grid">
          <div class="ad-form-group">
            <label class="ad-label">Password</label>
            <input type="password" id="cfg-rpdb-password" class="ad-input" autocomplete="off">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Write Password</label>
            <input type="password" id="cfg-rpdb-write-password" class="ad-input" autocomplete="off">
          </div>
        </div>
      </div>

      <div class="cfg-section">
        <div class="cfg-section-title">Platform Limits</div>
        <div class="ad-form-grid">
          <div class="ad-form-group">
            <label class="ad-label">IO Fault Timeout (s) — 0 = immediate standalone drop</label>
            <input type="number" id="cfg-fault-timeout" class="ad-input" min="0" max="300">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Memory Watchdog Limit (MB)</label>
            <input type="number" id="cfg-mem-limit" class="ad-input" min="100" max="4096">
          </div>
        </div>
      </div>

      <div class="cfg-section">
        <div class="cfg-section-title">Logging</div>
        <div class="cfg-form-grid-3">
          <div class="ad-form-group">
            <label class="ad-label">Log Level</label>
            <select id="cfg-log-level" class="ad-input">
              <option value="DEBUG">DEBUG</option>
              <option value="INFO">INFO</option>
              <option value="WARNING">WARNING</option>
              <option value="ERROR">ERROR</option>
            </select>
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Max File Size (MB)</label>
            <input type="number" id="cfg-log-max-mb" class="ad-input" min="1" max="100">
          </div>
          <div class="ad-form-group">
            <label class="ad-label">Rotated Files to Keep</label>
            <input type="number" id="cfg-log-keep" class="ad-input" min="1" max="20">
          </div>
        </div>
      </div>

      <div class="cfg-section">
        <div class="cfg-section-title">FLIR Cameras</div>
        <div class="table-card" style="margin-bottom:10px">
          <table style="width:100%">
            <thead><tr>
              <th style="width:36px">#</th>
              <th>IP Address</th>
              <th style="width:90px">Port</th>
              <th>Zones</th>
              <th style="width:32px"></th>
            </tr></thead>
            <tbody id="flir-cameras-tbody"></tbody>
          </table>
        </div>
        <div style="display:flex;align-items:center;gap:10px">
          <button class="ad-btn-save admin-only" style="background:#374151"
                  onclick="addSensorRow('flir-cameras-tbody','flir',8765)">+ Camera</button>
          <button class="ad-btn-save admin-only" onclick="saveSensorCfg('flir')">Save FLIR</button>
          <span id="flir-save-msg" style="font-size:12px"></span>
        </div>
      </div>

      <div class="cfg-section">
        <div class="cfg-section-title">AGD650 Units</div>
        <div class="table-card" style="margin-bottom:10px">
          <table style="width:100%">
            <thead><tr>
              <th style="width:36px">#</th>
              <th>IP Address</th>
              <th style="width:90px">Port</th>
              <th>Zones</th>
              <th style="width:32px"></th>
            </tr></thead>
            <tbody id="agd-units-tbody"></tbody>
          </table>
        </div>
        <div style="display:flex;align-items:center;gap:10px">
          <button class="ad-btn-save admin-only" style="background:#374151"
                  onclick="addSensorRow('agd-units-tbody','agd',9003)">+ Unit</button>
          <button class="ad-btn-save admin-only" onclick="saveSensorCfg('agd')">Save AGD</button>
          <span id="agd-save-msg" style="font-size:12px"></span>
        </div>
      </div>

      <div class="cfg-section">
        <div style="display:flex;align-items:center;gap:12px;margin-bottom:12px">
          <button class="ad-btn-save admin-only" onclick="savePlatformCfg()">Save</button>
          <span id="cfg-save-msg" style="font-size:12px"></span>
        </div>
        <div style="background:#fef3c7;border:1px solid #f59e0b;border-radius:6px;
                    padding:10px 14px;margin-bottom:12px;font-size:12px;color:#92400e">
          &#9888; All changes require a CM5 restart to take effect.
        </div>
        <div style="display:flex;align-items:center;gap:12px">
          <button class="cfg-btn-restart admin-only" id="cfg-restart-btn" onclick="restartCm5()">Restart CM5</button>
          <span id="cfg-restart-msg" style="font-size:12px"></span>
        </div>
      </div>
      </div><!-- /admin-only config wrap -->
    </div>

    <!-- ── Logs ── -->
    <!-- ── SCN Config ── -->
    <div id="panel-scncfg" class="panel">
      <div class="section-title" style="display:none" id="scncfg-readonly-msg">SCN configuration is read-only for your account.</div>
      <div class="admin-only">
      <div id="scncfg-container"></div>

      <button class="ad-btn-save admin-only" style="background:#374151;margin-bottom:12px"
              onclick="addSCN()">+ Add SCN</button>

      <div id="scncfg-conflicts" style="margin-bottom:10px;font-size:12px;color:#b91c1c"></div>

      <div style="background:#fef3c7;border:1px solid #f59e0b;border-radius:8px;
                  padding:12px 16px;margin-bottom:12px;font-size:12px">
        <strong>Apply &amp; Restart</strong> — saves ug405.cfg and immediately restarts CM5.
        Type <code>CONFIRM</code> to enable.
        <div style="display:flex;align-items:center;gap:10px;margin-top:8px">
          <input type="text" id="scncfg-confirm" class="ad-input" placeholder="Type CONFIRM"
                 style="width:160px" oninput="document.getElementById('scncfg-apply-btn').disabled=this.value!=='CONFIRM'">
          <button id="scncfg-apply-btn" class="cfg-btn-restart admin-only" disabled
                  onclick="applySCNConfig()">Apply &amp; Restart</button>
          <span id="scncfg-msg" style="font-size:12px"></span>
        </div>
      </div>
      </div><!-- /admin-only scncfg wrap -->
    </div>

    <div id="panel-logs" class="panel">
      <div class="log-toolbar">
        <select id="log-file-select" class="log-select" onchange="loadLog();updateLogDownload()">
          <option value="">— select log file —</option>
        </select>
        <select id="log-lines-select" class="log-select" style="min-width:0;width:100px" onchange="loadLog()">
          <option value="2000">2 000 lines</option>
          <option value="5000">5 000 lines</option>
          <option value="10000">10 000 lines</option>
        </select>
        <button class="log-lvl-btn l-debug active" onclick="logToggleLevel('DEBUG',this)">DEBUG</button>
        <button class="log-lvl-btn l-info  active" onclick="logToggleLevel('INFO',this)">INFO</button>
        <button class="log-lvl-btn l-warning active" onclick="logToggleLevel('WARNING',this)">WARN</button>
        <button class="log-lvl-btn l-error active" onclick="logToggleLevel('ERROR',this)">ERROR</button>
        <input type="text" id="log-search" class="log-search" placeholder="Search logs…" oninput="renderLog()">
        <a id="log-download-btn" href="#" download
           style="padding:4px 10px;border:1px solid #d1d5db;border-radius:5px;font-size:11px;
                  font-weight:600;background:#f3f4f6;color:#374151;text-decoration:none;
                  display:none">&#8659; Download raw</a>
        <span id="log-line-count" style="font-size:11px;color:#6b7280;margin-left:4px"></span>
      </div>
      <div class="log-viewer" id="log-viewer">
        <div style="padding:14px;color:#9ca3af;font-size:12px">Select a log file above.</div>
      </div>
    </div>

    <!-- ── Users (admin only) ── -->
    {% if is_admin %}
    <div id="panel-users" class="panel">
      <h2 style="font-size:14px;font-weight:600;color:#1a2744;margin-bottom:14px">User Management</h2>

      <!-- Site name -->
      <div style="background:#fff;border:1px solid #e5e7eb;border-radius:8px;padding:16px;margin-bottom:16px">
        <h3 style="font-size:13px;font-weight:600;color:#374151;margin-bottom:10px">Site Name</h3>
        <p style="font-size:11px;color:#6b7280;margin-bottom:8px">Displayed on the login page as a subtitle.</p>
        <div style="display:flex;gap:8px;align-items:center">
          <input type="text" id="site-name-input" placeholder="e.g. J0331 Thorncliffe Park"
                 style="padding:6px 10px;border:1px solid #d1d5db;border-radius:5px;font-size:12px;width:280px">
          <button onclick="saveSiteName()" style="padding:6px 16px;background:#1a2744;color:#fff;border:none;border-radius:5px;font-size:12px;cursor:pointer;font-weight:500">Save</button>
        </div>
        <div id="sn-msg" style="margin-top:6px;font-size:12px"></div>
      </div>

      <!-- User table -->
      <div style="background:#fff;border:1px solid #e5e7eb;border-radius:8px;overflow:hidden;margin-bottom:20px">
        <table style="width:100%;border-collapse:collapse;font-size:12px" id="users-table">
          <thead>
            <tr style="background:#f9fafb;border-bottom:1px solid #e5e7eb">
              <th style="padding:8px 12px;text-align:left;color:#6b7280;font-weight:500">Username</th>
              <th style="padding:8px 12px;text-align:left;color:#6b7280;font-weight:500">Role</th>
              <th style="padding:8px 12px;text-align:left;color:#6b7280;font-weight:500">Last login</th>
              <th style="padding:8px 12px;text-align:left;color:#6b7280;font-weight:500">Status</th>
              <th style="padding:8px 12px;text-align:left;color:#6b7280;font-weight:500">Actions</th>
            </tr>
          </thead>
          <tbody id="users-tbody">
            <tr><td colspan="5" style="padding:12px;color:#9ca3af;text-align:center">Loading…</td></tr>
          </tbody>
        </table>
      </div>

      <!-- Add user form -->
      <div style="background:#fff;border:1px solid #e5e7eb;border-radius:8px;padding:16px">
        <h3 style="font-size:13px;font-weight:600;color:#374151;margin-bottom:12px">Add User</h3>
        <div style="display:flex;gap:10px;flex-wrap:wrap;align-items:flex-end">
          <div>
            <label style="display:block;font-size:11px;color:#6b7280;margin-bottom:3px">Username</label>
            <input type="text" id="nu-username" style="padding:6px 10px;border:1px solid #d1d5db;border-radius:5px;font-size:12px;width:140px">
          </div>
          <div>
            <label style="display:block;font-size:11px;color:#6b7280;margin-bottom:3px">Password</label>
            <input type="password" id="nu-password" style="padding:6px 10px;border:1px solid #d1d5db;border-radius:5px;font-size:12px;width:140px">
          </div>
          <div>
            <label style="display:block;font-size:11px;color:#6b7280;margin-bottom:3px">Role</label>
            <select id="nu-role" style="padding:6px 10px;border:1px solid #d1d5db;border-radius:5px;font-size:12px">
              <option value="viewer">viewer</option>
              <option value="admin">admin</option>
            </select>
          </div>
          <button onclick="addUser()" style="padding:6px 16px;background:#1a2744;color:#fff;border:none;border-radius:5px;font-size:12px;cursor:pointer;font-weight:500">Add User</button>
        </div>
        <div id="nu-msg" style="margin-top:8px;font-size:12px"></div>
      </div>

      <!-- Change password modal (inline) -->
      <div id="pw-modal" style="display:none;margin-top:16px;background:#fff;border:1px solid #e5e7eb;border-radius:8px;padding:16px">
        <h3 style="font-size:13px;font-weight:600;color:#374151;margin-bottom:12px">Set Password — <span id="pw-modal-name"></span></h3>
        <div style="display:flex;gap:10px;align-items:flex-end">
          <div>
            <label style="display:block;font-size:11px;color:#6b7280;margin-bottom:3px">New Password</label>
            <input type="password" id="pw-new" style="padding:6px 10px;border:1px solid #d1d5db;border-radius:5px;font-size:12px;width:160px">
          </div>
          <button onclick="submitPwChange()" style="padding:6px 16px;background:#1a2744;color:#fff;border:none;border-radius:5px;font-size:12px;cursor:pointer">Set Password</button>
          <button onclick="document.getElementById('pw-modal').style.display='none'" style="padding:6px 12px;border:1px solid #d1d5db;border-radius:5px;font-size:12px;background:#fff;cursor:pointer">Cancel</button>
        </div>
        <div id="pw-msg" style="margin-top:8px;font-size:12px"></div>
      </div>
    </div>
    {% endif %}

    {% for n in range(mova_stream_count) %}
    <!-- ── MOVA Stream {{ n+1 }} ── -->
    <div id="panel-mova-{{n}}" class="panel" style="padding:0;background:#f0f2f5;border:none">
      <div class="mova-card" id="mova-card-{{n}}">

        <!-- Card header -->
        <div class="mova-card-hdr">
          <div style="display:flex;align-items:center;gap:8px">
            <span class="mova-card-title">STREAM {{n+1}}</span>
            <span style="font-family:'Courier New',monospace;font-size:10px;color:rgba(255,255,255,.45)">DATASET:&nbsp;<span id="mova-ds-title-{{n}}" style="color:rgba(255,255,255,.8)">—</span></span>
          </div>
          <div style="display:flex;align-items:center;gap:10px">
            <span id="mova-kver-{{n}}" style="font-family:'Courier New',monospace;font-size:9px;color:rgba(255,255,255,.35)"></span>
            <span class="mova-status-badge mova-badge-off" id="mova-badge-{{n}}">—</span>
          </div>
        </div>

        <!-- Status row 1: CRB / ON_CONTROL / MOVA_ENABLED / ERROR_COUNT / WARMUP -->
        <div class="mova-status-row">
          <div class="mova-status-cell">
            <div class="mova-slabel">CRB</div>
            <div class="mova-sval off" id="mova-crb-val-{{n}}"><div class="mova-crb-dot" id="mova-crb-dot-{{n}}"></div>NOT READY</div>
          </div>
          <div class="mova-status-cell">
            <div class="mova-slabel">On Control</div>
            <div class="mova-sval off" id="mova-onctrl-{{n}}">NO</div>
          </div>
          <div class="mova-status-cell">
            <div class="mova-slabel">MOVA Enabled</div>
            <div class="mova-sval off" id="mova-en-{{n}}">NO</div>
          </div>
          <div class="mova-status-cell">
            <div class="mova-slabel">Error Count</div>
            <div class="mova-sval" id="mova-ec-{{n}}">0</div>
          </div>
          <div class="mova-status-cell" style="flex:1">
            <div class="mova-slabel">Warmup</div>
            <div class="mova-sval off" id="mova-wc-{{n}}">—</div>
          </div>
        </div>

        <!-- Status row 2: kernel diagnostics -->
        <div class="mova-status-row" style="font-size:.82em">
          <div class="mova-status-cell"><div class="mova-slabel" title="Programme Marker">PM</div><div class="mova-sval" id="mova-pm-{{n}}">—</div></div>
          <div class="mova-status-cell"><div class="mova-slabel" title="Current Stage">CS</div><div class="mova-sval" id="mova-cs-{{n}}">—</div></div>
          <div class="mova-status-cell"><div class="mova-slabel" title="Demanded Stage">DS</div><div class="mova-sval" id="mova-ds-{{n}}">—</div></div>
          <div class="mova-status-cell"><div class="mova-slabel" title="Next Stage">NS</div><div class="mova-sval" id="mova-ns-{{n}}">—</div></div>
          <div class="mova-status-cell"><div class="mova-slabel" title="Wait Stage Change Timer (tenths/s)">WaitT</div><div class="mova-sval" id="mova-wt-{{n}}">—</div></div>
          <div class="mova-status-cell"><div class="mova-slabel" title="Kernel in Warmup">WU</div><div class="mova-sval" id="mova-wu-{{n}}">—</div></div>
          <div class="mova-status-cell"><div class="mova-slabel" title="CI_start_scan count">SCAN</div><div class="mova-sval" id="mova-scan-{{n}}">—</div></div>
          <div class="mova-status-cell"><div class="mova-slabel">TIME</div><div class="mova-sval" id="mova-time-{{n}}" style="font-family:'Courier New',monospace">—</div></div>
          <div class="mova-status-cell" style="flex:1"><div class="mova-slabel">DATE</div><div class="mova-sval" id="mova-date-{{n}}" style="font-family:'Courier New',monospace">—</div></div>
        </div>

        <!-- Detectors -->
        <div class="mova-sec-hdr">Detectors</div>
        <div class="mova-sec-body"><div class="mova-bit-grid" id="mova-dets-{{n}}"></div></div>

        <!-- Confirms -->
        <div class="mova-sec-hdr">Confirms</div>
        <div class="mova-sec-body"><div class="mova-bit-grid" id="mova-confs-{{n}}"></div></div>

        <!-- Forces -->
        <div class="mova-sec-hdr">Force Bits</div>
        <div class="mova-sec-body">
          <div class="mova-bit-grid">
            <div class="mova-bit-group" id="mova-forces-{{n}}"></div>
            <div class="mova-bit-group" style="margin-left:14px;border-left:1px solid #e5e7eb;padding-left:14px">
              <div class="mova-bit-cell"><div class="mova-bit-num">HI</div><div class="mova-bit-dot" id="mova-hi-{{n}}"></div></div>
              <div class="mova-bit-cell"><div class="mova-bit-num">TO</div><div class="mova-bit-dot" id="mova-to-{{n}}"></div></div>
              <div class="mova-bit-cell"><div class="mova-bit-num">SYNC</div><div class="mova-bit-dot cyan" id="mova-sync-{{n}}"></div></div>
              <div class="mova-bit-cell"><div class="mova-bit-num">FLT</div><div class="mova-bit-dot" id="mova-flt-{{n}}"></div></div>
            </div>
          </div>
        </div>

        <!-- Faults -->
        <div id="mova-faults-{{n}}"></div>

        <!-- I/O Map editor (hidden) -->
        <div id="mova-iomap-{{n}}" style="display:none">
          <div class="mova-sec-hdr" style="display:flex;justify-content:space-between;align-items:center">
            <span>Signal Map — Stream {{n+1}}</span>
            <div style="display:flex;gap:6px">
              <button class="mova-btn admin-only" onclick="movaSaveIOMap({{n}})" style="background:#2563eb;color:#fff;padding:2px 10px">Save</button>
              <button class="mova-btn" onclick="document.getElementById('mova-iomap-{{n}}').style.display='none'" style="padding:2px 8px">✕</button>
            </div>
          </div>
          <div class="mova-sec-body" id="mova-iomap-content-{{n}}"></div>
        </div>

        <!-- Dataset loader (hidden) -->
        <div id="mova-dataset-loader-{{n}}" style="display:none">
          <div class="mova-sec-hdr">Load Dataset — Stream {{n+1}}</div>
          <div class="mova-sec-body">
            <div id="mova-ds-list-{{n}}" style="margin-bottom:8px"></div>
            <input type="file" accept=".mxds" onchange="movaUploadDataset({{n}},this)" style="font-size:11px">
          </div>
        </div>

        <!-- Controls row -->
        <div class="mova-ctrl-row">
          <button class="mova-btn mova-on-btn admin-only" id="mova-btn-on-{{n}}" onclick="movaCmdOn({{n}})">⚡ MOVA ON</button>
          <button class="mova-btn mova-reset-btn admin-only" onclick="movaCmdReset({{n}})">Reset</button>
          <button class="mova-btn" onclick="movaShowDatasetLoader({{n}})">Dataset</button>
          <button class="mova-btn" onclick="movaToggleIOMap({{n}})">I/O Map</button>
          <button class="mova-btn mova-log-btn" onclick="movaPopout({{n}})">⬡ Pop out</button>
        </div>
      </div>
    </div>
    {% endfor %}

  </div>
</div>

<script>
if (window.CM5_ROLE !== 'admin') {
  document.body.classList.add('viewer-mode');
  ['cfg-readonly-msg','scncfg-readonly-msg'].forEach(id => {
    const el = document.getElementById(id); if (el) el.style.display = '';
  });
}

const OPMODE_LABELS  = {1:'1 — Standalone', 2:'2 — Monitor', 3:'3 — UTC Control'};
const OPMODE_CLASSES = {1:'red', 2:'amber', 3:'green'};
const PRIO_LABELS    = {1:'High', 2:'Default', 3:'Low'};
const PRIO_CLASSES   = {1:'prio-1', 2:'prio-2', 3:'prio-3'};

// ── Collapsible sections ──────────────────────────────────────────────────────
function _toggleCollapse(btn) {
  const id  = btn.dataset.target;
  const el  = document.getElementById(id);
  if (!el) return;
  const show = el.style.display === 'none';
  el.style.display = show ? '' : 'none';
  btn.textContent  = show ? '▲ Hide' : '▼ Show';
  try { sessionStorage.setItem('col_' + id, show ? '0' : '1'); } catch(e) {}
}

function _initCollapses() {
  document.querySelectorAll('.collapse-btn[data-target]').forEach(btn => {
    const id = btn.dataset.target;
    const el = document.getElementById(id);
    if (!el) return;
    let collapsed = true;
    try { const s = sessionStorage.getItem('col_' + id); if (s !== null) collapsed = s === '1'; } catch(e) {}
    el.style.display = collapsed ? 'none' : '';
    btn.textContent  = collapsed ? '▼ Show' : '▲ Hide';
  });
}

document.addEventListener('DOMContentLoaded', () => { _initCollapses(); renderOpModeTransitions(); });

// ── State ────────────────────────────────────────────────────────────────────
let state = {
  io_signals: [],
  cond_rules: [],
  rtig: null,
  mapping: {{ mapping_json | safe }},
  opmode: 1,
  offplanActive: false,
  cfg_state:          {},
  opmode_transitions: {},
  ctrl_log:           [],
};

// ── Nav ──────────────────────────────────────────────────────────────────────
function show(name, el) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
  document.getElementById('panel-' + name).classList.add('active');
  el.classList.add('active');
}

// ── UG405 ─────────────────────────────────────────────────────────────────────
function buildUG405Tables() {
  const m = state.mapping;
  if (!m || !m.scns) return;
  document.getElementById('ug-scns').textContent = m.scns.length;
  document.getElementById('site-name').textContent = m.scns.join(' · ');
  const container = document.getElementById('ug-scn-container');
  container.innerHTML = '';
  m.scns.forEach(scn => {
    const block = document.createElement('div');
    block.style.marginBottom = '16px';
    block.innerHTML = `
      <div class="section-title">SCN: ${scn}</div>
      <div class="row2">
        <div>
          <div style="font-size:11px;color:#6b7280;margin-bottom:6px">Control</div>
          <div class="table-card"><table>
            <thead><tr><th>Field</th><th>Signal</th><th>Value</th></tr></thead>
            <tbody>${buildSCNRows(m.control[scn], 'ctrl')}</tbody>
          </table></div>
        </div>
        <div>
          <div style="font-size:11px;color:#6b7280;margin-bottom:6px">Reply</div>
          <div class="table-card"><table>
            <thead><tr><th>Field</th><th>Signal</th><th>Value</th></tr></thead>
            <tbody>${buildSCNRows(m.reply[scn], 'reply')}</tbody>
          </table></div>
        </div>
      </div>`;
    container.appendChild(block);
  });
}

function buildSCNRows(fields, side) {
  if (!fields) return '';
  let rows = '';
  for (const [field, bits] of Object.entries(fields)) {
    for (const [bit, sig] of Object.entries(bits)) {
      const id = `${side}-${sig.replace(/\./g,'_')}`;
      rows += `<tr>
        <td>${field}${bit !== '0' ? '['+bit+']' : ''}</td>
        <td class="mono">${sig}</td>
        <td id="${id}" class="val-off">0</td>
      </tr>`;
    }
  }
  return rows;
}

// ── Driver status ────────────────────────────────────────────────────────────
function renderDriverStatus(ds) {
  const row = document.getElementById('driver-status-row');
  const tbody = document.getElementById('driver-status-body');
  if (!ds || Object.keys(ds).length === 0) { row.style.display = 'none'; return; }
  row.style.display = '';
  let html = '';
  for (const [name, d] of Object.entries(ds)) {
    const endpoint = d.ip ? `${d.ip}:${d.port}` : `${d.host}:${d.port}`;
    let statusTxt, statusCls;
    if (d.connected) {
      statusTxt = 'Connected'; statusCls = 'val-on';
    } else if (d.fail_count === 0) {
      statusTxt = 'Connecting…'; statusCls = '';
    } else {
      const next = d.backoff > 60 ? `${d.backoff}s` : `${d.backoff}s`;
      statusTxt = `Retrying in ${next} (${d.fail_count} fail${d.fail_count!==1?'s':''})`;
      statusCls = 'val-off';
    }
    html += `<tr>
      <td style="font-weight:600">${name.toUpperCase()}</td>
      <td style="font-family:monospace">${endpoint}</td>
      <td class="${statusCls}">${statusTxt}</td>
      <td><button onclick="driverReconnect('${name}')" class="admin-only" style="padding:2px 10px;font-size:11px">Reconnect</button></td>
    </tr>`;
  }
  tbody.innerHTML = html;
}

function driverReconnect(name) {
  fetch(`/api/driver/${name}/reconnect`, {method:'POST'})
    .then(r => r.json())
    .then(d => { if (!d.ok) alert('Reconnect failed: ' + (d.error||'unknown')); })
    .catch(e => alert('Request failed: ' + e));
}

// ── IO Bus ───────────────────────────────────────────────────────────────────
function renderIOTable() {
  const filter  = document.getElementById('io-filter').value.toLowerCase();
  const showAll = document.getElementById('io-show').value === 'all';
  const tbody   = document.getElementById('io-tbody');
  let rows = '';
  let nonzero = 0;
  let inputs = 0, outputs = 0;
  state.io_signals.forEach(([name, owner, val]) => {
    if (val !== 0) nonzero++;
    if (name.includes('.i.')) inputs++;
    if (name.includes('.o.')) outputs++;
    if (filter && !name.includes(filter)) return;
    if (!showAll && val === 0) return;
    const cls = val !== 0 ? 'val-on' : 'val-off';
    rows += `<tr>
      <td class="mono">${name}</td>
      <td style="color:#6b7280">${owner}</td>
      <td class="${cls}">${val}</td>
    </tr>`;
  });
  tbody.innerHTML = rows || '<tr><td colspan="3" style="color:#9ca3af;text-align:center;padding:12px">No signals match</td></tr>';
  document.getElementById('io-total').textContent   = state.io_signals.length;
  document.getElementById('io-nonzero').textContent = nonzero;
  document.getElementById('io-inputs').textContent  = inputs;
  document.getElementById('io-outputs').textContent = outputs;
}

// ── Conditioner ──────────────────────────────────────────────────────────────
function renderConditioner() {
  const rules  = state.cond_rules;
  const tbody  = document.getElementById('cond-tbody');
  let active = 0;
  let rows = '';
  rules.forEach(r => {
    const val = r.value || 0;
    if (val !== 0) active++;
    const cls = val !== 0 ? 'val-on' : 'val-off';
    rows += `<tr>
      <td class="mono">${r.writer}</td>
      <td class="mono" style="color:#374151">${r.expression}</td>
      <td class="${cls}">${val}</td>
    </tr>`;
  });
  tbody.innerHTML = rows || '<tr><td colspan="3" style="color:#9ca3af;padding:12px">No rules loaded</td></tr>';
  document.getElementById('cond-rules').textContent   = rules.length;
  document.getElementById('cond-outputs').textContent = rules.length;
  document.getElementById('cond-active').textContent  = active;
}

// ── RTIG ─────────────────────────────────────────────────────────────────────
function renderRTIG(rtig) {
  if (!rtig) return;
  document.getElementById('rtig-port').textContent  = rtig.port;
  document.getElementById('rtig-rules').textContent = rtig.rules_count;
  document.getElementById('rtig-pulse').textContent = rtig.pulse_secs + 's';
  document.getElementById('rtig-total').textContent = rtig.total_rx;

  // Rules table
  const rtbody = document.getElementById('rtig-rules-tbody');
  const pulsing = new Set(rtig.pulsing || []);
  rtbody.innerHTML = rtig.rules.map((r, i) => {
    const sig    = r.signal;
    const isPuls = pulsing.has(sig);
    const state  = isPuls
      ? '<span class="badge badge-amber">pulsing</span>'
      : '<span class="badge badge-gray">idle</span>';
    const match  = buildRuleMatch(r);
    return `<tr>
      <td>${i+1}</td>
      <td class="mono">${r.traffic_signal !== undefined ? r.traffic_signal : '*'}</td>
      <td>${r.movement !== undefined ? r.movement : '*'}</td>
      <td>${r.trigger_point !== undefined ? r.trigger_point : '*'}</td>
      <td style="font-size:11px;color:#374151">${match}</td>
      <td class="mono" style="color:#1d4ed8">${sig}</td>
      <td>${state}</td>
    </tr>`;
  }).join('');

  // TLP log
  const ltbody = document.getElementById('rtig-log-tbody');
  ltbody.innerHTML = (rtig.tlp_log || []).map(t => {
    const typeStr = t.priority !== null
      ? `Prio ${t.priority}`
      : t.schedule_deviation !== null
        ? `SchDev ${t.schedule_deviation}`
        : '—';
    const result = t.matched && t.matched.length
      ? `<span class="log-matched">→ ${t.matched.join(', ')}</span>`
      : '<span class="log-none">no match</span>';
    const pulsed = t.matched && t.matched.length
      ? t.matched.map(s=>`<span class="mono log-matched">${s}</span>`).join(', ')
      : '<span class="log-none">—</span>';
    return `<tr>
      <td class="log-ts">${t.ts}</td>
      <td class="mono">${t.traffic_signal}</td>
      <td>${t.movement}</td>
      <td>${t.trigger_point}</td>
      <td>${typeStr}</td>
      <td>${pulsed}</td>
    </tr>`;
  }).join('') || '<tr><td colspan="6" style="color:#9ca3af;text-align:center;padding:12px">No TLPs received yet</td></tr>';
}

function buildRuleMatch(r) {
  const parts = [];
  if (r.priority !== undefined) {
    if (typeof r.priority === 'object')
      parts.push(`Prio ${r.priority.op} ${r.priority.val}`);
    else
      parts.push(`Prio=${r.priority}`);
  }
  if (r.schedule_deviation !== undefined) {
    if (typeof r.schedule_deviation === 'object')
      parts.push(`SchDev ${r.schedule_deviation.op} ${r.schedule_deviation.val}`);
    else
      parts.push(`SchDev=${r.schedule_deviation}`);
  }
  return parts.join(', ') || '*';
}

// ── SSE update handler ────────────────────────────────────────────────────────
function _paintOpMode() {
  const el = document.getElementById('ug-opmode');
  if (!el) return;
  let label = OPMODE_LABELS[state.opmode] || ('Mode ' + state.opmode);
  if (state.offplanActive && state.opmode < 3) label += ' [Offline Plan Running]';
  el.textContent = label;
  el.className = 'metric-value sm ' + (OPMODE_CLASSES[state.opmode] || '');
}

function applyUpdate(data) {
  if (data.opmode !== undefined) {
    state.opmode = data.opmode;
    _paintOpMode();
  }
  if (data.instation !== undefined)
    document.getElementById('ug-instation').textContent = data.instation;
  if (data.lastupdate !== undefined)
    document.getElementById('ug-lastupdate').textContent = data.lastupdate;

  if (data.io_signals !== undefined) {
    state.io_signals = data.io_signals;
    renderIOTable();
  }
  if (data.cond_rules !== undefined) {
    state.cond_rules = data.cond_rules;
    renderConditioner();
  }
  if (data.rtig !== undefined) {
    state.rtig = data.rtig;
    renderRTIG(data.rtig);
  }
  if (data.flir !== undefined) {
    renderFLIR(data.flir);
  }
  if (data.agd !== undefined) {
    renderAGD(data.agd);
  }
  if (data.autodim !== undefined) {
    renderAutoDim(data.autodim);
  }
  if (data.offplan !== undefined) {
    state.offplanActive = Object.values(data.offplan.scns || {}).some(s => s.active_plan);
    _paintOpMode();
    renderOffplan(data.offplan);
  }
  if (data.cfg_state !== undefined) {
    state.cfg_state = data.cfg_state;
    renderCfgState();
  }
  if (data.opmode_transitions !== undefined) {
    state.opmode_transitions = data.opmode_transitions;
    renderOpModeTransitions();
  }
  if (data.ctrl_log !== undefined) {
    state.ctrl_log = data.ctrl_log;
    renderCtrlLog();
  }
  if (data.ctrl_log_entry !== undefined) {
    state.ctrl_log.unshift(data.ctrl_log_entry);
    if (state.ctrl_log.length > 30) state.ctrl_log.length = 30;
    renderCtrlLog();
  }
  if (data.changes) {
    data.changes.forEach(([id, val]) => {
      const el = document.getElementById(id);
      if (el) { el.textContent = val; el.className = val > 0 ? 'val-on' : 'val-off'; }
    });
  }
  if (data.status_pills) {
    document.getElementById('status-pills').innerHTML = data.status_pills;
  }
  if (data.driver_status !== undefined) {
    renderDriverStatus(data.driver_status);
  }
  if (data.mem_rss !== undefined) {
    const el = document.getElementById('mem-pill');
    const rss = data.mem_rss, lim = data.mem_limit || 0;
    el.textContent = lim ? `MEM: ${rss}/${lim} MB` : `MEM: ${rss} MB`;
    const pct = lim ? rss / lim : 0;
    el.className = 'pill ' + (pct > 0.8 ? 'pill-err' : pct > 0.5 ? 'pill-warn' : 'pill-ok');
  }
  if (data.mova !== undefined) {
    state.mova = data.mova;
    applyMovaUpdate(data.mova);
  }
}

// ── Conditioner editor ───────────────────────────────────────────────────────
let _condEditRules = [];

function showCondEditor() {
  // Load current rules into editor
  _condEditRules = state.cond_rules.map(r => ({
    writer: r.writer, expression: r.expression
  }));
  renderCondEditTable();
  document.getElementById('cond-live-card').style.display = 'none';
  document.getElementById('cond-editor').style.display = 'block';
  document.getElementById('cond-save-msg').textContent = '';
}

function hideCondEditor() {
  document.getElementById('cond-live-card').style.display = 'block';
  document.getElementById('cond-editor').style.display = 'none';
}

function renderCondEditTable() {
  const tbody = document.getElementById('cond-edit-tbody');
  tbody.innerHTML = _condEditRules.map((r, i) => `
    <tr id="cond-row-${i}">
      <td style="padding:6px 12px;border-bottom:1px solid #f3f4f6">
        <input type="text" value="${r.writer}" onchange="_condEditRules[${i}].writer=this.value"
          placeholder="virt.0 or xkop.o.5"
          style="width:160px;padding:4px 8px;border:1px solid #d1d5db;border-radius:4px;font-size:12px;font-family:monospace">
      </td>
      <td style="padding:6px 12px;border-bottom:1px solid #f3f4f6">
        <input type="text" value="${r.expression}" onchange="_condEditRules[${i}].expression=this.value"
          placeholder="e.g. xkop.i.151 AND xkop.i.152"
          style="width:100%;padding:4px 8px;border:1px solid #d1d5db;border-radius:4px;font-size:12px;font-family:monospace">
      </td>
      <td style="padding:6px 12px;border-bottom:1px solid #f3f4f6;text-align:center">
        <button onclick="condDeleteRow(${i})"
          style="padding:3px 8px;border:1px solid #fca5a5;border-radius:4px;background:#fff;color:#b91c1c;font-size:11px;cursor:pointer">
          ✕
        </button>
      </td>
    </tr>`).join('');
}

function condAddRow() {
  _condEditRules.push({writer: '', expression: ''});
  renderCondEditTable();
  // Focus the new output field
  const rows = document.querySelectorAll('#cond-edit-tbody tr');
  if (rows.length) rows[rows.length-1].querySelector('input').focus();
}

function condDeleteRow(i) {
  _condEditRules.splice(i, 1);
  renderCondEditTable();
}

async function condSave() {
  const msg = document.getElementById('cond-save-msg');
  msg.textContent = 'Saving…';
  msg.style.color = '#6b7280';

  // Collect current input values
  const rules = _condEditRules.map((r, i) => {
    const row = document.getElementById(`cond-row-${i}`);
    if (!row) return r;
    const inputs = row.querySelectorAll('input');
    return {writer: inputs[0].value.trim(), expression: inputs[1].value.trim()};
  }).filter(r => r.writer && r.expression);

  try {
    const resp = await fetch('/api/conditioner', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(rules)
    });
    const data = await resp.json();
    if (!resp.ok) {
      const errs = data.errors ? data.errors.join('; ') : data.error;
      msg.textContent = '✗ ' + errs;
      msg.style.color = '#b91c1c';
    } else {
      msg.textContent = `✓ Saved ${data.saved} rule(s) and applied`;
      msg.style.color = '#15803d';
      setTimeout(hideCondEditor, 1500);
    }
  } catch(e) {
    msg.textContent = '✗ ' + e.message;
    msg.style.color = '#b91c1c';
  }
}

// ── AGD650 ───────────────────────────────────────────────────────────────────
function renderAGD(agd) {
  if (!agd) return;

  document.getElementById('agd-units').textContent = (agd.units || []).length + ' unit(s)';

  const anyDet  = agd.globals && agd.globals.any_detected;
  const anyPed  = agd.globals && agd.globals.any_pedestrian;
  const detEl   = document.getElementById('agd-any-detected');
  const pedEl   = document.getElementById('agd-any-pedestrian');
  detEl.textContent = anyDet ? 'YES' : 'No';
  detEl.className   = 'metric-value ' + (anyDet ? 'green' : '');
  pedEl.textContent = anyPed ? 'YES' : 'No';
  pedEl.className   = 'metric-value ' + (anyPed ? 'amber' : '');

  // Zone table grouped by unit
  const zTbody = document.getElementById('agd-zone-tbody');
  const col = (v) => `<td class="${v ? 'val-on' : 'val-off'}">${v ? '●' : '○'}</td>`;
  zTbody.innerHTML = (agd.units || []).map(unit => {
    const hdr = `<tr style="background:#f9fafb">
      <td colspan="7" style="font-size:11px;color:#6b7280;padding:5px 12px;font-weight:600">
        ${unit.id} &nbsp;·&nbsp; ${unit.ip}:${unit.port}
      </td></tr>`;
    const rows = (unit.zones || []).map(z =>
      `<tr><td style="padding-left:20px">Zone ${z.id}</td>
        ${col(z.detected)}
        ${col(z.has_car)}${col(z.has_pedestrian)}${col(z.has_cyclist)}${col(z.has_bus)}${col(z.has_lorry)}
      </tr>`).join('');
    return hdr + rows;
  }).join('');

  // Globals table
  const gTbody = document.getElementById('agd-global-tbody');
  const g = agd.globals || {};
  gTbody.innerHTML = [
    ['Detected',   g.any_detected],
    ['Car',        g.any_car],
    ['Pedestrian', g.any_pedestrian],
    ['Cyclist',    g.any_cyclist],
    ['Bus',        g.any_bus],
    ['Lorry',      g.any_lorry],
  ].map(([label, val]) => `<tr>
    <td>${label}</td>
    <td class="${val ? 'val-on' : 'val-off'}">${val ? '● Active' : '○ None'}</td>
  </tr>`).join('');

  // Events log
  const eTbody = document.getElementById('agd-events-tbody');
  eTbody.innerHTML = (agd.events || []).slice().reverse().map(e => {
    const stCls = e.detected ? 'badge-green' : 'badge-gray';
    const stLbl = e.detected ? 'Detect' : 'Clear';
    return `<tr>
      <td class="log-ts">${e.ts}</td>
      <td style="color:#6b7280;font-size:11px">${e.unit}</td>
      <td>Zone ${e.zone}</td>
      <td><span class="badge ${stCls}">${stLbl}</span></td>
      <td>${e.classes || '—'}</td>
    </tr>`;
  }).join('') || '<tr><td colspan="5" style="color:#9ca3af;text-align:center;padding:12px">No detections yet</td></tr>';
}

// ── FLIR ─────────────────────────────────────────────────────────────────────
function renderFLIR(flir) {
  if (!flir) return;

  // Camera count pill
  document.getElementById('flir-mode').textContent =
    (flir.cameras || []).length + ' camera(s)';

  // any_* cards
  const anyOcc  = flir.globals && flir.globals.any_occupied;
  const anyDil  = flir.globals && flir.globals.any_dilemma;
  const occEl   = document.getElementById('flir-any-occupied');
  const dilEl   = document.getElementById('flir-any-dilemma');
  occEl.textContent = anyOcc ? 'YES' : 'No';
  occEl.className   = 'metric-value ' + (anyOcc ? 'green' : '');
  dilEl.textContent = anyDil ? 'YES' : 'No';
  dilEl.className   = 'metric-value red ' + (anyDil ? '' : 'val-off');

  // Zone table — grouped by camera
  const zTbody = document.getElementById('flir-zone-tbody');
  const col = (v) => `<td class="${v ? 'val-on' : 'val-off'}">${v ? '●' : '○'}</td>`;
  zTbody.innerHTML = (flir.cameras || []).map(cam => {
    const camRow = `<tr style="background:#f9fafb">
      <td colspan="6" style="font-size:11px;color:#6b7280;padding:5px 12px;font-weight:600">
        ${cam.id} &nbsp;·&nbsp; ${cam.ip}:${cam.port}
      </td></tr>`;
    const zoneRows = (cam.zones || []).map(z =>
      `<tr><td style="padding-left:20px">Zone ${z.id}</td>
        ${col(z.occupied)}${col(z.dilemma)}${col(z.has_pedestrian)}${col(z.has_bicycle)}${col(z.has_vehicle)}
      </tr>`).join('');
    return camRow + zoneRows;
  }).join('');

  // Globals table
  const gTbody = document.getElementById('flir-global-tbody');
  const g = flir.globals || {};
  gTbody.innerHTML = [
    ['Occupied',   g.any_occupied],
    ['Dilemma',    g.any_dilemma],
    ['Pedestrian', g.any_pedestrian],
    ['Bicycle',    g.any_bicycle],
    ['Vehicle',    g.any_vehicle],
  ].map(([label, val]) => `<tr>
    <td>${label}</td>
    <td class="${val ? 'val-on' : 'val-off'}">${val ? '● Active' : '○ None'}</td>
  </tr>`).join('');

  // Events log
  const eTbody = document.getElementById('flir-events-tbody');
  eTbody.innerHTML = (flir.events || []).slice().reverse().map(e => {
    const stCls = e.state === 'Begin' ? 'badge-green' : 'badge-gray';
    return `<tr>
      <td class="log-ts">${e.ts}</td>
      <td style="color:#6b7280;font-size:11px">${e.camera || '—'}</td>
      <td>Zone ${e.zone}</td>
      <td>${e.type}</td>
      <td><span class="badge ${stCls}">${e.state}</span></td>
      <td>${e.cls || '—'}</td>
    </tr>`;
  }).join('') || '<tr><td colspan="6" style="color:#9ca3af;text-align:center;padding:12px">No events yet</td></tr>';
}

// ── Auto Dim ─────────────────────────────────────────────────────────────────
let _adLoaded = false;

function adSearch() {
  const q = document.getElementById('ad-search').value.trim();
  if (!q) return;
  const res = document.getElementById('ad-search-result');
  res.textContent = 'Searching…';
  fetch(`https://nominatim.openstreetmap.org/search?q=${encodeURIComponent(q)}&format=json&limit=1`)
    .then(r => r.json())
    .then(data => {
      if (!data.length) { res.textContent = 'No result found.'; return; }
      const place = data[0];
      document.getElementById('ad-lat').value = parseFloat(place.lat).toFixed(6);
      document.getElementById('ad-lon').value = parseFloat(place.lon).toFixed(6);
      res.textContent = '✓ ' + place.display_name;
    })
    .catch(() => { res.textContent = 'Search unavailable (no internet).'; });
}

function adPopulateSignals() {
  fetch('/api/bus_signals').then(r => r.json()).then(sigs => {
    const dl = document.getElementById('ad-signal-list');
    dl.innerHTML = sigs.map(s => `<option value="${s}">`).join('');
  }).catch(() => {});
}

function renderAutoDim(d) {
  if (!d) return;

  // Status cards
  const stateEl = document.getElementById('ad-state');
  if (d.output) {
    stateEl.textContent = 'DIM';
    stateEl.className = 'metric-value sm amber';
  } else {
    stateEl.textContent = 'BRIGHT';
    stateEl.className = 'metric-value sm green';
  }
  document.getElementById('ad-next-event').textContent =
    d.next_event + (d.next_event_in && d.next_event_in !== '—' ? ' (' + d.next_event_in + ')' : '');
  document.getElementById('ad-times-rise').textContent =
    (d.sunrise || '—') + ' → ' + (d.bright_time || '—');
  document.getElementById('ad-times-set').textContent =
    (d.sunset || '—') + ' → ' + (d.dim_time || '—');

  // Populate form on first load only (not after every SSE push)
  if (!_adLoaded) {
    _adLoaded = true;
    document.getElementById('ad-lat').value           = d.lat;
    document.getElementById('ad-lon').value           = d.lon;
    document.getElementById('ad-dim-offset').value    = d.dim_offset;
    document.getElementById('ad-bright-offset').value = d.bright_offset;
    document.getElementById('ad-signal').value        = d.signal;
    adPopulateSignals();
  }
}

function adSave() {
  const payload = {
    lat:           parseFloat(document.getElementById('ad-lat').value),
    lon:           parseFloat(document.getElementById('ad-lon').value),
    dim_offset:    parseInt(document.getElementById('ad-dim-offset').value, 10),
    bright_offset: parseInt(document.getElementById('ad-bright-offset').value, 10),
    signal:        document.getElementById('ad-signal').value.trim(),
    enabled:       true,
  };
  const msg = document.getElementById('ad-save-msg');
  msg.style.color = '#6b7280';
  msg.textContent = 'Saving…';
  fetch('/api/autodim', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload),
  }).then(r => r.json()).then(res => {
    if (res.error) { msg.style.color = '#b91c1c'; msg.textContent = res.error; }
    else           { msg.style.color = '#15803d'; msg.textContent = 'Saved.'; }
  }).catch(e => { msg.style.color = '#b91c1c'; msg.textContent = String(e); });
}

// ── SNMP activity sections ────────────────────────────────────────────────────
function renderCfgState() {
  const el = document.getElementById('ug-cfg-state');
  if (!el) return;
  const cfg = state.cfg_state;
  if (!Object.keys(cfg).length) {
    el.innerHTML = '<div style="color:#9ca3af;font-size:12px;padding:6px">No config received from instation yet.</div>';
    return;
  }
  let html = '<div class="table-card"><table style="width:100%"><thead><tr>'
    + '<th>Field</th><th>Value</th><th style="white-space:nowrap">Last Set</th>'
    + '</tr></thead><tbody>';
  for (const [field, d] of Object.entries(cfg)) {
    html += `<tr>
      <td><code style="font-size:11px">${field}</code></td>
      <td style="font-weight:600">${d.value}</td>
      <td style="font-size:11px;color:#6b7280;white-space:nowrap">${d.ts}</td>
    </tr>`;
  }
  html += '</tbody></table></div>';
  el.innerHTML = html;
}

const _OM_TRANS = ['1→2','2→3','3→3','3→2','3→1','2→1'];
const _OM_COL   = {
  '1→2':'#1d4ed8','2→3':'#15803d','3→3':'#6b7280',
  '3→2':'#b45309','3→1':'#b91c1c','2→1':'#b91c1c',
};
function renderOpModeTransitions() {
  const el = document.getElementById('ug-opmode-transitions');
  if (!el) return;
  const t = state.opmode_transitions;
  let html = '<div class="table-card"><table style="width:100%"><thead><tr>'
    + '<th>Transition</th><th>Last Seen</th></tr></thead><tbody>';
  for (const tr of _OM_TRANS) {
    const d      = t[tr];
    const ts     = d ? d.ts     : null;
    const reason = d ? d.reason : '';
    const col    = ts ? (_OM_COL[tr] || '#374151') : '#d1d5db';
    const label  = ts ? (reason ? `${ts} <span style="color:#9ca3af">(${reason})</span>` : ts) : '—';
    html += `<tr>
      <td style="font-weight:600;color:${col}">${tr}</td>
      <td style="font-size:11px;color:#6b7280">${label}</td>
    </tr>`;
  }
  html += '</tbody></table></div>';
  el.innerHTML = html;
}

function renderCtrlLog() {
  const el = document.getElementById('ug-ctrl-log');
  if (!el) return;
  const entries = state.ctrl_log;
  if (!entries.length) {
    el.innerHTML = '<div style="color:#9ca3af;font-size:12px;padding:6px">No control SETs received yet.</div>';
    return;
  }
  let html = '<div class="table-card"><table style="width:100%"><thead><tr>'
    + '<th style="white-space:nowrap">Time</th><th>SCN</th><th>Field</th><th>Value</th>'
    + '</tr></thead><tbody>';
  for (const e of entries) {
    html += `<tr>
      <td style="white-space:nowrap;font-size:11px;color:#6b7280">${e.ts}</td>
      <td>${e.scn || '—'}</td>
      <td><code style="font-size:11px">${e.field}</code></td>
      <td><code style="font-size:11px">${e.value}</code></td>
    </tr>`;
  }
  html += '</tbody></table></div>';
  el.innerHTML = html;
}

// ── Fixed-Time Plan ───────────────────────────────────────────────────────────
const _OP_MODE_LABEL = {1:'Standalone', 2:'Monitor', 3:'UTC Control'};

function renderOffplan(d) {
  if (!d) return;
  const settings = d.settings || {};
  const scns     = d.scns     || {};
  const plans    = d.plans    || {};
  const tt       = d.timetable|| {};

  // Settings cards
  document.getElementById('op-time-mode').textContent =
    settings.base_time_mode || '—';
  // Format "ddmmyyyy hhmm" → "dd/mm/yyyy hh:mm", fall back to raw string
  const _bt = settings.base_time || '';
  let _btFmt = _bt;
  if (/^\d{8} \d{4}$/.test(_bt)) {
    _btFmt = _bt.slice(0,2)+'/'+_bt.slice(2,4)+'/'+_bt.slice(4,8)+' '+_bt.slice(9,11)+':'+_bt.slice(11,13);
  }
  document.getElementById('op-base-time').textContent = _btFmt || '—';
  const modeNames = (settings.active_in_modes || [])
    .map(m => _OP_MODE_LABEL[m] || ('Mode ' + m)).join(', ');
  document.getElementById('op-active-modes').textContent = modeNames || '—';

  // Service status — show active plan(s) or standby
  const activePlans = Object.entries(scns)
    .filter(([,s]) => s.active_plan)
    .map(([scn,s]) => `${scn}: ${s.active_plan}`);
  const statusEl = document.getElementById('op-status');
  if (activePlans.length) {
    statusEl.textContent = activePlans.join(' · ');
    statusEl.className   = 'metric-value sm green';
  } else {
    statusEl.textContent = 'Standby';
    statusEl.className   = 'metric-value sm';
  }

  // Per-SCN status
  let scnHtml = '';
  for (const [scn, st] of Object.entries(scns)) {
    const pct = st.cycle_secs > 0
      ? Math.min(100, Math.round((st.cycle_pos / st.cycle_secs) * 100)) : 0;
    const planLabel = st.active_plan
      ? `<span class="badge badge-blue">${st.active_plan}</span>`
      : `<span class="badge badge-gray">None</span>`;
    const posLabel = st.cycle_secs > 0
      ? `${Math.floor(st.cycle_pos)}s / ${st.cycle_secs}s` : '—';

    let offsetDetail = '—';
    if (st.active_plan && st.offset_idx >= 0) {
      const offsets = plans[scn]?.[st.active_plan]?.offsets || [];
      const off = offsets[st.offset_idx];
      if (off) {
        const active = Object.entries(off.bits).filter(([,v]) => v).map(([k]) => k);
        offsetDetail = `@${off.at}s &nbsp; ` +
          (active.length
            ? active.map(k => `<span style="color:#15803d;font-weight:600">${k}</span>`).join(' ')
            : '<span style="color:#9ca3af">all clear</span>');
      }
    }

    scnHtml += `
    <div class="section-title">SCN: ${scn}</div>
    <div class="cards-row cards-3" style="margin-bottom:14px">
      <div class="metric-card">
        <div class="metric-label">Active plan</div>
        <div style="margin-top:6px">${planLabel}</div>
      </div>
      <div class="metric-card">
        <div class="metric-label">Cycle position</div>
        <div class="metric-value sm" style="margin-bottom:6px">${posLabel}</div>
        <div style="background:#e5e7eb;border-radius:4px;height:6px">
          <div style="background:#2563eb;width:${pct}%;height:100%;border-radius:4px;transition:width 0.8s"></div>
        </div>
      </div>
      <div class="metric-card">
        <div class="metric-label">Active offset</div>
        <div style="font-size:11px;margin-top:4px;line-height:1.7">${offsetDetail}</div>
      </div>
    </div>`;
  }
  document.getElementById('op-scn-container').innerHTML = scnHtml;

  // Timetable — global, flat array
  const ttEntries = Array.isArray(d.timetable) ? d.timetable : [];
  let ttHtml = '';
  if (ttEntries.length) {
    ttHtml = `<div class="table-card" style="margin-bottom:12px"><table>
      <thead><tr><th>Days</th><th>Start</th><th>Plan</th></tr></thead><tbody>`;
    for (const e of ttEntries) {
      const days  = (e.days || []).join(', ');
      const start = (e.start || '0000').replace(/(\d{2})(\d{2})/, '$1:$2');
      const plan  = e.plan
        ? `<span class="badge badge-blue">${e.plan}</span>`
        : `<span class="badge badge-gray">No plan</span>`;
      ttHtml += `<tr><td>${days}</td><td>${start}</td><td>${plan}</td></tr>`;
    }
    ttHtml += `</tbody></table></div>`;
  }
  document.getElementById('op-timetable-container').innerHTML = ttHtml;

  // Plan definitions
  let planHtml = '';
  for (const [scn, scnPlans] of Object.entries(plans)) {
    planHtml += `<div style="font-size:11px;color:#6b7280;margin-bottom:4px">SCN: ${scn}</div>`;
    for (const [planName, plan] of Object.entries(scnPlans)) {
      const offsets = plan.offsets || [];
      // Only show columns where at least one offset has the bit set, in natural order.
      const allCols = offsets.length ? Object.keys(offsets[0].bits) : [];
      const usedSet = new Set();
      for (const off of offsets)
        for (const [k,v] of Object.entries(off.bits)) { if (v) usedSet.add(k); }
      const cols = allCols.filter(c => usedSet.has(c));
      planHtml += `<div style="margin-bottom:10px">
        <div style="font-size:12px;font-weight:600;margin-bottom:4px">
          ${planName}
          <span class="badge badge-gray" style="margin-left:6px">${plan.cycle_secs}s cycle</span>
        </div>
        <div class="table-card"><table>
          <thead><tr><th>At (s)</th>${cols.map(c=>`<th>${c}</th>`).join('')}</tr></thead>
          <tbody>`;
      for (const off of offsets) {
        planHtml += `<tr><td>${off.at}</td>`;
        for (const col of cols) {
          const v = off.bits[col];
          planHtml += v ? `<td class="val-on">1</td>` : `<td class="val-off">0</td>`;
        }
        planHtml += `</tr>`;
      }
      planHtml += `</tbody></table></div></div>`;
    }
  }
  document.getElementById('op-plans-container').innerHTML = planHtml;
}

// ── Logs ─────────────────────────────────────────────────────────────────────
let _logLevels = new Set(['DEBUG','INFO','WARNING','ERROR','CRITICAL']);
let _logLines  = [];

function loadLogFiles() {
  const sel = document.getElementById('log-file-select');
  if (sel.options.length > 1) return;
  fetch('/api/logs').then(r => r.json()).then(files => {
    files.forEach(f => {
      const opt = document.createElement('option');
      opt.value = f.name;
      opt.textContent = f.name + '  (' + (f.size / 1024).toFixed(0) + ' KB)';
      sel.appendChild(opt);
    });
    if (files.length) { sel.value = files[0].name; loadLog(); updateLogDownload(); }
  });
}

function loadLog() {
  const sel   = document.getElementById('log-file-select');
  const nSel  = document.getElementById('log-lines-select');
  if (!sel.value) return;
  document.getElementById('log-viewer').innerHTML =
    '<div style="padding:14px;color:#9ca3af;font-size:12px">Loading…</div>';
  fetch('/api/logs/' + sel.value + '?lines=' + nSel.value)
    .then(r => r.json())
    .then(lines => { _logLines = lines; renderLog(); });
}

function updateLogDownload() {
  const sel = document.getElementById('log-file-select');
  const btn = document.getElementById('log-download-btn');
  if (!sel.value) { btn.style.display = 'none'; return; }
  btn.href = '/api/logs/' + sel.value + '/download';
  btn.download = sel.value;
  btn.style.display = '';
}

function logToggleLevel(lvl, btn) {
  if (_logLevels.has(lvl)) _logLevels.delete(lvl); else _logLevels.add(lvl);
  btn.classList.toggle('active');
  renderLog();
}

const _LV_RE = /\] (DEBUG|INFO|WARNING|ERROR|CRITICAL)\s/;
function _lineLevel(line) {
  const m = line.match(_LV_RE);
  return m ? m[1] : 'INFO';
}

function renderLog() {
  const viewer  = document.getElementById('log-viewer');
  const term    = (document.getElementById('log-search').value || '').trim().toLowerCase();
  const byLevel = _logLines.filter(l => _logLevels.has(_lineLevel(l)));
  const visible = term ? byLevel.filter(l => l.toLowerCase().includes(term)) : byLevel;
  const matchTxt = term ? ' — ' + visible.length.toLocaleString() + ' match' + (visible.length === 1 ? '' : 'es') : '';
  document.getElementById('log-line-count').textContent =
    byLevel.length.toLocaleString() + ' of ' + _logLines.length.toLocaleString() + ' lines' + matchTxt;
  if (!visible.length) {
    viewer.innerHTML = '<div style="padding:14px;color:#9ca3af;font-size:12px">'
      + (term ? 'No lines match "' + term.replace(/</g,'&lt;') + '".' : 'No lines match selected levels.') + '</div>';
    return;
  }
  function _highlight(raw) {
    const esc = raw.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
    if (!term) return esc;
    const tEsc = term.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
    return esc.replace(new RegExp(tEsc.replace(/[.*+?^${}()|[\]\\]/g,'\\$&'), 'gi'),
      m => '<span class="lv-match">' + m + '</span>');
  }
  viewer.innerHTML = visible.map(line => {
    const lvl = _lineLevel(line).toLowerCase();
    return '<div class="lv-line lv-' + lvl + '">' + _highlight(line) + '</div>';
  }).join('');
  viewer.scrollTop = viewer.scrollHeight;
}

// ── Fixed-Time Plan Config ────────────────────────────────────────────────────
let _opCfgLoaded = false;
let _opCfgData   = null;

function loadOffplanCfg() {
  if (_opCfgLoaded) return;
  fetch('/api/offline_plan_cfg').then(r => r.json()).then(d => {
    if (d.error) { console.error('offline_plan_cfg:', d.error); return; }
    _opCfgLoaded = true;
    _opCfgData   = d;
    _renderOffplanEditor(d);
  }).catch(e => console.error('loadOffplanCfg:', e));
}

function _renderOffplanEditor(d) {
  const settings  = d.settings  || {};
  const scns      = d.scns      || {};
  const timetable = d.timetable || [];
  const bt        = settings.base_time || '';
  const btFmt     = /^\d{8} \d{4}$/.test(bt)
    ? bt.slice(0,2)+'/'+bt.slice(2,4)+'/'+bt.slice(4,8)+' '+bt.slice(9,11)+':'+bt.slice(11,13)
    : bt;
  const modes    = settings.active_in_modes || [1,2];
  const modeOpts = ['datetime','time','none']
    .map(m=>`<option value="${m}" ${settings.base_time_mode===m?'selected':''}>${m}</option>`).join('');

  // Settings
  let html = `
  <div class="cfg-section" style="margin-bottom:12px">
    <div class="cfg-section-title">Settings</div>
    <div style="display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px 16px;margin-bottom:10px">
      <div class="ad-form-group">
        <label class="ad-label">Time Mode</label>
        <select id="opcfg-mode" class="ad-input">${modeOpts}</select>
      </div>
      <div class="ad-form-group">
        <label class="ad-label">Base Time (dd/mm/yyyy hh:mm)</label>
        <input type="text" id="opcfg-basetime" class="ad-input" value="${btFmt}" placeholder="01/01/2020 00:00">
      </div>
      <div class="ad-form-group">
        <label class="ad-label">Active In</label>
        <div style="display:flex;gap:14px;align-items:center;padding-top:6px">
          <label style="font-size:12px"><input type="checkbox" id="opcfg-m1" ${modes.includes(1)?'checked':''}> Standalone</label>
          <label style="font-size:12px"><input type="checkbox" id="opcfg-m2" ${modes.includes(2)?'checked':''}> Monitor</label>
        </div>
      </div>
    </div>
  </div>`;

  // Global timetable
  html += `<div class="cfg-section" style="margin-bottom:12px">
    <div class="cfg-section-title">Global Timetable</div>
    <div class="table-card" style="margin-bottom:6px"><table style="width:100%">
      <thead><tr>
        <th>Mon</th><th>Tue</th><th>Wed</th><th>Thu</th>
        <th>Fri</th><th>Sat</th><th>Sun</th>
        <th>Start</th><th>Plan</th><th style="width:32px"></th>
      </tr></thead>
      <tbody id="op-global-tt-tbody">`;
  for (const entry of timetable) {
    html += _buildTTRow(entry);
  }
  html += `</tbody></table></div>
    <button class="ad-btn-save admin-only" style="background:#374151;font-size:11px;margin-bottom:4px"
      onclick="_addGlobalTTRow()">+ Add Entry</button>
  </div>`;

  // Per-SCN plan definitions
  for (const [scnName, scnData] of Object.entries(scns)) {
    if (!scnData.plan_cols || !scnData.plan_cols.length) continue;
    html += _buildSCNPlanSection(scnName, scnData);
  }

  // Single save button
  html += `<div style="display:flex;align-items:center;gap:12px;margin-top:12px">
    <button class="ad-btn-save admin-only" onclick="saveOffplan(this)">Save All</button>
    <span class="op-save-msg" style="font-size:12px"></span>
    <span style="font-size:11px;color:#15803d">&#8635; Hot-reloads — no restart needed</span>
  </div>`;

  document.getElementById('op-plan-editor').innerHTML = html;
}

function _colHeaders(planCols) {
  return planCols.map(c => c.bits
    ? c.bits.map(b=>`<th style="text-align:center;white-space:nowrap">${c.field}[${b}]</th>`).join('')
    : `<th style="text-align:center">${c.field}</th>`
  ).join('');
}

function _offsetCells(planCols, offset) {
  return planCols.map(c => {
    if (c.bits) {
      const mask = parseInt(offset ? (offset[c.field.toLowerCase()]||0) : 0);
      return c.bits.map(b => {
        const chk = ((mask >> (b-1)) & 1) ? 'checked' : '';
        return `<td style="text-align:center;padding:3px">
          <input type="checkbox" class="op-cb" data-field="${c.field}" data-bit="${b}" ${chk}></td>`;
      }).join('');
    } else {
      const chk = offset && parseInt(offset[c.field.toLowerCase()]||0) ? 'checked' : '';
      return `<td style="text-align:center;padding:3px">
        <input type="checkbox" class="op-cb" data-field="${c.field}" ${chk}></td>`;
    }
  }).join('');
}

function _buildSCNPlanSection(scnName, scnData) {
  const cols     = scnData.plan_cols;
  const plans    = scnData.plans || {};
  const colsJson = JSON.stringify(cols).replace(/"/g,'&quot;');

  let html = `<div class="cfg-section" data-scn="${scnName}" style="margin-bottom:12px">
    <div class="cfg-section-title">SCN: ${scnName}</div>
    <div class="op-plans-wrap">`;

  for (const [planName, plan] of Object.entries(plans)) {
    html += _buildPlanBlock(planName, plan, cols, colsJson);
  }

  html += `</div>
    <button class="ad-btn-save admin-only" style="background:#374151;font-size:11px;margin-top:6px"
      onclick="_addPlan(this,'${scnName}')">+ Add Plan</button>
  </div>`;

  return html;
}

function _buildPlanBlock(planName, plan, cols, colsJson) {
  // Plan name is a SELECT populated from the global timetable.
  const ttNames = _getGlobalPlanNames();
  // Include current name even if not in timetable (handles stale/orphan gracefully).
  const allNames = [...new Set([planName, ...ttNames])].filter(Boolean);
  const nameOpts = allNames
    .map(n => `<option value="${n}" ${n===planName?'selected':''}>${n}</option>`).join('');

  let html = `<div class="table-card op-plan-block" style="margin-bottom:10px;padding:10px">
    <div style="display:flex;align-items:center;gap:8px;margin-bottom:8px">
      <span style="font-size:11px;color:#6b7280">Plan</span>
      <select class="ad-input op-plan-name" style="width:160px;font-weight:600">${nameOpts}</select>
      <span style="font-size:11px;color:#6b7280">Cycle</span>
      <input type="number" class="ad-input op-cycle" value="${plan.cycle_secs}"
             min="1" style="width:72px"> s
      <button onclick="this.closest('.op-plan-block').remove()"
        style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:13px;margin-left:auto">Remove Plan</button>
    </div>
    <div style="overflow-x:auto">
    <table style="width:100%" data-cols="${colsJson}">
      <thead><tr>
        <th style="width:70px">At (s)</th>
        ${_colHeaders(cols)}
        <th style="width:32px"></th>
      </tr></thead>
      <tbody class="op-offsets-tbody">`;

  for (const off of (plan.offsets || [])) {
    html += `<tr>
      <td style="padding:3px 8px"><input type="number" class="ad-input op-at"
          value="${off.at}" min="0" style="width:60px"></td>
      ${_offsetCells(cols, off)}
      <td style="text-align:center">
        <button onclick="this.closest('tr').remove()"
          style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:18px;line-height:1;padding:0">×</button>
      </td></tr>`;
  }

  html += `</tbody></table></div>
    <button class="ad-btn-save admin-only" style="background:#374151;font-size:11px;margin-top:8px"
      onclick="_addOffsetRow(this)">+ Add Offset</button>
  </div>`;
  return html;
}


function _buildTTRow(entry) {
  const days = ['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
  const start = (entry.start||'0000').replace(/(\d{2})(\d{2})/,'$1:$2');
  const planVal = entry.plan || '';
  const dayCells = days.map(d =>
    `<td style="text-align:center;padding:3px">
       <input type="checkbox" class="op-day" data-day="${d}"
         ${(entry.days||[]).includes(d)?'checked':''}></td>`
  ).join('');
  return `<tr>
    ${dayCells}
    <td style="padding:3px 8px"><input type="text" class="ad-input op-start"
        value="${start}" placeholder="07:00" style="width:72px"></td>
    <td style="padding:3px 8px"><input type="text" class="ad-input op-tt-plan-name"
        value="${planVal}" placeholder="AM_PEAK" style="width:140px"
        oninput="_syncPlanNameSelects()"></td>
    <td style="text-align:center">
      <button onclick="this.closest('tr').remove()"
        style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:18px;line-height:1;padding:0">×</button>
    </td></tr>`;
}

function _addOffsetRow(btn) {
  const table = btn.closest('.op-plan-block').querySelector('table');
  const cols  = JSON.parse(table.dataset.cols.replace(/&quot;/g,'"'));
  const tbody = table.querySelector('.op-offsets-tbody');
  const tr    = document.createElement('tr');
  tr.innerHTML = `
    <td style="padding:3px 8px"><input type="number" class="ad-input op-at"
        value="0" min="0" style="width:60px"></td>
    ${_offsetCells(cols, null)}
    <td style="text-align:center">
      <button onclick="this.closest('tr').remove()"
        style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:18px;line-height:1;padding:0">×</button>
    </td>`;
  tbody.appendChild(tr);
}

function _addPlan(btn, scnName) {
  const wrap = btn.closest('.cfg-section').querySelector('.op-plans-wrap');
  const cols = _opCfgData.scns[scnName]?.plan_cols || [];
  const colsJson = JSON.stringify(cols).replace(/"/g,'&quot;');
  const allNames = _getGlobalPlanNames();
  const defined  = new Set(
    Array.from(wrap.querySelectorAll('.op-plan-name')).map(el => el.value)
  );
  // Pre-select first timetable name not already defined; fall back to first name.
  const name = allNames.find(n => !defined.has(n)) || allNames[0] || 'NEW_PLAN';
  const div = document.createElement('div');
  div.innerHTML = _buildPlanBlock(name, {cycle_secs:60, offsets:[]}, cols, colsJson);
  wrap.appendChild(div.firstElementChild);
}

function _addGlobalTTRow() {
  const tbody = document.getElementById('op-global-tt-tbody');
  const temp = document.createElement('tbody');
  temp.innerHTML = _buildTTRow({days:[], start:'0700', plan:''});
  tbody.appendChild(temp.firstElementChild);
}

function _getGlobalPlanNames() {
  return [...new Set(
    Array.from(document.querySelectorAll('#op-global-tt-tbody .op-tt-plan-name'))
      .map(el => el.value.trim()).filter(Boolean)
  )];
}

function _readAllOffplanData() {
  const settings = {
    base_time_mode: document.getElementById('opcfg-mode').value,
    base_time:      document.getElementById('opcfg-basetime').value.trim(),
    active_in_modes: [
      ...(document.getElementById('opcfg-m1').checked ? [1] : []),
      ...(document.getElementById('opcfg-m2').checked ? [2] : []),
    ],
  };

  // Global timetable
  const timetable = [];
  document.querySelectorAll('#op-global-tt-tbody tr').forEach(tr => {
    const days  = Array.from(tr.querySelectorAll('.op-day:checked')).map(cb => cb.dataset.day);
    const start = tr.querySelector('.op-start').value.replace(':', '');
    const plan  = tr.querySelector('.op-tt-plan-name').value.trim() || null;
    if (days.length || start) timetable.push({days, start, plan});
  });

  // Per-SCN plan definitions only
  const scns = {};
  document.getElementById('op-plan-editor').querySelectorAll('.cfg-section[data-scn]').forEach(section => {
    const scnName = section.dataset.scn;
    const plans = {};
    section.querySelectorAll('.op-plan-block').forEach(block => {
      const planName  = block.querySelector('.op-plan-name').value.trim();
      const cycleSecs = parseInt(block.querySelector('.op-cycle').value);
      if (!planName) return;
      const offsets = [];
      block.querySelectorAll('.op-offsets-tbody tr').forEach(tr => {
        const at    = parseInt(tr.querySelector('.op-at').value);
        const entry = {at};
        const masks = {};
        tr.querySelectorAll('.op-cb[data-bit]').forEach(cb => {
          const field = cb.dataset.field.toLowerCase();
          const bit   = parseInt(cb.dataset.bit);
          if (!masks[field]) masks[field] = 0;
          if (cb.checked) masks[field] |= (1 << (bit-1));
        });
        for (const [k,v] of Object.entries(masks)) entry[k] = hex(v);
        tr.querySelectorAll('.op-cb:not([data-bit])').forEach(cb => {
          if (cb.checked) entry[cb.dataset.field.toLowerCase()] = 1;
        });
        offsets.push(entry);
      });
      offsets.sort((a,b) => a.at - b.at);
      plans[planName] = {cycle_secs: cycleSecs, offsets};
    });
    scns[scnName] = {plans};
  });

  return {settings, timetable, scns};
}

function hex(n) { return '0x'+n.toString(16).toUpperCase().padStart(2,'0'); }

function _syncPlanNameSelects() {
  // Update all SCN plan-name SELECTs when the global timetable changes.
  const names = _getGlobalPlanNames();
  document.getElementById('op-plan-editor')
    .querySelectorAll('.cfg-section[data-scn] .op-plan-name')
    .forEach(sel => {
      const current = sel.value;
      const allNames = [...new Set([current, ...names])].filter(Boolean);
      sel.innerHTML = allNames
        .map(n => `<option value="${n}" ${n===current?'selected':''}>${n}</option>`)
        .join('');
    });
}

function saveOffplan(btn) {
  const msg = btn.nextElementSibling;
  msg.style.color = '#6b7280'; msg.textContent = 'Saving…';
  const data = _readAllOffplanData();
  fetch('/api/offline_plan_cfg', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify(data),
  }).then(r => r.json()).then(res => {
    if (res.error) { msg.style.color='#b91c1c'; msg.textContent=res.error; }
    else           { msg.style.color='#15803d'; msg.textContent='Saved — plan reloaded.'; }
  }).catch(e => { msg.style.color='#b91c1c'; msg.textContent=String(e); });
}

// ── FLIR Zone Mapping ─────────────────────────────────────────────────────────
let _flirMappingLoaded = false;

function loadFLIRMapping() {
  if (_flirMappingLoaded) return;
  fetch('/api/flir_mapping').then(r => r.json()).then(d => {
    _flirMappingLoaded = true;
    const types  = d.zone_types  || [];
    const labels = d.type_labels || {};
    // Build header
    document.getElementById('flir-mapping-thead').innerHTML =
      `<tr><th>Zone</th><th>Output Signal</th>${types.map(t=>`<th style="text-align:center">${labels[t]||t}</th>`).join('')}</tr>`;
    // Build rows — one per zone across all cameras
    let rows = '';
    for (const cam of (d.cameras||[])) {
      for (const z of cam.zones) {
        const zd = (d.zones||{})[String(z)] || {};
        const out = zd.output || '';
        rows += `<tr>
          <td style="font-weight:600;color:#374151;padding:5px 10px">${z}
            <span style="font-size:10px;color:#9ca3af;margin-left:4px">${cam.id}</span></td>
          <td style="padding:3px 8px"><input type="text" class="ad-input flir-out"
            data-zone="${z}" value="${out}" placeholder="virt.X" style="width:120px"></td>
          ${types.map(t=>`<td style="text-align:center;padding:3px">
            <input type="checkbox" class="flir-cb" data-zone="${z}" data-type="${t}"
              ${(zd.types||[]).includes(t)?'checked':''}></td>`).join('')}
        </tr>`;
      }
    }
    document.getElementById('flir-mapping-tbody').innerHTML = rows;
  }).catch(e => console.error('loadFLIRMapping:', e));
}

function saveFLIRMapping() {
  const msg = document.getElementById('flir-mapping-msg');
  msg.style.color = '#6b7280'; msg.textContent = 'Saving…';
  const zones = {};
  document.querySelectorAll('.flir-out').forEach(el => {
    const z = el.dataset.zone;
    if (!zones[z]) zones[z] = {output: '', types: []};
    zones[z].output = el.value.trim();
  });
  document.querySelectorAll('.flir-cb:checked').forEach(el => {
    const z = el.dataset.zone;
    if (!zones[z]) zones[z] = {output: '', types: []};
    zones[z].types.push(el.dataset.type);
  });
  fetch('/api/flir_mapping', { method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({zones}) })
    .then(r => r.json()).then(res => {
      if (res.error) { msg.style.color='#b91c1c'; msg.textContent=res.error; }
      else           { msg.style.color='#15803d'; msg.textContent='Saved — restart to apply.'; }
    }).catch(e => { msg.style.color='#b91c1c'; msg.textContent=String(e); });
}

// ── AGD Zone Mapping ──────────────────────────────────────────────────────────
let _agdMappingLoaded = false;
let _agdMappingData   = null;

function loadAGDMapping() {
  if (_agdMappingLoaded) return;
  fetch('/api/agd_mapping').then(r => r.json()).then(d => {
    _agdMappingLoaded = true;
    _agdMappingData   = d;
    // Populate classes table
    const tbody = document.getElementById('agd-classes-tbody');
    tbody.innerHTML = '';
    for (const [name, suffix] of Object.entries(d.classes||{})) _appendAGDClassRow(tbody, name, suffix);
    _buildAGDZoneUI(d);
  }).catch(e => console.error('loadAGDMapping:', e));
}

function _appendAGDClassRow(tbody, name, suffix) {
  const tr = document.createElement('tr');
  tr.innerHTML =
    `<td style="padding:3px 8px"><input type="text" class="ad-input agd-cls-name"   value="${name}"   placeholder="car" style="width:100%"></td>
     <td style="padding:3px 8px"><input type="text" class="ad-input agd-cls-suffix" value="${suffix}" placeholder="has_car" style="width:100%"></td>
     <td style="padding:3px 2px;text-align:center">
       <button onclick="this.closest('tr').remove()"
               style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:18px;line-height:1;padding:0">×</button>
     </td>`;
  tbody.appendChild(tr);
}

function addAGDClass() {
  _appendAGDClassRow(document.getElementById('agd-classes-tbody'), '', '');
}

function _readAGDClasses() {
  const cls = {};
  document.querySelectorAll('#agd-classes-tbody tr').forEach(tr => {
    const n = tr.querySelector('.agd-cls-name').value.trim();
    const s = tr.querySelector('.agd-cls-suffix').value.trim();
    if (n && s) cls[n] = s;
  });
  return cls;
}

function rebuildAGDZoneTable() {
  const classes  = _readAGDClasses();
  const suffixes = ['detected', ...Object.values(classes)];
  // Preserve current output values before rebuild
  const saved = {};
  document.querySelectorAll('.agd-out').forEach(el => {
    saved[`${el.dataset.unit}|${el.dataset.zone}`] = el.value.trim();
  });
  const savedTypes = {};
  document.querySelectorAll('.agd-cb:checked').forEach(el => {
    const k = `${el.dataset.unit}|${el.dataset.zone}`;
    if (!savedTypes[k]) savedTypes[k] = [];
    savedTypes[k].push(el.dataset.suffix);
  });
  _buildAGDZoneUI(Object.assign({}, _agdMappingData, {classes, suffixes,
    zones: Object.fromEntries(Object.entries((_agdMappingData||{}).zones||{}).map(([uid,uz])=>
      [uid, Object.fromEntries(Object.entries(uz).map(([z,zd])=>
        [z, {output: saved[`${uid}|${z}`]??zd.output, types: savedTypes[`${uid}|${z}`]??zd.types}]))]))}));
}

function _buildAGDZoneUI(d) {
  const classes  = d.classes  || {};
  const suffixes = d.suffixes || ['detected', ...Object.values(classes)];
  const slabels  = Object.fromEntries(
    [['detected','Detected'],
     ...Object.entries(classes).map(([n,s])=>[s, n[0].toUpperCase()+n.slice(1)])]
  );
  let html = '';
  for (const unit of (d.units||[])) {
    const uid = unit.id.toLowerCase();
    html += `<div style="font-size:11px;color:#6b7280;margin-bottom:3px">${unit.id}</div>
    <div class="table-card" style="margin-bottom:10px"><table style="width:100%">
      <thead><tr><th>Zone</th><th>Output Signal</th>
        ${suffixes.map(s=>`<th style="text-align:center">${slabels[s]||s}</th>`).join('')}
      </tr></thead><tbody>`;
    for (const z of unit.zones) {
      const zd = ((d.zones[uid]||{})[String(z)])||{};
      html += `<tr>
        <td style="font-weight:600;color:#374151;padding:5px 10px">${z}</td>
        <td style="padding:3px 8px"><input type="text" class="ad-input agd-out"
          data-unit="${uid}" data-zone="${z}" value="${zd.output||''}"
          placeholder="virt.X" style="width:120px"></td>
        ${suffixes.map(s=>`<td style="text-align:center;padding:3px">
          <input type="checkbox" class="agd-cb" data-unit="${uid}" data-zone="${z}" data-suffix="${s}"
            ${(zd.types||[]).includes(s)?'checked':''}></td>`).join('')}
      </tr>`;
    }
    html += '</tbody></table></div>';
  }
  document.getElementById('agd-mapping-container').innerHTML = html;
}

function saveAGDMapping() {
  const msg = document.getElementById('agd-mapping-msg');
  msg.style.color = '#6b7280'; msg.textContent = 'Saving…';
  const classes = _readAGDClasses();
  const zones   = {};
  document.querySelectorAll('.agd-out').forEach(el => {
    const uid = el.dataset.unit, z = el.dataset.zone;
    if (!zones[uid])    zones[uid] = {};
    if (!zones[uid][z]) zones[uid][z] = {output:'', types:[]};
    zones[uid][z].output = el.value.trim();
  });
  document.querySelectorAll('.agd-cb:checked').forEach(el => {
    const uid = el.dataset.unit, z = el.dataset.zone;
    if (!zones[uid])    zones[uid] = {};
    if (!zones[uid][z]) zones[uid][z] = {output:'', types:[]};
    zones[uid][z].types.push(el.dataset.suffix);
  });
  fetch('/api/agd_mapping', { method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({classes, zones}) })
    .then(r => r.json()).then(res => {
      if (res.error) { msg.style.color='#b91c1c'; msg.textContent=res.error; }
      else           { msg.style.color='#15803d'; msg.textContent='Saved — restart to apply.'; }
    }).catch(e => { msg.style.color='#b91c1c'; msg.textContent=String(e); });
}

// ── SCN Config ────────────────────────────────────────────────────────────────
let _scnCfgLoaded = false;
let _scnFieldDefs = {};
let _outputSigs   = [];
let _inputSigs    = [];

function loadSCNConfig() {
  if (_scnCfgLoaded) return;
  Promise.all([
    fetch('/api/ug405_scns').then(r => r.json()),
    fetch('/api/bus_signals?dir=output').then(r => r.json()),
    fetch('/api/bus_signals?dir=input').then(r => r.json()),
  ]).then(([d, outs, ins]) => {
    _scnCfgLoaded = true;
    _scnFieldDefs = d;
    _outputSigs   = outs;
    _inputSigs    = ins;
    _buildSCNDatalist('scncfg-dl-out', outs);
    _buildSCNDatalist('scncfg-dl-in',  ins);
    const container = document.getElementById('scncfg-container');
    container.innerHTML = '';
    (d.scns || []).forEach(scn => container.appendChild(_makeSCNBlock(scn)));
  }).catch(e => console.error('loadSCNConfig:', e));
}

function _buildSCNDatalist(id, sigs) {
  let dl = document.getElementById(id);
  if (!dl) { dl = document.createElement('datalist'); dl.id = id; document.body.appendChild(dl); }
  dl.innerHTML = sigs.map(s => `<option value="${s}">`).join('');
}

function _makeSCNBlock(scn) {
  const wrap = document.createElement('div');
  wrap.className = 'cfg-section';
  wrap.dataset.scn = scn.name || '';

  // Header row
  const hdr = document.createElement('div');
  hdr.style = 'display:flex;align-items:center;gap:10px;margin-bottom:10px';
  hdr.innerHTML = `
    <span class="cfg-section-title" style="margin:0">SCN</span>
    <input type="text" class="ad-input scncfg-name" value="${scn.name||''}"
           placeholder="e.g. J0331" style="width:120px;font-weight:600">
    <button onclick="this.closest('.cfg-section').remove();_checkSCNConflicts()"
            style="background:none;border:none;cursor:pointer;color:#b91c1c;
                   font-size:13px;margin-left:auto">Remove SCN</button>`;
  wrap.appendChild(hdr);

  // Reply table
  wrap.appendChild(_makeSigTable('Reply signals', 'reply', scn.reply||[], false));
  // Control table
  wrap.appendChild(_makeSigTable('Control signals', 'control', scn.control||[], true));

  return wrap;
}

function _makeSigTable(title, side, rows, isControl) {
  const div = document.createElement('div');
  div.style.marginBottom = '12px';
  const dlId = isControl ? 'scncfg-dl-out' : 'scncfg-dl-in';
  div.innerHTML = `
    <div style="font-size:11px;color:#6b7280;font-weight:600;margin-bottom:4px">${title}</div>
    <div class="table-card" style="margin-bottom:6px">
      <table style="width:100%">
        <thead><tr>
          <th style="width:110px">Field</th>
          <th style="width:60px">Bit</th>
          <th>Signal</th>
          ${isControl ? '' : '<th style="width:60px;text-align:center">Inverted</th>'}
          <th style="width:32px"></th>
        </tr></thead>
        <tbody class="scncfg-tbody" data-side="${side}" data-dl="${dlId}"></tbody>
      </table>
    </div>
    <div style="display:flex;gap:6px;flex-wrap:wrap">
      ${_fieldButtons(side, isControl, dlId)}
    </div>`;
  const tbody = div.querySelector('tbody');
  rows.forEach(r => _appendSigRow(tbody, r, isControl, dlId));
  return div;
}

function _fieldButtons(side, isControl, dlId) {
  const bitmask = isControl ? (_scnFieldDefs.ctrl_bitmask||[]) : (_scnFieldDefs.repl_bitmask||[]);
  const single  = isControl ? (_scnFieldDefs.ctrl_single ||[]) : (_scnFieldDefs.repl_single ||[]);
  let html = '';
  bitmask.forEach(f => {
    html += `<button class="ad-btn-save admin-only" style="background:#374151;font-size:11px;padding:4px 10px"
      onclick="_addSigRow(this,'${f}',true,${isControl},'${dlId}')">${f}[n]</button>`;
  });
  single.forEach(f => {
    html += `<button class="ad-btn-save admin-only" style="background:#374151;font-size:11px;padding:4px 10px"
      onclick="_addSigRow(this,'${f}',false,${isControl},'${dlId}')">${f}</button>`;
  });
  return html;
}

function _addSigRow(btn, field, isBitmask, isControl, dlId) {
  const tbody = btn.closest('div').previousElementSibling.querySelector('tbody');
  _appendSigRow(tbody, {field, bit: isBitmask ? 1 : null, signal: '', inverted: false}, isControl, dlId);
}

function _appendSigRow(tbody, row, isControl, dlId) {
  const tr = document.createElement('tr');
  tr.dataset.field = row.field;   // store field name on the row, not just as text
  const bitCell = row.bit !== null && row.bit !== undefined
    ? `<input type="number" class="ad-input scncfg-bit" value="${row.bit}" min="1" max="32"
              style="width:54px" onchange="_checkSCNConflicts()">`
    : `<span style="color:#9ca3af;font-size:11px">—</span>`;
  const invCell = isControl ? '' :
    `<td style="text-align:center;padding:3px">
       <input type="checkbox" class="scncfg-inv" ${row.inverted?'checked':''}></td>`;
  tr.innerHTML = `
    <td style="padding:3px 8px;font-weight:500;font-size:12px">${row.field}</td>
    <td style="padding:3px 6px">${bitCell}</td>
    <td style="padding:3px 6px">
      <input type="text" class="ad-input scncfg-sig" value="${row.signal||''}"
             list="${dlId}" placeholder="signal"
             style="width:100%" onchange="_checkSCNConflicts()"></td>
    ${invCell}
    <td style="padding:3px 2px;text-align:center">
      <button onclick="this.closest('tr').remove();_checkSCNConflicts()"
              style="background:none;border:none;cursor:pointer;color:#b91c1c;
                     font-size:18px;line-height:1;padding:0">×</button></td>`;
  tbody.appendChild(tr);
}

function addSCN() {
  const container = document.getElementById('scncfg-container');
  container.appendChild(_makeSCNBlock({name:'', control:[], reply:[]}));
}

function _checkSCNConflicts() {
  const seen = {}, errors = [];
  document.getElementById('scncfg-container').querySelectorAll('.cfg-section[data-scn]').forEach(block => {
    const scnName = block.querySelector('.scncfg-name').value.trim() || '?';
    block.querySelectorAll('[data-side="control"] .scncfg-sig').forEach(inp => {
      const sig = inp.value.trim();
      if (!sig) return;
      const field = inp.closest('tr').dataset.field || '?';
      if (seen[sig]) {
        errors.push(`Signal conflict: ${sig} used by ${seen[sig]} and ${scnName}.${field}`);
        inp.style.borderColor = '#b91c1c';
      } else {
        seen[sig] = `${scnName}.${field}`;
        inp.style.borderColor = '';
      }
    });
  });
  const el = document.getElementById('scncfg-conflicts');
  el.innerHTML = errors.map(e => `<div>&#9888; ${e}</div>`).join('');
  return errors.length === 0;
}

function _readSCNData() {
  const scns = [];
  document.getElementById('scncfg-container').querySelectorAll('.cfg-section[data-scn]').forEach(block => {
    const name = block.querySelector('.scncfg-name').value.trim();
    const control = [], reply = [];
    block.querySelectorAll('tbody[data-side="control"] tr').forEach(tr => {
      const field  = tr.dataset.field;
      const bitEl  = tr.querySelector('.scncfg-bit');
      const sig    = tr.querySelector('.scncfg-sig').value.trim();
      if (!field || !sig) return;
      control.push({field, bit: bitEl ? parseInt(bitEl.value) : null, signal: sig});
    });
    block.querySelectorAll('tbody[data-side="reply"] tr').forEach(tr => {
      const field  = tr.dataset.field;
      const bitEl  = tr.querySelector('.scncfg-bit');
      const sig    = tr.querySelector('.scncfg-sig').value.trim();
      const inv    = tr.querySelector('.scncfg-inv');
      if (!field || !sig) return;
      reply.push({field, bit: bitEl ? parseInt(bitEl.value) : null,
                  signal: sig, inverted: inv ? inv.checked : false});
    });
    scns.push({name, control, reply});
  });
  return scns;
}

function applySCNConfig() {
  if (!_checkSCNConflicts()) {
    document.getElementById('scncfg-msg').textContent = 'Fix conflicts before applying.';
    return;
  }
  const msg = document.getElementById('scncfg-msg');
  const btn = document.getElementById('scncfg-apply-btn');
  msg.style.color = '#6b7280'; msg.textContent = 'Saving…';
  btn.disabled = true;
  fetch('/api/ug405_scns', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(_readSCNData()),
  }).then(r => r.json()).then(res => {
    if (res.error) {
      msg.style.color = '#b91c1c'; msg.textContent = res.error;
      btn.disabled = false;
    } else {
      msg.style.color = '#b45309'; msg.textContent = 'Saved — restarting…';
      fetch('/api/restart', {method:'POST'}).then(() => {
        const t = setInterval(() => {
          if (es.readyState === EventSource.OPEN) {
            clearInterval(t);
            msg.style.color = '#15803d'; msg.textContent = 'Restarted.';
            document.getElementById('scncfg-confirm').value = '';
            btn.disabled = true;
            _scnCfgLoaded = false;  // reload on next open
          }
        }, 500);
      });
    }
  }).catch(e => { msg.style.color='#b91c1c'; msg.textContent=String(e); btn.disabled=false; });
}

// ── Sensor Configuration (FLIR / AGD) ────────────────────────────────────────
let _sensorsLoaded = false;

function loadSensorsCfg() {
  if (_sensorsLoaded) return;
  fetch('/api/sensors_cfg').then(r => r.json()).then(d => {
    _sensorsLoaded = true;
    const fb = document.getElementById('flir-cameras-tbody');
    const ab = document.getElementById('agd-units-tbody');
    fb.innerHTML = ''; ab.innerHTML = '';
    (d.flir || []).forEach(c => _appendSensorRow(fb, c.ip, c.port, c.zones));
    (d.agd  || []).forEach(u => _appendSensorRow(ab, u.ip, u.port, u.zones));
    _renumber(fb); _renumber(ab);
  }).catch(e => console.error('loadSensorsCfg:', e));
}

function _appendSensorRow(tbody, ip, port, zones) {
  const tr = document.createElement('tr');
  tr.innerHTML =
    `<td class="srow-num" style="color:#6b7280;text-align:center;padding:4px 6px"></td>
     <td style="padding:3px 6px"><input type="text"   class="ad-input" value="${ip}"    placeholder="192.168.x.x" style="width:100%"></td>
     <td style="padding:3px 6px"><input type="number" class="ad-input" value="${port}"  min="1" max="65535" style="width:100%"></td>
     <td style="padding:3px 6px"><input type="text"   class="ad-input" value="${zones}" placeholder="1,2,3" style="width:100%"></td>
     <td style="padding:3px 2px;text-align:center">
       <button onclick="removeSensorRow(this)"
               style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:18px;line-height:1;padding:0">×</button>
     </td>`;
  tbody.appendChild(tr);
}

function addSensorRow(tbodyId, type, defaultPort) {
  const tbody = document.getElementById(tbodyId);
  _appendSensorRow(tbody, '', defaultPort, type === 'flir' ? '1' : '1,2,3,4,5,6,7,8');
  _renumber(tbody);
}

function removeSensorRow(btn) {
  const tbody = btn.closest('tbody');
  btn.closest('tr').remove();
  _renumber(tbody);
}

function _renumber(tbody) {
  tbody.querySelectorAll('tr').forEach((tr, i) => {
    const el = tr.querySelector('.srow-num');
    if (el) el.textContent = i + 1;
  });
}

function saveSensorCfg(type) {
  const tbodyId = type === 'flir' ? 'flir-cameras-tbody' : 'agd-units-tbody';
  const msgId   = type === 'flir' ? 'flir-save-msg'      : 'agd-save-msg';
  const url     = type === 'flir' ? '/api/flir_cfg'       : '/api/agd_cfg';
  const msg = document.getElementById(msgId);
  msg.style.color = '#6b7280'; msg.textContent = 'Saving…';
  const rows = Array.from(document.getElementById(tbodyId).querySelectorAll('tr')).map(tr => {
    const inp = tr.querySelectorAll('input');
    return { ip: inp[0].value.trim(), port: parseInt(inp[1].value, 10), zones: inp[2].value.trim() };
  });
  fetch(url, { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(rows) })
    .then(r => r.json()).then(res => {
      if (res.error) { msg.style.color = '#b91c1c'; msg.textContent = res.error; }
      else           { msg.style.color = '#15803d'; msg.textContent = 'Saved — restart to apply.'; }
    }).catch(e => { msg.style.color = '#b91c1c'; msg.textContent = String(e); });
}

// ── RTIG Configuration ───────────────────────────────────────────────────────
let _rtigCfgLoaded = false;
let _rtigCfgData   = null;

function loadRTIGCfg() {
  if (_rtigCfgLoaded) return;
  fetch('/api/rtig_cfg').then(r => r.json()).then(d => {
    if (d.error) { console.error('rtig_cfg:', d.error); return; }
    _rtigCfgLoaded = true;
    _rtigCfgData   = d;
    _renderRTIGCfg(d);
  }).catch(e => console.error('loadRTIGCfg:', e));
}

function _renderRTIGCfg(d) {
  const sm   = d.signal_map || {};
  const rules = d.rules || [];
  const cfg  = d.settings || {};
  const sigNames = Object.keys(sm);

  const opOpts = ['','=','<=','>=','<','>'].map(o =>
    `<option value="${o}">${o||'—'}</option>`).join('');

  let smRows = '';
  for (const [name, sig] of Object.entries(sm)) {
    smRows += _rtigSMRow(name, sig);
  }

  let ruleRows = '';
  for (const r of rules) ruleRows += _rtigRuleRow(r, sigNames);

  document.getElementById('rtig-cfg-editor').innerHTML = `
  <div class="cfg-section" style="margin-bottom:12px">
    <div class="cfg-section-title">Settings
      <span style="font-size:11px;color:#9ca3af;font-weight:400;margin-left:8px">Port and pulse changes require restart</span>
    </div>
    <div style="display:flex;gap:24px;margin-bottom:10px">
      <div class="ad-form-group">
        <label class="ad-label">Port</label>
        <input type="number" id="rtig-cfg-port" class="ad-input" style="width:90px" value="${cfg.port||9010}">
      </div>
      <div class="ad-form-group">
        <label class="ad-label">Pulse (s)</label>
        <input type="number" id="rtig-cfg-pulse" class="ad-input" style="width:90px" step="0.5" value="${cfg.pulse_seconds||2.0}">
      </div>
    </div>

    <div style="font-size:11px;color:#6b7280;font-weight:600;margin-bottom:6px">Signal Map
      <span style="font-size:10px;color:#9ca3af;font-weight:400;margin-left:6px">logical name → IO bus signal — restart required to apply</span>
    </div>
    <div class="table-card" style="margin-bottom:6px">
      <table style="width:100%">
        <thead><tr><th>Name</th><th>IO Bus Signal</th><th style="width:32px"></th></tr></thead>
        <tbody id="rtig-sm-tbody">${smRows}</tbody>
      </table>
    </div>
    <div style="display:flex;gap:8px;margin-bottom:12px">
      <button class="ad-btn-save admin-only" style="background:#374151;font-size:11px" onclick="_rtigAddSMRow()">+ Add Signal</button>
    </div>
    <div style="display:flex;align-items:center;gap:12px">
      <button class="ad-btn-save admin-only" onclick="saveRTIGCfg()">Save Signal Map</button>
      <span id="rtig-sm-msg" style="font-size:12px"></span>
      <span style="font-size:11px;color:#9ca3af">&#x26A0; Restart required</span>
    </div>
  </div>

  <div class="cfg-section">
    <div class="cfg-section-title">Rules
      <span style="font-size:11px;color:#15803d;font-weight:400;margin-left:8px">&#8635; Hot-reloads — no restart needed</span>
    </div>
    <div class="table-card" style="margin-bottom:6px;overflow-x:auto">
      <table style="width:100%">
        <thead><tr>
          <th>Signal</th><th>Movement</th><th>Trigger</th>
          <th>Traffic Sig</th><th colspan="2">Priority</th><th colspan="2">Sched. Dev</th>
          <th style="width:32px"></th>
        </tr></thead>
        <tbody id="rtig-rules-tbody2">${ruleRows}</tbody>
      </table>
    </div>
    <div style="display:flex;gap:8px;margin-bottom:10px">
      <button class="ad-btn-save admin-only" style="background:#374151;font-size:11px" onclick="_rtigAddRuleRow()">+ Add Rule</button>
    </div>
    <div style="display:flex;align-items:center;gap:12px">
      <button class="ad-btn-save admin-only" onclick="saveRTIGRules()">Save Rules</button>
      <span id="rtig-rules-msg" style="font-size:12px"></span>
    </div>
  </div>`;
}

function _rtigSMRow(name, sig) {
  return `<tr>
    <td style="padding:3px 8px"><input type="text" class="ad-input rtig-sm-name"
        value="${name}" placeholder="RTIG1" style="width:100px"></td>
    <td style="padding:3px 8px"><input type="text" class="ad-input rtig-sm-sig"
        list="ad-signal-list" value="${sig}" placeholder="virt.10" style="width:160px"></td>
    <td style="text-align:center;padding:3px 2px">
      <button onclick="this.closest('tr').remove()"
        style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:18px;line-height:1;padding:0">×</button>
    </td></tr>`;
}

function _rtigRuleRow(r, sigNames) {
  const sigOpts = (sigNames.length ? sigNames : (Object.keys(_rtigCfgData?.signal_map||{})))
    .map(n => `<option value="${n}" ${n.toUpperCase()===(r.signal||'').toUpperCase()?'selected':''}>${n}</option>`).join('');
  const pri = r.priority || {};
  const sd  = r.schedule_deviation || {};
  const opOpts = (sel) => ['','=','<=','>=','<','>']
    .map(o => `<option value="${o}" ${o===sel?'selected':''}>${o||'—'}</option>`).join('');
  return `<tr>
    <td style="padding:3px 6px"><select class="ad-input rtig-r-sig" style="width:90px">
      ${sigOpts}</select></td>
    <td style="padding:3px 6px"><input type="number" class="ad-input rtig-r-mov"
        value="${r.movement??''}" style="width:60px"></td>
    <td style="padding:3px 6px"><input type="number" class="ad-input rtig-r-trig"
        value="${r.trigger_point??''}" min="0" max="1" style="width:52px"></td>
    <td style="padding:3px 6px"><input type="number" class="ad-input rtig-r-tsig"
        value="${r.traffic_signal??''}" style="width:80px" placeholder="any"></td>
    <td style="padding:3px 4px"><select class="ad-input rtig-r-pri-op" style="width:52px">
      ${opOpts(pri.op||'')}</select></td>
    <td style="padding:3px 6px"><input type="number" class="ad-input rtig-r-pri-val"
        value="${pri.val??''}" style="width:52px" placeholder="—"></td>
    <td style="padding:3px 4px"><select class="ad-input rtig-r-sd-op" style="width:52px">
      ${opOpts(sd.op||'')}</select></td>
    <td style="padding:3px 6px"><input type="number" class="ad-input rtig-r-sd-val"
        value="${sd.val??''}" style="width:52px" placeholder="—"></td>
    <td style="text-align:center;padding:3px 2px">
      <button onclick="this.closest('tr').remove()"
        style="background:none;border:none;cursor:pointer;color:#b91c1c;font-size:18px;line-height:1;padding:0">×</button>
    </td></tr>`;
}

function _rtigAddSMRow() {
  const temp = document.createElement('tbody');
  temp.innerHTML = _rtigSMRow('', '');
  document.getElementById('rtig-sm-tbody').appendChild(temp.firstElementChild);
}

function _rtigAddRuleRow() {
  const sigNames = Object.keys(_rtigCfgData?.signal_map || {});
  const temp = document.createElement('tbody');
  temp.innerHTML = _rtigRuleRow({}, sigNames);
  document.getElementById('rtig-rules-tbody2').appendChild(temp.firstElementChild);
}

function saveRTIGCfg() {
  const msg = document.getElementById('rtig-sm-msg');
  msg.style.color = '#6b7280'; msg.textContent = 'Saving…';
  const signal_map = {};
  document.querySelectorAll('#rtig-sm-tbody tr').forEach(tr => {
    const name = tr.querySelector('.rtig-sm-name').value.trim();
    const sig  = tr.querySelector('.rtig-sm-sig').value.trim();
    if (name && sig) signal_map[name] = sig;
  });
  const settings = {
    port:          parseInt(document.getElementById('rtig-cfg-port').value) || 9010,
    pulse_seconds: parseFloat(document.getElementById('rtig-cfg-pulse').value) || 2.0,
  };
  fetch('/api/rtig_cfg', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({signal_map, settings}),
  }).then(r => r.json()).then(res => {
    if (res.error) { msg.style.color='#b91c1c'; msg.textContent=res.error; }
    else           { msg.style.color='#9ca3af'; msg.textContent='Saved — restart to apply.'; }
  }).catch(e => { msg.style.color='#b91c1c'; msg.textContent=String(e); });
}

function saveRTIGRules() {
  const msg = document.getElementById('rtig-rules-msg');
  msg.style.color = '#6b7280'; msg.textContent = 'Saving…';
  const rules = [];
  document.querySelectorAll('#rtig-rules-tbody2 tr').forEach(tr => {
    const sig  = tr.querySelector('.rtig-r-sig').value;
    const mov  = tr.querySelector('.rtig-r-mov').value;
    const trig = tr.querySelector('.rtig-r-trig').value;
    if (!sig) return;
    const rule = {signal: sig};
    if (mov  !== '') rule.movement      = parseInt(mov);
    if (trig !== '') rule.trigger_point = parseInt(trig);
    const tsig = tr.querySelector('.rtig-r-tsig').value;
    if (tsig !== '') rule.traffic_signal = parseInt(tsig);
    const priOp = tr.querySelector('.rtig-r-pri-op').value;
    const priVal = tr.querySelector('.rtig-r-pri-val').value;
    if (priOp && priVal !== '') rule.priority = {op: priOp, val: parseInt(priVal)};
    const sdOp  = tr.querySelector('.rtig-r-sd-op').value;
    const sdVal = tr.querySelector('.rtig-r-sd-val').value;
    if (sdOp && sdVal !== '') rule.schedule_deviation = {op: sdOp, val: parseInt(sdVal)};
    rules.push(rule);
  });
  fetch('/api/rtig_rules', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify(rules),
  }).then(r => r.json()).then(res => {
    if (res.error) { msg.style.color='#b91c1c'; msg.textContent=res.error; }
    else           { msg.style.color='#15803d'; msg.textContent=`Saved ${res.rules} rule(s) — hot-reloaded.`; }
  }).catch(e => { msg.style.color='#b91c1c'; msg.textContent=String(e); });
}

// ── Platform Configuration ────────────────────────────────────────────────────
let _cfgLoaded = false;

function loadPlatformCfg() {
  if (_cfgLoaded) return;
  fetch('/api/platform_cfg').then(r => r.json()).then(d => {
    _cfgLoaded = true;
    const svcs = d.services || {};
    ['xkop','ug405','rtig','conditioner','rpdb','flir','agd','autodim','offline_plan'].forEach(s => {
      const el = document.getElementById('svc-' + s);
      if (el) el.checked = svcs[s] !== 'disabled';
    });
    const x = d.xkop || {};
    document.getElementById('cfg-xkop-ip').value   = x.ip   || '';
    document.getElementById('cfg-xkop-port').value = x.port || 8001;
    document.getElementById('cfg-xkop-mode').value = x.mode || 'client';
    const r = d.rpdb || {};
    document.getElementById('cfg-rpdb-host').value           = r.host           || '';
    document.getElementById('cfg-rpdb-port').value           = r.port           || 35340;
    document.getElementById('cfg-rpdb-username').value       = r.username       || '';
    document.getElementById('cfg-rpdb-password').value       = r.password       || '';
    document.getElementById('cfg-rpdb-write-password').value = r.write_password || '';
    document.getElementById('cfg-fault-timeout').value = svcs.io_fault_timeout || 30;
    document.getElementById('cfg-mem-limit').value     = svcs.memory_limit_mb  || 300;
    const l = d.logging || {};
    document.getElementById('cfg-log-level').value  = (l.level || 'INFO').toUpperCase();
    document.getElementById('cfg-log-max-mb').value = l.log_max_mb || 10;
    document.getElementById('cfg-log-keep').value   = l.log_keep   || 10;
  }).catch(e => console.error('loadPlatformCfg:', e));
}

function savePlatformCfg() {
  const services = { io: 'enabled', ug405: 'enabled' };
  ['xkop','ug405','rtig','conditioner','rpdb','flir','agd','autodim','offline_plan'].forEach(s => {
    const el = document.getElementById('svc-' + s);
    services[s] = (el && el.checked) ? 'enabled' : 'disabled';
  });
  services.io_fault_timeout = parseInt(document.getElementById('cfg-fault-timeout').value, 10);
  services.memory_limit_mb  = parseInt(document.getElementById('cfg-mem-limit').value,     10);
  const payload = {
    services,
    rpdb: {
      host:           document.getElementById('cfg-rpdb-host').value.trim(),
      port:           parseInt(document.getElementById('cfg-rpdb-port').value, 10),
      username:       document.getElementById('cfg-rpdb-username').value,
      password:       document.getElementById('cfg-rpdb-password').value,
      write_password: document.getElementById('cfg-rpdb-write-password').value,
    },
    xkop: {
      ip:   document.getElementById('cfg-xkop-ip').value.trim(),
      port: parseInt(document.getElementById('cfg-xkop-port').value, 10),
      mode: document.getElementById('cfg-xkop-mode').value,
    },
    logging: {
      level:      document.getElementById('cfg-log-level').value,
      log_max_mb: parseInt(document.getElementById('cfg-log-max-mb').value, 10),
      log_keep:   parseInt(document.getElementById('cfg-log-keep').value,   10),
    },
  };
  const msg = document.getElementById('cfg-save-msg');
  msg.style.color = '#6b7280';
  msg.textContent = 'Saving…';
  fetch('/api/platform_cfg', {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload),
  }).then(r => r.json()).then(res => {
    if (res.error) { msg.style.color = '#b91c1c'; msg.textContent = res.error; }
    else           { msg.style.color = '#15803d'; msg.textContent = 'Saved — restart to apply.'; }
  }).catch(e => { msg.style.color = '#b91c1c'; msg.textContent = String(e); });
}

function restartCm5() {
  if (!confirm('Restart CM5? All connections will drop for a few seconds.')) return;
  const btn = document.getElementById('cfg-restart-btn');
  const msg = document.getElementById('cfg-restart-msg');
  btn.disabled = true;
  msg.style.color = '#b45309';
  msg.textContent = 'Restarting…';
  fetch('/api/restart', { method: 'POST' }).then(() => {
    msg.textContent = 'Restarting — waiting for reconnect…';
    const t = setInterval(() => {
      if (es.readyState === EventSource.OPEN) {
        clearInterval(t);
        btn.disabled = false;
        msg.style.color = '#15803d';
        msg.textContent = 'Restarted successfully.';
        _cfgLoaded = false;
      }
    }, 500);
  }).catch(() => {
    btn.disabled = false;
    msg.style.color = '#b91c1c';
    msg.textContent = 'Error sending restart command.';
  });
}

// ── Users panel (admin only) ──────────────────────────────────────────────────
let _pwTargetId = null;

function loadUsers() {
  fetch('/api/users').then(r=>r.json()).then(users => {
    const tbody = document.getElementById('users-tbody');
    if (!tbody) return;
    if (!users.length) {
      tbody.innerHTML = '<tr><td colspan="5" style="padding:12px;color:#9ca3af;text-align:center">No users.</td></tr>';
      return;
    }
    tbody.innerHTML = users.map(u => {
      const role = u.role === 'admin'
        ? '<span style="color:#b45309;font-weight:600">admin</span>'
        : '<span style="color:#374151">viewer</span>';
      const statusLabel = u.active ? '' : ' <span style="color:#b91c1c;font-size:10px">(disabled)</span>';
      const toggleBtn = u.active
        ? `<button onclick="toggleUser(${u.id},0)" style="padding:2px 8px;font-size:11px;border:1px solid #d1d5db;border-radius:4px;background:#fff;cursor:pointer;color:#b91c1c">Disable</button>`
        : `<button onclick="toggleUser(${u.id},1)" style="padding:2px 8px;font-size:11px;border:1px solid #d1d5db;border-radius:4px;background:#fff;cursor:pointer;color:#15803d">Enable</button>`;
      const roleToggle = u.role === 'admin'
        ? `<button onclick="setUserRole(${u.id},'viewer')" style="padding:2px 8px;font-size:11px;border:1px solid #d1d5db;border-radius:4px;background:#fff;cursor:pointer">→ viewer</button>`
        : `<button onclick="setUserRole(${u.id},'admin')" style="padding:2px 8px;font-size:11px;border:1px solid #d1d5db;border-radius:4px;background:#fff;cursor:pointer">→ admin</button>`;
      const last = (u.last_login||'—').replace('T',' ').replace('Z','');
      return `<tr style="border-top:1px solid #f3f4f6">
        <td style="padding:8px 12px;font-weight:500">${u.username}${statusLabel}</td>
        <td style="padding:8px 12px">${role}</td>
        <td style="padding:8px 12px;color:#6b7280;font-size:11px">${last}</td>
        <td style="padding:8px 12px">${u.active ? '<span style="color:#15803d">Active</span>' : '<span style="color:#6b7280">Disabled</span>'}</td>
        <td style="padding:8px 12px">
          <div style="display:flex;gap:6px;flex-wrap:wrap">
            ${roleToggle}
            <button onclick="openPwModal(${u.id},'${u.username}')" style="padding:2px 8px;font-size:11px;border:1px solid #d1d5db;border-radius:4px;background:#fff;cursor:pointer">Set PW</button>
            ${toggleBtn}
          </div>
        </td>
      </tr>`;
    }).join('');
    // Load current site name
    fetch('/api/site_name').then(r=>r.json()).then(d => {
      const el = document.getElementById('site-name-input');
      if (el) el.value = d.name || '';
    });
  }).catch(() => {
    const tbody = document.getElementById('users-tbody');
    if (tbody) tbody.innerHTML = '<tr><td colspan="5" style="padding:12px;color:#b91c1c">Error loading users.</td></tr>';
  });
}

function addUser() {
  const u = document.getElementById('nu-username').value.trim();
  const p = document.getElementById('nu-password').value;
  const r = document.getElementById('nu-role').value;
  const msg = document.getElementById('nu-msg');
  if (!u || !p) { msg.style.color='#b91c1c'; msg.textContent='Username and password required.'; return; }
  fetch('/api/users/add', {method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({username:u,password:p,role:r})})
    .then(r=>r.json()).then(d => {
      if (d.ok) {
        msg.style.color='#15803d'; msg.textContent='User added.';
        document.getElementById('nu-username').value='';
        document.getElementById('nu-password').value='';
        loadUsers();
      } else { msg.style.color='#b91c1c'; msg.textContent=d.error||'Error'; }
    });
}

function setUserRole(uid, role) {
  fetch('/api/users/'+uid+'/role', {method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({role})}).then(()=>loadUsers());
}

function toggleUser(uid, active) {
  fetch('/api/users/'+uid+'/active', {method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({active})}).then(()=>loadUsers());
}

function openPwModal(uid, uname) {
  _pwTargetId = uid;
  document.getElementById('pw-modal-name').textContent = uname;
  document.getElementById('pw-new').value = '';
  document.getElementById('pw-msg').textContent = '';
  document.getElementById('pw-modal').style.display = 'block';
}

function submitPwChange() {
  const pw = document.getElementById('pw-new').value;
  const msg = document.getElementById('pw-msg');
  if (!pw || pw.length < 8) { msg.style.color='#b91c1c'; msg.textContent='Min 8 characters.'; return; }
  fetch('/api/users/'+_pwTargetId+'/password', {method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify({password:pw})})
    .then(r=>r.json()).then(d => {
      if (d.ok) { msg.style.color='#15803d'; msg.textContent='Password updated.'; }
      else { msg.style.color='#b91c1c'; msg.textContent=d.error||'Error'; }
    });
}

function saveSiteName() {
  const name = document.getElementById('site-name-input').value.trim();
  const msg  = document.getElementById('sn-msg');
  fetch('/api/site_name', {method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({name})})
    .then(r=>r.json()).then(d => {
      if (d.ok) { msg.style.color='#15803d'; msg.textContent='Saved.'; }
      else { msg.style.color='#b91c1c'; msg.textContent=d.error||'Error'; }
    });
}

// ── Init ──────────────────────────────────────────────────────────────────────
buildUG405Tables();

// ── MOVA streams ─────────────────────────────────────────────────────────────
const PM_LABELS = {0:'CONT_IG',1:'START_IG',2:'ABS_MIN',3:'VAR_MIN',
                   4:'CHK_ENDSAT',5:'DELAY_STOPS',7:'MAX_GREEN',8:'WAIT_CHG'};
const MOVA_COLOURS = {
  ON_CONTROL:'#15803d', WARMUP:'#b45309', NO_CRB:'#b91c1c',
  FAILED_RESTART:'#b91c1c', OFF:'#6b7280', NOT_STARTED:'#6b7280',
  NO_DATASET:'#6b7280', INIT:'#b45309', INITIAL:'#b45309',
};

const PM_LABELS_SHORT = {0:'CONT_IG',1:'START_IG',2:'ABS_MIN',3:'VAR_MIN',4:'CHK_ENDSAT',5:'DELAY_STOPS',7:'MAX_GREEN!',8:'WAIT_CHG'};

function _scls(status) {
  const s = (status||'').toUpperCase();
  if (s==='ON_CONTROL')                           return 'control';
  if (s==='WARMUP')                               return 'warmup';
  if (s==='OFF'||s==='NOT_STARTED')               return 'off';
  if (s==='NO_CRB'||s==='FAILED_RESTART'||s==='MULTI_CONFIRMS') return 'fault';
  if (s==='NO_DATASET'||s==='NO_LICENCE')         return 'nodata';
  return 'other';
}

function _movaSetBits(container_id, values, cls_on) {
  const c = document.getElementById(container_id);
  if (!c) return;
  if (!c._built || c._built !== values.length) {
    c.innerHTML = '';
    const grp = document.createElement('div'); grp.className = 'mova-bit-group';
    values.forEach((v,i) => {
      const cell = document.createElement('div'); cell.className = 'mova-bit-cell';
      cell.innerHTML = `<div class="mova-bit-num">${i+1}</div><div class="mova-bit-dot" id="${container_id}-bit-${i}"></div>`;
      grp.appendChild(cell);
    });
    c.appendChild(grp);
    c._built = values.length;
  }
  values.forEach((v,i) => {
    const dot = document.getElementById(`${container_id}-bit-${i}`);
    if (dot) dot.className = 'mova-bit-dot' + (v ? ' ' + (cls_on||'on') : '');
  });
}

function applyMovaUpdate(streams) {
  if (!Array.isArray(streams)) return;
  streams.forEach(s => {
    const n = s.stream;
    if (n === undefined) return;
    const buf = s.buffers || {}, io_arr = buf.io || [], status = s.status || '—';
    const ios = s.io_state || {};
    const sc = _scls(status);
    let el;

    // Card border class
    const card = document.getElementById('mova-card-' + n);
    if (card) card.className = 'mova-card' + (sc==='fault'?' fault':sc==='warmup'?' warmup':sc==='control'?' active':'');

    // Sidebar pill
    el = document.getElementById('mova-pill-' + n);
    if (el) {
      const lbl = s.connected ? status.replace(/_/g,' ') : 'OFF';
      const bg  = s.connected ? (MOVA_COLOURS[status]||'#6b7280') : '#374151';
      el.textContent = lbl; el.style.background = bg; el.style.color = '#fff';
    }

    // Status badge
    el = document.getElementById('mova-badge-' + n);
    if (el) { el.textContent = status.replace(/_/g,' '); el.className = 'mova-status-badge mova-badge-'+sc; }

    // Dataset in header
    el = document.getElementById('mova-ds-title-' + n);
    if (el) el.textContent = s.dataset ? (s.dataset.title||s.dataset.stream_id_str||'—') : '—';

    // Kernel version
    el = document.getElementById('mova-kver-' + n);
    if (el && s.kernel_version) el.textContent = s.kernel_version;

    const ec  = io_arr.length>16 ? io_arr[16] : 0;
    const onc = io_arr.length>19 ? io_arr[19] : 0;
    const mon = io_arr.length>27 ? io_arr[27] : 0;
    const crb = buf.crb;
    const wc  = buf.warmup_counter ?? -1;
    const wu  = buf.kernel_in_warmup;

    // CRB
    el = document.getElementById('mova-crb-dot-' + n); if (el) el.className = 'mova-crb-dot'+(crb?' on':'');
    el = document.getElementById('mova-crb-val-' + n);
    if (el) { el.textContent = crb ? 'READY' : 'NOT READY'; el.className = 'mova-sval '+(crb?'on':'off'); }

    // ON_CONTROL
    el = document.getElementById('mova-onctrl-' + n);
    if (el) { el.textContent = onc ? 'YES' : 'NO'; el.className = 'mova-sval '+(onc?'on':'off'); }

    // MOVA Enabled
    el = document.getElementById('mova-en-' + n);
    if (el) { el.textContent = mon ? 'YES' : 'NO'; el.className = 'mova-sval '+(mon?'on':'off'); }

    // Error count
    el = document.getElementById('mova-ec-' + n);
    if (el) { el.textContent = ec; el.className = 'mova-sval '+(ec>0?'err':''); }

    // Warmup counter
    el = document.getElementById('mova-wc-' + n);
    if (el) { el.textContent = wc<0?'—':'WC:'+wc+(buf.kernel_stages?' / '+buf.kernel_stages:''); el.className = 'mova-sval '+(wu?'warn':''); }

    // Diagnostic row
    const cs = buf.kernel_current_stage ?? '—', ds = buf.kernel_demanded_stage ?? '—', ns = buf.kernel_next_stage ?? '—';
    const pm = buf.prog_marker, wt = buf.wait_stage_change_timer;
    const scan = io_arr.length>63 ? io_arr[63] : '—';
    el = document.getElementById('mova-pm-' + n);   if (el) el.textContent = pm!==undefined ? (PM_LABELS_SHORT[pm]||pm) : '—';
    el = document.getElementById('mova-cs-' + n);   if (el) el.textContent = cs;
    el = document.getElementById('mova-ds-' + n);   if (el) el.textContent = ds;
    el = document.getElementById('mova-ns-' + n);   if (el) el.textContent = ns;
    el = document.getElementById('mova-wt-' + n);   if (el) el.textContent = wt!==undefined ? wt : '—';
    el = document.getElementById('mova-wu-' + n);   if (el) { el.textContent = wu?'YES':'NO'; el.className='mova-sval '+(wu?'warn':''); }
    el = document.getElementById('mova-scan-' + n); if (el) el.textContent = scan;
    el = document.getElementById('mova-time-' + n); if (el) el.textContent = s.server_time || '—';
    el = document.getElementById('mova-date-' + n); if (el) el.textContent = s.server_date || '—';

    // MOVA ON button
    el = document.getElementById('mova-btn-on-' + n);
    if (el) { el.textContent = mon ? '⚡ MOVA OFF' : '⚡ MOVA ON'; el.className = 'mova-btn admin-only ' + (mon?'mova-off-btn':'mova-on-btn'); }

    // Bit grids
    _movaSetBits('mova-dets-' + n,  ios.detectors||[], 'on');
    _movaSetBits('mova-confs-' + n, ios.confirms||[], 'on');

    // Forces
    const forces = ios.forces||[];
    const fg = document.getElementById('mova-forces-' + n);
    if (fg) {
      if (!fg._built || fg._built !== forces.length) {
        fg.innerHTML = '';
        forces.forEach((v,i) => {
          fg.innerHTML += `<div class="mova-bit-cell"><div class="mova-bit-num">${i+1}</div><div class="mova-bit-dot" id="mova-f-${n}-${i}"></div></div>`;
        });
        fg._built = forces.length;
      }
      forces.forEach((v,i) => { const d=document.getElementById(`mova-f-${n}-${i}`); if(d) d.className='mova-bit-dot'+(v?' amber':''); });
    }

    // HI / TO / SYNC / FLT
    const dout = buf.dout || {};
    const setBit = (id, on, cls) => { const d=document.getElementById(id); if(d) d.className='mova-bit-dot'+(on?' '+(cls||'on'):''); };
    setBit('mova-hi-'+n,   ios.hi,   'on');
    setBit('mova-to-'+n,   ios.to,   'amber');
    setBit('mova-sync-'+n, ios.sync, 'cyan');
    setBit('mova-flt-'+n,  ios.mova_fault||ios.det_fault, 'red');

    // Faults
    el = document.getElementById('mova-faults-' + n);
    if (el && s.active_faults) {
      el.innerHTML = s.active_faults.length
        ? s.active_faults.map(f=>`<div class="mova-fault-item">${f.label||f.error_id||f}</div>`).join('')
        : '';
    }
  });
}

function showMova(n, navEl) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.nav-item').forEach(ni => ni.classList.remove('active'));
  const p = document.getElementById('panel-mova-' + n);
  if (p) p.classList.add('active');
  if (navEl) navEl.classList.add('active');
  loadMovaDatasets(n);
}

function movaCmdOn(n) {
  const s = (state.mova||[])[n] || {};
  const io = (s.buffers||{}).io || [];
  const cur = io.length > 27 ? io[27] : 0;
  movaCmd(n, 'SET_IO', ['27', cur ? '0' : '1']);
}
function movaCmdReset(n) {
  if (!confirm('Reset stream ' + n + '? This clears EC and restarts the kernel.')) return;
  movaCmd(n, 'RESET', []);
}
function movaCmd(n, cmd, args) {
  return fetch('/api/mova/stream/'+n+'/cmd', {method:'POST',
    headers:{'Content-Type':'application/json'}, body:JSON.stringify({cmd,args})})
    .then(r=>r.json());
}
function movaPopout(n) {
  window.open('/mova/stream/'+n,'mova_stream_'+n,'width=1200,height=900,resizable=yes,scrollbars=yes');
}

// I/O Map
function movaToggleIOMap(n) {
  const el = document.getElementById('mova-iomap-' + n);
  if (!el) return;
  if (el.style.display === 'none') { el.style.display=''; loadMovaIOMap(n); } else el.style.display='none';
}
function loadMovaIOMap(n) {
  fetch('/api/mova/stream/'+n+'/io_map').then(r=>r.json()).then(data => {
    const el = document.getElementById('mova-iomap-content-'+n); if (!el) return;
    const io = data.io || {};
    const row = (label,key,val,attr) =>
      `<tr><td style="padding:3px 8px;color:#6b7280;width:130px">${label}</td>`+
      `<td><input ${attr}="${key}" value="${val||''}" style="width:200px;font-family:monospace;font-size:11px;`+
      `padding:2px 4px;border:1px solid #d1d5db;border-radius:3px"></td></tr>`;
    let html = '<table style="width:100%;border-collapse:collapse">';
    html += row('CRB','crb',io.crb,'data-key');
    html += row('TO','to',io.to,'data-key');
    html += row('HI','hi',io.hi,'data-key');
    html += row('SYNC','sync',io.sync,'data-key');
    Object.entries(io.det_map||{}).sort((a,b)=>+a[0]-+b[0]).forEach(([k,v]) => html += row('Det '+(+k+1),k,v,'data-det'));
    Object.entries(io.confirm_map||{}).sort((a,b)=>+a[0]-+b[0]).forEach(([k,v]) => html += row('Confirm '+(+k+1),k,v,'data-conf'));
    Object.entries(io.force_map||{}).sort((a,b)=>+a[0]-+b[0]).forEach(([k,v]) => html += row('Force '+(+k+1),k,v,'data-force'));
    html += '</table>';
    el.innerHTML = html;
  });
}
function movaSaveIOMap(n) {
  const el = document.getElementById('mova-iomap-content-'+n); if (!el) return;
  const io = {det_map:{},confirm_map:{},force_map:{}};
  el.querySelectorAll('input').forEach(inp => {
    const v = inp.value.trim();
    if (inp.dataset.key)   io[inp.dataset.key]=v;
    if (inp.dataset.det)   io.det_map[inp.dataset.det]=v;
    if (inp.dataset.conf)  io.confirm_map[inp.dataset.conf]=v;
    if (inp.dataset.force) io.force_map[inp.dataset.force]=v;
  });
  fetch('/api/mova/stream/'+n+'/io_map',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({io})})
    .then(r=>r.json()).then(d=>{ if(d.ok) document.getElementById('mova-iomap-'+n).style.display='none'; else alert('Save failed: '+d.err); });
}

// Dataset
function movaShowDatasetLoader(n) {
  const el = document.getElementById('mova-dataset-loader-'+n); if (!el) return;
  if (el.style.display==='none') { el.style.display=''; loadMovaDatasets(n); } else el.style.display='none';
}
function loadMovaDatasets(n) {
  const el = document.getElementById('mova-ds-list-'+n); if (!el) return;
  fetch('/api/mova/datasets').then(r=>r.json()).then(files => {
    el.innerHTML = files.length
      ? files.map(f=>`<a href="#" onclick="movaPickDataset(${n},'${f.path}');return false" `+
          `style="display:block;margin-bottom:3px;color:#2563eb;font-size:11px">${f.name}</a>`).join('')
      : '<span style="color:#6b7280;font-size:11px">No datasets found</span>';
  });
}
function movaPickDataset(n, path) {
  fetch('/api/mova/dataset_streams',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({path})})
    .then(r=>r.json()).then(d => {
      if (!d.ok) { alert('Parse error: '+d.err); return; }
      const sid = d.streams.length===1 ? d.streams[0]
        : prompt('Select controller stream:\n'+d.streams.join('\n'), d.streams[0]);
      if (!sid) return;
      movaCmd(n,'LOAD',[path,sid]).then(r=>{ if(!r.ok) alert('Load failed: '+r.err); else document.getElementById('mova-dataset-loader-'+n).style.display='none'; });
    });
}
function movaUploadDataset(n, inp) {
  if (!inp.files.length) return;
  const fd = new FormData(); fd.append('file', inp.files[0]);
  fetch('/api/mova/upload_dataset',{method:'POST',body:fd}).then(r=>r.json())
    .then(d=>{ if(d.ok) movaPickDataset(n,d.path); else alert('Upload failed: '+d.err); });
}

const es = new EventSource('/stream');
es.onmessage = e => applyUpdate(JSON.parse(e.data));
es.onerror   = () => {
  document.getElementById('status-pills').innerHTML =
    '<span class="pill pill-err">SSE disconnected</span>';
};
</script>
</body>
</html>
'''

_PAGE_TEMPLATE = app.jinja_env.from_string(HTML)


# ── MOVA snapshot helpers ─────────────────────────────────────────────────────

def _mova_snapshot(n: int) -> dict:
    if _kernel_registry is None:
        return {"stream": n, "connected": False}
    client = _kernel_registry.get(n)
    if client is None:
        return {"stream": n, "connected": False}
    snap = client.latest_snapshot()
    if not snap:
        return {"stream": n, "connected": client.connected}
    buf = snap.get("buffers", {})
    msgs = []
    try:
        msgs = list(client.all_messages())[-5:]
    except Exception:
        pass
    return {
        "stream":         n,
        "connected":      client.connected,
        "status":         snap.get("status", {}).get("current"),
        "buffers":        buf,
        "io_state":       snap.get("io", {}),
        "dataset":        snap.get("dataset"),
        "running":        snap.get("running", False),
        "active_plan":    snap.get("active_plan"),
        "kernel_version": snap.get("kernel_version"),
        "server_time":    snap.get("server_time"),
        "server_date":    snap.get("server_date"),
        "active_faults":  snap.get("active_faults", []),
        "recent_msgs":    msgs,
    }


def _mova_all_snapshots() -> list:
    return [_mova_snapshot(n) for n in range(_mova_stream_count)]

# ── Mapping JSON builder ───────────────────────────────────────────────────────

def _build_mapping_json():
    if not _ug405_cfg:
        return json.dumps({'scns': [], 'control': {}, 'reply': {}})
    out = {'scns': _ug405_cfg['scns'], 'control': {}, 'reply': {}}
    for scn in _ug405_cfg['scns']:
        out['control'][scn] = {
            field: {str(bit): sig for bit, sig in bits.items()}
            for field, bits in _ug405_cfg['control'][scn].items()
        }
        out['reply'][scn] = {
            field: {str(bit): _sig_raw(sig) for bit, sig in bits.items()}
            for field, bits in _ug405_cfg['reply'][scn].items()
        }
    return json.dumps(out)


def _read_rss_mb():
    try:
        with open('/proc/self/status') as f:
            for line in f:
                if line.startswith('VmRSS:'):
                    return int(line.split()[1]) // 1024
    except Exception:
        pass
    return 0


def _driver_status():
    result = {}
    for name, drv in (('xkop', _xkop_driver), ('rpdb', _rpdb_driver)):
        if drv is not None:
            result[name] = drv.status()
    return result or None


def _sig_raw(sig):
    return sig[1:] if sig.startswith('!') else sig

def _sig_id(side, sig):
    return f"{side}-{_sig_raw(sig).replace('.', '_')}"


# ── SSE helpers ───────────────────────────────────────────────────────────────

def _push(payload):
    msg = json.dumps(payload)
    with _subscribers_lock:
        for q in _subscribers:
            q.append(msg)


def push_update(changes=None, opmode=None, instation=None, set_log_entry=None):
    """Called by UG405 service on state change."""
    payload = {}
    if changes:
        payload['changes'] = changes
    if opmode is not None:
        payload['opmode'] = opmode
    if instation:
        payload['instation'] = instation
    if set_log_entry:
        t = set_log_entry['type']
        if t == 'Config':
            _snmp_cfg_state[set_log_entry['field']] = {
                'value': set_log_entry['value'],
                'ts':    set_log_entry['ts'],
            }
            payload['cfg_state'] = dict(_snmp_cfg_state)
        elif t == 'opMode':
            _snmp_opmode_trans[set_log_entry['value']] = {
                'ts':     set_log_entry['ts'],
                'reason': set_log_entry.get('reason', ''),
            }
            payload['opmode_transitions'] = dict(_snmp_opmode_trans)
        elif t == 'Control':
            _snmp_ctrl_log.appendleft(set_log_entry)
            payload['ctrl_log_entry'] = set_log_entry
    if payload:
        _push(payload)


def _on_io_change(name, value, source):
    """Bus subscriber — push signal value changes to web clients immediately."""
    if not _ug405_cfg:
        return
    # Check if this signal appears in any SCN control or reply mapping
    changes = []
    for scn in _ug405_cfg['scns']:
        for field, bits in _ug405_cfg['control'][scn].items():
            for bit, sig in bits.items():
                if sig == name:
                    changes.append([_sig_id('ctrl', sig), value])
        for field, bits in _ug405_cfg['reply'][scn].items():
            for bit, sig in bits.items():
                if _sig_raw(sig) == name:
                    v = (1 - value) if sig.startswith('!') else value
                    changes.append([_sig_id('reply', sig), v])
    if changes:
        _push({'changes': changes})


def _build_full_state():
    """Build the complete state snapshot for a new SSE subscriber."""
    payload = {}

    # UG405 signals
    if _io and _ug405_cfg:
        changes = []
        ts = time.strftime('%H:%M:%S')
        for scn in _ug405_cfg['scns']:
            for field, bits in _ug405_cfg['control'][scn].items():
                for bit, sig in bits.items():
                    changes.append([_sig_id('ctrl', sig), _io.read(sig)])
            for field, bits in _ug405_cfg['reply'][scn].items():
                for bit, sig in bits.items():
                    raw = _sig_raw(sig)
                    val = _io.read(raw)
                    v   = (1 - val) if sig.startswith('!') else val
                    changes.append([_sig_id('reply', sig), v])
        payload['changes']    = changes
        payload['lastupdate'] = ts

    if _op_mode:
        payload['opmode'] = _op_mode['value']
    if _live:
        payload['instation'] = f"{_live['InstationAddress']}:{_live['InstationPort']}"
    payload['cfg_state']          = dict(_snmp_cfg_state)
    payload['opmode_transitions'] = dict(_snmp_opmode_trans)
    payload['ctrl_log']           = list(_snmp_ctrl_log)

    # IO bus snapshot
    if _io:
        payload['io_signals'] = [
            [name, owner, _io.read(name)]
            for name, owner in _io.registered_signals()
        ]

    # Conditioner rules + values
    if _cond:
        payload['cond_rules'] = _cond_snapshot()

    # RTIG
    if _rtig_svc:
        payload['rtig'] = _rtig_svc.snapshot()

    # FLIR
    if _flir:
        payload['flir'] = _flir_snapshot()

    # AGD
    if _agd:
        payload['agd'] = _agd_snapshot()

    # AutoDim
    if _autodim:
        payload['autodim'] = _autodim_snapshot()

    # Offline plan
    if _offline_plan:
        payload['offplan'] = _offplan_snapshot()

    # MOVA streams
    if _kernel_registry and _mova_stream_count > 0:
        payload['mova'] = _mova_all_snapshots()

    # Status pills
    payload['status_pills'] = _build_pills()
    payload['mem_rss']       = _read_rss_mb()
    payload['mem_limit']     = _mem_limit_mb
    ds = _driver_status()
    if ds:
        payload['driver_status'] = ds

    return payload


def _agd_snapshot():
    if not _agd:
        return None
    mapping = _agd.cfg['mapping']
    def _r(key):
        sig = mapping.get(key)
        return _io.read(sig) if sig else 0
    globals_ = {
        'any_detected':   _r('any_detected'),
        'any_car':        _r('any_car'),
        'any_pedestrian': _r('any_pedestrian'),
        'any_cyclist':    _r('any_cyclist'),
        'any_bus':        _r('any_bus'),
        'any_lorry':      _r('any_lorry'),
    }
    return {
        'units':   _agd.snapshot_zones(),
        'globals': globals_,
        'events':  list(_agd._events),
    }


def _autodim_snapshot():
    if not _autodim:
        return None
    return _autodim.snapshot()


def _offplan_snapshot():
    if not _offline_plan:
        return None
    return _offline_plan.snapshot()


def _flir_snapshot():
    if not _flir or not _io:
        return None
    mapping  = _flir.cfg['mapping']
    def _r(key):
        sig = mapping.get(key)
        return _io.read(sig) if sig else 0
    globals_ = {k: _r(k) for k in
                ['any_occupied','any_dilemma','any_pedestrian','any_bicycle','any_vehicle']}
    return {
        'cameras': _flir.snapshot_zones(),
        'globals': globals_,
        'events':  list(_flir._events),
    }


def _cond_snapshot():
    """Get conditioner rules with current virt values."""
    if not _cond or not _io:
        return []
    result = []
    for block in _cond.blocks:
        if getattr(block, 'hidden', False):
            continue   # inner nested block — not shown directly
        writer = getattr(block, 'output', '?')
        # Use stored original expression when available (set by parse_rules)
        expr = getattr(block, '_orig_expr', None)
        if expr is None:
            # Fallback for blocks without _orig_expr (e.g. loaded before this version)
            cls = type(block).__name__
            if hasattr(block, 'expression'):
                expr = block.expression
            elif cls in ('CountUpBlock', 'AccumulateBlock'):
                rst  = f', reset={block.reset_sig}' if getattr(block, 'reset_sig', None) else ''
                expr = f'COUNT_UP({block.input_sig}{rst})'
            elif cls == 'CountDownBlock':
                rst  = f', reset={block.reset_sig}' if getattr(block, 'reset_sig', None) else ''
                expr = f'COUNT_DOWN({block.input_sig}, start={block.start}{rst})'
            elif cls == 'BiDirCounterBlock':
                parts = [f'COUNT_UP({s})' for s in block.up_sigs] + \
                        [f'COUNT_DOWN({s})' for s in block.down_sigs]
                expr = ' OR '.join(parts)
            elif cls == 'FactorBlock':
                expr = f'FACTOR({block.source_sig}, bit={block.bit})'
            elif cls == 'N1CountBlock':
                expr = f'N1COUNT(a={block.a_sig}, b={block.b_sig}, c={block.c_sig})'
            elif cls == 'LatchBlock':
                expr = f'LATCH(set={block.set_sig}, reset={block.reset_sig})'
            elif cls == 'CountBlock':
                inp = getattr(block, 'input', '')
                n   = getattr(block, 'n', 1)
                rst = f', reset={block.reset_sig}' if getattr(block, 'reset_sig', None) else ''
                expr = f'COUNT({inp}, n={n}{rst})'
            elif cls == 'SpeedBlock':
                expr = f'SPEED(det1={block.det1_sig}, det2={block.det2_sig}, distance_m={block.distance_m})'
            else:
                fn     = cls.replace('Block', '').upper()
                inp    = getattr(block, 'input', None) or getattr(block, 'input_sig', '')
                params = getattr(block, 'delay', None) or getattr(block, 'timeout', None) or \
                         getattr(block, 'pulse', None) or ''
                expr   = f'{fn}({inp}{", " + str(params) if params else ""})'
        val = _io.read(writer) if _io else 0
        result.append({'writer': writer, 'expression': str(expr), 'value': val})
    return result


def _build_pills():
    pills = []
    # XKOP status via io_signals
    if _io:
        xkop_ok = any(
            v != 0
            for name, owner, v in (
                [name, owner, _io.read(name)]
                for name, owner in _io.registered_signals()
                if name.startswith('xkop.')
            )
        )
    else:
        xkop_ok = False

    def _ok(label):
        return f'<span class="pill pill-ok">{label} ✓</span>'
    def _off(label):
        return f'<span class="pill pill-warn">{label} off</span>'

    pills.append(_ok('UG405') if _op_mode is not None else _off('UG405'))
    pills.append(_ok('RTIG')  if _rtig_svc else _off('RTIG'))
    pills.append(_ok('COND')  if _cond     else _off('COND'))
    pills.append(_ok('FLIR')  if _flir     else _off('FLIR'))
    pills.append(_ok('AGD650')  if _agd     else _off('AGD650'))
    pills.append(_ok('DIM Service') if _autodim else _off('DIM Service'))
    pills.append(_ok('Fixed-Time Plan') if _offline_plan else _off('Fixed-Time Plan'))

    return ''.join(pills)


# ── Background poller ─────────────────────────────────────────────────────────

def _poll_loop():
    """Push IO/conditioner/RTIG snapshots every second."""
    while True:
        time.sleep(1.0)
        try:
            payload = {}
            if _io:
                payload['io_signals'] = [
                    [name, owner, _io.read(name)]
                    for name, owner in _io.registered_signals()
                ]
            if _cond:
                payload['cond_rules'] = _cond_snapshot()
            if _rtig_svc:
                payload['rtig'] = _rtig_svc.snapshot()
            if _flir:
                payload['flir'] = _flir_snapshot()
            if _agd:
                payload['agd'] = _agd_snapshot()
            if _autodim:
                payload['autodim'] = _autodim_snapshot()
            if _offline_plan:
                payload['offplan'] = _offplan_snapshot()
            if _kernel_registry and _mova_stream_count > 0:
                payload['mova'] = _mova_all_snapshots()
            payload['lastupdate']    = time.strftime('%H:%M:%S')
            payload['mem_rss']       = _read_rss_mb()
            payload['mem_limit']     = _mem_limit_mb
            ds = _driver_status()
            if ds:
                payload['driver_status'] = ds
            if payload:
                _push(payload)
        except Exception as e:
            log.debug('Poll error: %s', e)


# ── Routes ────────────────────────────────────────────────────────────────────

def _render_page():
    """Render the main dashboard for the current authenticated user."""
    u = _auth.current_user()
    if u is None:
        return redirect('/login')
    return _PAGE_TEMPLATE.render(
        mapping_json=_build_mapping_json(),
        username=u['username'],
        role=u['role'],
        is_admin=(u['role'] == 'admin'),
        mova_stream_count=_mova_stream_count,
    )


@app.route('/')
def root():
    return _render_page()


@app.route('/ug405/')
@app.route('/io/')
@app.route('/conditioner/')
@app.route('/rtig/')
@app.route('/autodim/')
def index():
    return _render_page()


# ── Auth routes ───────────────────────────────────────────────────────────────

@app.route('/login', methods=['GET', 'POST'])
def login():
    if _auth.needs_setup():
        return redirect('/setup')
    if request.method == 'POST':
        username = request.form.get('username', '').strip()
        password = request.form.get('password', '')
        user = _auth.authenticate(username, password)
        if user is None:
            return Response(_auth.login_page('Invalid username or password.'),
                            mimetype='text/html')
        token = _auth.start_session(user['id'], request.remote_addr)
        log.info('Login: %s (%s) from %s', username, user.get('role','?'), request.remote_addr)
        resp  = make_response(redirect('/'))
        _auth.set_session_cookie(resp, token)
        return resp
    if _auth.current_user():
        return redirect('/')
    return Response(_auth.login_page(), mimetype='text/html')


@app.route('/logout', methods=['GET', 'POST'])
def logout():
    token = request.cookies.get(_auth.COOKIE)
    if token:
        u = _auth.current_user()
        _auth.end_session(token)
        if u:
            log.info('Logout: %s from %s', u['username'], request.remote_addr)
    resp = make_response(redirect('/login'))
    _auth.clear_session_cookie(resp)
    return resp


@app.route('/setup', methods=['GET', 'POST'])
def setup():
    if not _auth.needs_setup():
        return redirect('/login')
    if request.method == 'POST':
        username = request.form.get('username', '').strip()
        password = request.form.get('password', '')
        confirm  = request.form.get('confirm', '')
        if not username or not password:
            return Response(_auth.setup_page('Username and password required.'),
                            mimetype='text/html')
        if password != confirm:
            return Response(_auth.setup_page('Passwords do not match.'),
                            mimetype='text/html')
        if len(password) < 8:
            return Response(_auth.setup_page('Password must be at least 8 characters.'),
                            mimetype='text/html')
        _auth.create_user(username, password, role='admin')
        return redirect('/login')
    return Response(_auth.setup_page(), mimetype='text/html')


# ── User management API (admin only) ─────────────────────────────────────────

@app.route('/api/users', methods=['GET'])
@_auth.require_admin
def api_users_list():
    return Response(json.dumps(_auth.list_users()), mimetype='application/json')


@app.route('/api/users/add', methods=['POST'])
@_auth.require_admin
def api_users_add():
    data = request.get_json(force=True) or {}
    username = (data.get('username') or '').strip()
    password = data.get('password') or ''
    role     = data.get('role', 'viewer')
    if not username or not password:
        return Response(json.dumps({'error': 'Username and password required.'}),
                        status=400, mimetype='application/json')
    if len(password) < 8:
        return Response(json.dumps({'error': 'Password must be at least 8 characters.'}),
                        status=400, mimetype='application/json')
    if role not in ('admin', 'viewer'):
        role = 'viewer'
    try:
        _auth.create_user(username, password, role)
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400,
                        mimetype='application/json')


@app.route('/api/users/<int:uid>/role', methods=['POST'])
@_auth.require_admin
def api_users_role(uid):
    data = request.get_json(force=True) or {}
    role = data.get('role', 'viewer')
    if role not in ('admin', 'viewer'):
        return Response(json.dumps({'error': 'Invalid role'}),
                        status=400, mimetype='application/json')
    _auth.set_role(uid, role)
    return Response(json.dumps({'ok': True}), mimetype='application/json')


@app.route('/api/users/<int:uid>/password', methods=['POST'])
@_auth.require_admin
def api_users_password(uid):
    data = request.get_json(force=True) or {}
    pw = data.get('password') or ''
    if len(pw) < 8:
        return Response(json.dumps({'error': 'Min 8 characters.'}),
                        status=400, mimetype='application/json')
    _auth.set_password(uid, pw)
    return Response(json.dumps({'ok': True}), mimetype='application/json')


@app.route('/api/users/<int:uid>/active', methods=['POST'])
@_auth.require_admin
def api_users_active(uid):
    data   = request.get_json(force=True) or {}
    active = int(data.get('active', 1))
    if active:
        _auth.activate_user(uid)
    else:
        _auth.deactivate_user(uid)
    return Response(json.dumps({'ok': True}), mimetype='application/json')


@app.route('/api/site_name', methods=['GET'])
@_auth.require_admin
def api_site_name_get():
    return Response(json.dumps({'name': _auth.get_setting('site_name', '')}),
                    mimetype='application/json')


@app.route('/api/site_name', methods=['POST'])
@_auth.require_admin
def api_site_name_post():
    data = request.get_json(force=True) or {}
    _auth.set_setting('site_name', (data.get('name') or '').strip())
    return Response(json.dumps({'ok': True}), mimetype='application/json')


@app.route('/api/conditioner', methods=['GET'])
@_auth.require_auth
def api_cond_get():
    """Return current conditioner rules as JSON."""
    if not _cond:
        return Response('[]', mimetype='application/json')
    rules = []
    for block in _cond.blocks:
        writer = getattr(block, 'output', '?')
        expr   = getattr(block, 'expression', None)
        if expr is None:
            cls  = type(block).__name__.replace('Block', '')
            inp  = getattr(block, 'input', None) or getattr(block, 'input_sig', '')
            expr = f'{cls}({inp})'
        rules.append({'writer': writer, 'expression': str(expr)})
    return Response(json.dumps(rules), mimetype='application/json')


@app.route('/api/conditioner', methods=['POST'])
@_auth.require_admin
def api_cond_post():
    """Validate, save and hot-reload conditioner rules."""
    try:
        data = request.get_json(force=True)
        if not isinstance(data, list):
            return Response(json.dumps({'error': 'Expected array'}), status=400,
                            mimetype='application/json')

        # Validate each rule
        errors = []
        valid_outputs = ('virt.', 'xkop.o.', 'gp.o.', 'cm5.o.', 'modb.o.')
        for i, r in enumerate(data):
            w = r.get('writer', '').strip()
            e = r.get('expression', '').strip()
            if not w:
                errors.append(f'Rule {i+1}: output signal required')
            elif not any(w.startswith(p) for p in valid_outputs):
                errors.append(f'Rule {i+1}: output "{w}" must be virt.*, xkop.o.*, gp.o.* etc')
            if not e:
                errors.append(f'Rule {i+1}: expression required')

        if errors:
            return Response(json.dumps({'errors': errors}), status=400,
                            mimetype='application/json')

        # Write to conditioner.cfg
        cond_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  'conditioner', 'conditioner.cfg')
        _write_conditioner_cfg(cond_path, data)

        # Hot-reload
        if _cond:
            rule_dicts = [{'writer': r['writer'], 'expression': r['expression']}
                          for r in data]
            _cond.reload(rule_dicts)

        log.info("Conditioner rules updated via web UI (%d rules)", len(data))
        return Response(json.dumps({'saved': len(data)}), mimetype='application/json')

    except Exception as e:
        log.error("Conditioner API error: %s", e)
        return Response(json.dumps({'error': str(e)}), status=500,
                        mimetype='application/json')


@app.route('/api/autodim', methods=['GET'])
@_auth.require_auth
def api_autodim_get():
    if not _autodim:
        return Response('{}', mimetype='application/json')
    return Response(json.dumps(_autodim.snapshot()), mimetype='application/json')


@app.route('/api/autodim', methods=['POST'])
@_auth.require_admin
def api_autodim_post():
    import os as _os
    try:
        data = request.get_json(force=True)
        lat           = float(data.get('lat', 0))
        lon           = float(data.get('lon', 0))
        dim_offset    = int(data.get('dim_offset', 20))
        bright_offset = int(data.get('bright_offset', -30))
        signal        = str(data.get('signal', 'autodim.o.dim')).strip()
        enabled       = bool(data.get('enabled', True))

        if not (-90 <= lat <= 90):
            return Response(json.dumps({'error': 'lat out of range'}), status=400,
                            mimetype='application/json')
        if not (-180 <= lon <= 180):
            return Response(json.dumps({'error': 'lon out of range'}), status=400,
                            mimetype='application/json')
        if not (-240 <= dim_offset <= 240):
            return Response(json.dumps({'error': 'dim_offset out of range (±240)'}), status=400,
                            mimetype='application/json')
        if not (-240 <= bright_offset <= 240):
            return Response(json.dumps({'error': 'bright_offset out of range (±240)'}), status=400,
                            mimetype='application/json')

        new_cfg = {
            'lat': lat, 'lon': lon,
            'dim_offset': dim_offset, 'bright_offset': bright_offset,
            'signal': signal, 'enabled': enabled,
        }

        # Persist to disk
        cfg_path = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)),
                                  'autodim', 'autodim.cfg')
        from svc_autodim import save_autodim
        save_autodim(cfg_path, new_cfg)

        # Hot-reload driver (lat/lon/offsets/enabled only; signal change = restart)
        if _autodim:
            _autodim.reload_cfg(new_cfg)

        log.info("AutoDim config updated via web — lat=%.4f lon=%.4f dim%+d bright%+d",
                 lat, lon, dim_offset, bright_offset)
        return Response(json.dumps({'saved': True}), mimetype='application/json')

    except Exception as e:
        log.error("AutoDim API error: %s", e)
        return Response(json.dumps({'error': str(e)}), status=500,
                        mimetype='application/json')


_LOGS_DIR         = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'logs')
_PLATFORM_CFG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'platform.cfg')
_UG405_CFG_PATH    = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ug405', 'ug405.cfg')
_OFFLINE_PLAN_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'offline_plan', 'offline_plan.json')
_RTIG_CFG_PATH     = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rtig', 'rtig.cfg')
_RTIG_RULES_PATH   = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rtig', 'rtig_rules.json')

_PLAN_FIELDS = ['Fn', 'Dn', 'SFn', 'GO', 'MO', 'PV', 'PX']   # control OIDs relevant for plans
_RPDB_CFG_PATH     = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'io',   'rpdb.cfg')
_FLIR_CFG_PATH     = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'flir', 'flir.cfg')
_AGD_CFG_PATH      = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'agd',  'agd.cfg')


def _read_rpdb_connection():
    """Read connection fields from [RPDB] section of rpdb.cfg."""
    result = {'host': '', 'port': 35340, 'username': '', 'password': '', 'write_password': ''}
    try:
        import configparser as _cp
        cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
        cp.read(_RPDB_CFG_PATH)
        s = 'RPDB' if cp.has_section('RPDB') else None
        if s:
            for k in result:
                if cp.has_option(s, k):
                    v = cp.get(s, k)
                    result[k] = int(v) if k == 'port' else v
    except Exception:
        pass
    return result


def _write_rpdb_connection(host, port, username, password, write_password):
    """Update only the connection fields in [RPDB] section, preserving rest of file."""
    updates = {
        'host':           str(host),
        'port':           str(int(port)),
        'username':       str(username),
        'password':       str(password),
        'write_password': str(write_password),
    }
    try:
        with open(_RPDB_CFG_PATH) as f:
            lines = f.readlines()
        in_rpdb = False
        out = []
        for line in lines:
            stripped = line.split('#')[0].strip()
            if stripped.startswith('['):
                in_rpdb = stripped.upper() == '[RPDB]'
                out.append(line)
                continue
            if in_rpdb and '=' in stripped:
                k = stripped.split('=')[0].strip().lower()
                if k in updates:
                    out.append(f'{k:<15}= {updates[k]}\n')
                    continue
            out.append(line)
        with open(_RPDB_CFG_PATH, 'w') as f:
            f.writelines(out)
    except Exception as e:
        log.error("Failed to write rpdb.cfg: %s", e)
        raise


def _write_platform_cfg(data):
    svcs = data['services']
    xkop = data['xkop']
    log_ = data['logging']
    lines = [
        '# platform.cfg',
        '# CM5 Platform — managed via web configuration editor',
        '',
        '[SERVICES]',
        f'io           = enabled',
        f'xkop         = {svcs.get("xkop",         "enabled")}',
        f'ug405        = {svcs.get("ug405",         "enabled")}',
        f'rtig         = {svcs.get("rtig",         "disabled")}',
        f'conditioner  = {svcs.get("conditioner",  "disabled")}',
        f'rpdb         = {svcs.get("rpdb",         "disabled")}',
        f'iobus_socket = {svcs.get("iobus_socket", "disabled")}',
        f'flir         = {svcs.get("flir",         "disabled")}',
        f'agd          = {svcs.get("agd",          "disabled")}',
        f'autodim      = {svcs.get("autodim",      "disabled")}',
        f'offline_plan = {svcs.get("offline_plan", "disabled")}',
        f'io_fault_timeout = {int(svcs.get("io_fault_timeout", 30))}',
        f'memory_limit_mb  = {int(svcs.get("memory_limit_mb",  300))}',
        '',
        '[XKOP]',
        f'ip   = {xkop["ip"]}',
        f'port = {int(xkop["port"])}',
        f'mode = {xkop["mode"]}',
        '',
        '[LOGGING]',
        f'level      = {log_["level"]}',
        f'log_max_mb = {int(log_.get("log_max_mb", 10))}',
        f'log_keep   = {int(log_.get("log_keep",   10))}',
        '',
    ]
    with open(_PLATFORM_CFG_PATH, 'w') as f:
        f.write('\n'.join(lines))


def _read_sensor_cameras():
    """Return FLIR cameras as list of {ip, port, zones} dicts."""
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_FLIR_CFG_PATH)
    cameras = []
    for s in sorted(cp.sections()):
        if not s.upper().startswith('CAMERA.'):
            continue
        zones = cp.get(s, 'zones', fallback='1')
        cameras.append({'ip': cp.get(s, 'ip', fallback=''), 'port': cp.getint(s, 'port', fallback=8765), 'zones': zones})
    return cameras


def _read_sensor_units():
    """Return AGD units as list of {ip, port, zones} dicts."""
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_AGD_CFG_PATH)
    units = []
    for s in sorted(cp.sections()):
        if not s.upper().startswith('AGD.'):
            continue
        zones = cp.get(s, 'zones', fallback='1,2,3,4,5,6,7,8')
        units.append({'ip': cp.get(s, 'ip', fallback=''), 'port': cp.getint(s, 'port', fallback=9003), 'zones': zones})
    return units


def _write_flir_cameras(cameras):
    """Rewrite [CAMERA.N] sections, preserving [VIRT_MAPPING]."""
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_FLIR_CFG_PATH)
    virt = dict(cp.items('VIRT_MAPPING')) if cp.has_section('VIRT_MAPPING') else {}

    lines = ['# flir/flir.cfg\n', '# Managed via CM5 web configuration editor\n', '\n']
    for i, cam in enumerate(cameras, 1):
        lines += [f'[CAMERA.{i}]\n',
                  f'ip    = {cam["ip"]}\n',
                  f'port  = {int(cam["port"])}\n',
                  f'zones = {cam["zones"]}\n', '\n']
    if virt:
        lines.append('[VIRT_MAPPING]\n')
        for k, v in virt.items():
            lines.append(f'{k:<25}= {v}\n')
        lines.append('\n')
    with open(_FLIR_CFG_PATH, 'w') as f:
        f.writelines(lines)


def _write_agd_units(units):
    """Rewrite [AGD.N] sections, preserving [SETTINGS], [CLASSES], [VIRT_MAPPING]."""
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_AGD_CFG_PATH)
    frame_timeout = cp.getfloat('SETTINGS', 'frame_timeout', fallback=5.0) \
                    if cp.has_section('SETTINGS') else 5.0
    classes = dict(cp.items('CLASSES'))       if cp.has_section('CLASSES')       else {}
    virt    = dict(cp.items('VIRT_MAPPING'))  if cp.has_section('VIRT_MAPPING')  else {}

    lines = ['# agd/agd.cfg\n', '# Managed via CM5 web configuration editor\n', '\n']
    for i, unit in enumerate(units, 1):
        lines += [f'[AGD.{i}]\n',
                  f'ip    = {unit["ip"]}\n',
                  f'port  = {int(unit["port"])}\n',
                  f'zones = {unit["zones"]}\n', '\n']
    lines += [f'[SETTINGS]\nframe_timeout = {frame_timeout}\n\n']
    if classes:
        lines.append('[CLASSES]\n')
        for k, v in classes.items():
            lines.append(f'{k:<15}= {v}\n')
        lines.append('\n')
    if virt:
        lines.append('[VIRT_MAPPING]\n')
        for k, v in virt.items():
            lines.append(f'{k:<35}= {v}\n')
        lines.append('\n')
    with open(_AGD_CFG_PATH, 'w') as f:
        f.writelines(lines)


_FLIR_ZONE_TYPES  = ['occupied','dilemma','has_pedestrian','has_bicycle','has_vehicle']
_FLIR_TYPE_LABELS = {'occupied':'Occupied','dilemma':'Dilemma',
                     'has_pedestrian':'Pedestrian','has_bicycle':'Bicycle','has_vehicle':'Vehicle'}


def _parse_flir_mapping():
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_FLIR_CFG_PATH)
    cameras = []
    for s in sorted(cp.sections()):
        if not s.upper().startswith('CAMERA.'):
            continue
        zones = [int(z.strip()) for z in cp.get(s,'zones',fallback='1').split(',') if z.strip()]
        cameras.append({'id': s, 'zones': zones})
    raw = dict(cp.items('VIRT_MAPPING')) if cp.has_section('VIRT_MAPPING') else {}
    all_zones = [z for cam in cameras for z in cam['zones']]
    zones_out = {}
    for z in all_zones:
        mapped = {t: raw.get(f'zone.{z}.{t}','') for t in _FLIR_ZONE_TYPES}
        output = next((v for v in mapped.values() if v), '')
        zones_out[str(z)] = {'output': output, 'types': [t for t,v in mapped.items() if v]}
    return {'cameras': cameras, 'zone_types': _FLIR_ZONE_TYPES,
            'type_labels': _FLIR_TYPE_LABELS, 'zones': zones_out}


def _write_flir_mapping(data):
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_FLIR_CFG_PATH)
    cam_rows = [(s, cp.get(s,'ip',fallback=''), cp.getint(s,'port',fallback=8765), cp.get(s,'zones',fallback='1'))
                for s in sorted(cp.sections()) if s.upper().startswith('CAMERA.')]
    lines = ['# flir/flir.cfg\n', '# Managed via CM5 web configuration editor\n', '\n']
    for s, ip, port, z in cam_rows:
        lines += [f'[{s}]\n', f'ip    = {ip}\n', f'port  = {port}\n', f'zones = {z}\n', '\n']
    lines.append('[VIRT_MAPPING]\n')
    for zone_num, zd in sorted(data.get('zones',{}).items(), key=lambda x: int(x[0])):
        out = zd.get('output','').strip()
        if out:
            for t in zd.get('types',[]):
                lines.append(f'zone.{zone_num}.{t:<20}= {out}\n')
    lines.append('\n')
    with open(_FLIR_CFG_PATH, 'w') as f:
        f.writelines(lines)


def _parse_agd_mapping():
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_AGD_CFG_PATH)
    units = []
    for s in sorted(cp.sections()):
        if not s.upper().startswith('AGD.'):
            continue
        zones = [int(z.strip()) for z in cp.get(s,'zones',fallback='1').split(',') if z.strip()]
        units.append({'id': s, 'num': int(s.split('.')[1]), 'zones': zones})
    classes  = dict(cp.items('CLASSES'))      if cp.has_section('CLASSES')      else {}
    raw      = dict(cp.items('VIRT_MAPPING')) if cp.has_section('VIRT_MAPPING') else {}
    suffixes = ['detected'] + list(classes.values())
    zones_out = {}
    for unit in units:
        uid = unit['id'].lower()
        zones_out[uid] = {}
        for z in unit['zones']:
            mapped = {s: raw.get(f'{uid}.zone.{z}.{s}','') for s in suffixes}
            output = next((v for v in mapped.values() if v), '')
            zones_out[uid][str(z)] = {'output': output, 'types': [s for s,v in mapped.items() if v]}
    return {'units': units, 'classes': classes, 'suffixes': suffixes, 'zones': zones_out}


def _write_agd_mapping(data):
    import configparser as _cp
    cp = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(_AGD_CFG_PATH)
    unit_rows = [(s, cp.get(s,'ip',fallback=''), cp.getint(s,'port',fallback=9003), cp.get(s,'zones',fallback='1,2,3,4,5,6,7,8'))
                 for s in sorted(cp.sections()) if s.upper().startswith('AGD.')]
    frame_timeout = cp.getfloat('SETTINGS','frame_timeout',fallback=5.0) if cp.has_section('SETTINGS') else 5.0
    classes = data.get('classes',{})
    zones   = data.get('zones',  {})
    lines = ['# agd/agd.cfg\n', '# Managed via CM5 web configuration editor\n', '\n']
    for s, ip, port, z in unit_rows:
        lines += [f'[{s}]\n', f'ip    = {ip}\n', f'port  = {port}\n', f'zones = {z}\n', '\n']
    lines += [f'[SETTINGS]\nframe_timeout = {frame_timeout}\n\n']
    if classes:
        lines.append('[CLASSES]\n')
        for name, suffix in classes.items():
            lines.append(f'{name:<15}= {suffix}\n')
        lines.append('\n')
    lines.append('[VIRT_MAPPING]\n')
    for uid, unit_zones in sorted(zones.items()):
        for zone_num, zd in sorted(unit_zones.items(), key=lambda x: int(x[0])):
            out = zd.get('output','').strip()
            if out:
                for suffix in zd.get('types',[]):
                    lines.append(f'{uid}.zone.{zone_num}.{suffix:<30}= {out}\n')
        lines.append('\n')
    with open(_AGD_CFG_PATH, 'w') as f:
        f.writelines(lines)


@app.route('/api/flir_mapping', methods=['GET'])
@_auth.require_auth
def api_flir_mapping_get():
    return Response(json.dumps(_parse_flir_mapping()), mimetype='application/json')

@app.route('/api/flir_mapping', methods=['POST'])
@_auth.require_admin
def api_flir_mapping_post():
    try:
        _write_flir_mapping(request.get_json(force=True))
        log.info("flir.cfg VIRT_MAPPING updated via web UI")
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400, mimetype='application/json')

@app.route('/api/agd_mapping', methods=['GET'])
@_auth.require_auth
def api_agd_mapping_get():
    return Response(json.dumps(_parse_agd_mapping()), mimetype='application/json')

@app.route('/api/agd_mapping', methods=['POST'])
@_auth.require_admin
def api_agd_mapping_post():
    try:
        _write_agd_mapping(request.get_json(force=True))
        log.info("agd.cfg mapping updated via web UI")
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400, mimetype='application/json')

@app.route('/api/sensors_cfg', methods=['GET'])
@_auth.require_auth
def api_sensors_cfg_get():
    return Response(json.dumps({'flir': _read_sensor_cameras(), 'agd': _read_sensor_units()}),
                    mimetype='application/json')


def _parse_sensor_post(request, default_port):
    rows = request.get_json(force=True)
    if not isinstance(rows, list):
        raise ValueError('Expected list')
    parsed = []
    for i, row in enumerate(rows, 1):
        ip = row.get('ip', '').strip()
        if not ip:
            raise ValueError(f'Row {i}: IP required')
        port = int(row.get('port', default_port))
        if not (1 <= port <= 65535):
            raise ValueError(f'Row {i}: port must be 1–65535')
        zones = row.get('zones', '').strip()
        if not zones:
            raise ValueError(f'Row {i}: zones required')
        parsed.append({'ip': ip, 'port': port, 'zones': zones})
    return parsed


@app.route('/api/flir_cfg', methods=['POST'])
@_auth.require_admin
def api_flir_cfg_post():
    try:
        cameras = _parse_sensor_post(request, 8765)
        _write_flir_cameras(cameras)
        log.info("flir.cfg updated via web UI (%d cameras)", len(cameras))
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400, mimetype='application/json')


@app.route('/api/agd_cfg', methods=['POST'])
@_auth.require_admin
def api_agd_cfg_post():
    try:
        units = _parse_sensor_post(request, 9003)
        _write_agd_units(units)
        log.info("agd.cfg updated via web UI (%d units)", len(units))
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400, mimetype='application/json')


@app.route('/api/platform_cfg', methods=['GET'])
@_auth.require_auth
def api_platform_cfg_get():
    from config import load_platform
    cfg = load_platform(_PLATFORM_CFG_PATH)
    cfg['rpdb'] = _read_rpdb_connection()
    return Response(json.dumps(cfg), mimetype='application/json')


@app.route('/api/platform_cfg', methods=['POST'])
@_auth.require_admin
def api_platform_cfg_post():
    try:
        data = request.get_json(force=True)
        svcs = data.get('services', {})
        xkop = data.get('xkop', {})
        log_ = data.get('logging', {})

        ip = xkop.get('ip', '').strip()
        if not ip:
            return Response(json.dumps({'error': 'XKOP IP is required'}),
                            status=400, mimetype='application/json')
        port = int(xkop.get('port', 0))
        if not (1 <= port <= 65535):
            return Response(json.dumps({'error': 'XKOP port must be 1–65535'}),
                            status=400, mimetype='application/json')
        if xkop.get('mode') not in ('client', 'server'):
            return Response(json.dumps({'error': 'XKOP mode must be client or server'}),
                            status=400, mimetype='application/json')
        timeout = int(svcs.get('io_fault_timeout', 30))
        if not (0 <= timeout <= 300):
            return Response(json.dumps({'error': 'IO fault timeout must be 0–300 s'}),
                            status=400, mimetype='application/json')
        mem = int(svcs.get('memory_limit_mb', 300))
        if not (100 <= mem <= 4096):
            return Response(json.dumps({'error': 'Memory limit must be 100–4096 MB'}),
                            status=400, mimetype='application/json')
        if log_.get('level', 'INFO').upper() not in ('DEBUG','INFO','WARNING','ERROR','CRITICAL'):
            return Response(json.dumps({'error': 'Invalid log level'}),
                            status=400, mimetype='application/json')

        _write_platform_cfg(data)
        log.info("platform.cfg updated via web UI")

        # Write RPDB connection if provided
        rpdb = data.get('rpdb', {})
        if rpdb.get('host'):
            rpdb_port = int(rpdb.get('port', 35340))
            if not (1 <= rpdb_port <= 65535):
                return Response(json.dumps({'error': 'RPDB port must be 1–65535'}),
                                status=400, mimetype='application/json')
            _write_rpdb_connection(
                host           = rpdb['host'].strip(),
                port           = rpdb_port,
                username       = rpdb.get('username', ''),
                password       = rpdb.get('password', ''),
                write_password = rpdb.get('write_password', ''),
            )
            log.info("rpdb.cfg connection updated via web UI")

        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=500, mimetype='application/json')


@app.route('/api/restart', methods=['POST'])
@_auth.require_admin
def api_restart():
    import subprocess
    def _do():
        time.sleep(0.5)
        subprocess.run(['systemctl', 'restart', 'cm5'])
    threading.Thread(target=_do, daemon=True, name='RestartCM5').start()
    log.info("CM5 restart requested via web UI")
    return Response(json.dumps({'ok': True}), mimetype='application/json')

@app.route('/api/logs', methods=['GET'])
@_auth.require_auth
def api_logs_list():
    try:
        files = []
        for name in os.listdir(_LOGS_DIR):
            if not name.startswith('cm5.log'):
                continue
            path = os.path.join(_LOGS_DIR, name)
            st = os.stat(path)
            files.append({'name': name, 'size': st.st_size, 'mtime': st.st_mtime})
        files.sort(key=lambda f: (0 if f['name'] == 'cm5.log'
                                  else int(f['name'].rsplit('.', 1)[-1])))
        return Response(json.dumps(files), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), mimetype='application/json', status=500)


@app.route('/api/logs/<filename>', methods=['GET'])
@_auth.require_auth
def api_logs_file(filename):
    if '/' in filename or '..' in filename or not filename.startswith('cm5.log'):
        return Response('Forbidden', status=403)
    path = os.path.join(_LOGS_DIR, filename)
    if not os.path.isfile(path):
        return Response('Not found', status=404)
    try:
        n = int(request.args.get('lines', 2000))
        with open(path, 'r', errors='replace') as f:
            lines = f.readlines()
        return Response(json.dumps([l.rstrip('\n') for l in lines[-n:]]),
                        mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), mimetype='application/json', status=500)


@app.route('/api/logs/<filename>/download', methods=['GET'])
@_auth.require_auth
def api_logs_download(filename):
    if '/' in filename or '..' in filename or not filename.startswith('cm5.log'):
        return Response('Forbidden', status=403)
    path = os.path.join(_LOGS_DIR, filename)
    if not os.path.isfile(path):
        return Response('Not found', status=404)
    return send_file(path, mimetype='text/plain', as_attachment=True,
                     download_name=filename)


@app.route('/api/driver/<name>/reconnect', methods=['POST'])
@_auth.require_admin
def api_driver_reconnect(name):
    drivers = {'xkop': _xkop_driver, 'rpdb': _rpdb_driver}
    drv = drivers.get(name)
    if drv is None:
        return Response(json.dumps({'ok': False, 'error': 'unknown driver'}),
                        mimetype='application/json', status=404)
    drv.resume()
    return Response(json.dumps({'ok': True}), mimetype='application/json')


def _all_signals(direction):
    """
    Generate all valid signal names for a given direction across every namespace.
    direction: 'output', 'input', or '' (all registered).
    Used to populate datalists — includes unregistered signals so users can
    map to rpdb, gp, cm5, modb signals even when those drivers are disabled.
    """
    sigs = []
    if direction in ('output', ''):
        for i in range(256): sigs.append(f'xkop.o.{i}')
        for i in range(256): sigs.append(f'rpdb.o.{i}')   # rpdb.o.N → PARDB VIRTUAL_IN etc.
        for i in range(128): sigs.append(f'gp.o.{i}')
        for i in range(128): sigs.append(f'cm5.o.{i}')
        for i in range(256): sigs.append(f'modb.o.{i}')
    if direction in ('input', ''):
        for i in range(256): sigs.append(f'xkop.i.{i}')
        for i in range(64):  sigs.append(f'rpdb.i.xsg.{i}')
        for i in range(256): sigs.append(f'rpdb.i.xdet.{i}')
        for i in range(256): sigs.append(f'rpdb.i.xin.{i}')
        for i in range(256): sigs.append(f'rpdb.i.xout.{i}')
        for i in range(64):  sigs.append(f'rpdb.i.stg.{i}')
        for i in range(64):  sigs.append(f'rpdb.i.mode.{i}')
        for i in range(128): sigs.append(f'gp.i.{i}')
        for i in range(128): sigs.append(f'cm5.i.{i}')
        for i in range(256): sigs.append(f'modb.i.{i}')
    if direction in ('output', 'input', ''):
        for i in range(512): sigs.append(f'virt.{i}')
    return sigs


@app.route('/api/bus_signals', methods=['GET'])
@_auth.require_auth
def api_bus_signals():
    """
    Return signal names for autocomplete. ?dir=output|input to filter by direction.
    Returns all valid signals across all namespaces (not just registered ones) so
    users can map to rpdb/gp/cm5/modb signals even when those drivers are disabled.
    Registered signals are listed first, then unregistered namespace signals.
    """
    filt = request.args.get('dir', '')
    registered = set()
    if _io:
        for name, _ in _io.registered_signals():
            if filt == 'output' and '.o.' not in name and not name.startswith('virt.'):
                continue
            if filt == 'input' and '.i.' not in name and not name.startswith('virt.'):
                continue
            registered.add(name)
    all_sigs  = _all_signals(filt)
    # Registered first (currently active), then remaining namespace signals
    ordered   = list(registered) + [s for s in all_sigs if s not in registered]
    return Response(json.dumps(ordered), mimetype='application/json')


def _write_conditioner_cfg(path, rules):
    """Write rules back to conditioner.cfg preserving header comments."""
    # Read existing file to preserve header
    header_lines = []
    try:
        with open(path) as f:
            for line in f:
                if line.strip().startswith('[IO]'):
                    break
                header_lines.append(line)
    except FileNotFoundError:
        header_lines = ['# conditioner/conditioner.cfg\n',
                        '# IO Conditioner rules\n', '\n']

    with open(path, 'w') as f:
        f.writelines(header_lines)
        f.write('[IO]\n')
        for r in rules:
            f.write(f" {r['writer']} = {r['expression']}\n")


@app.route('/stream')
@_auth.require_auth
def stream():
    q = deque(maxlen=200)
    with _subscribers_lock:
        _subscribers.append(q)

    # Capture socket fd now (while request context is valid) for disconnect detection.
    # When the browser refreshes it sends TCP FIN, putting the socket in CLOSE-WAIT.
    # Writes to CLOSE-WAIT sockets succeed indefinitely (client ACKs in FIN_WAIT_2),
    # so we can't rely on write failure. Instead, poll readability: a readable socket
    # with no data means the client sent FIN (EOF).
    _sock_fd = None
    try:
        wsgi_input = request.environ.get('wsgi.input')
        if wsgi_input is not None:
            _sock_fd = wsgi_input.fileno()
    except Exception:
        pass
    if _sock_fd is None:
        try:
            raw = getattr(wsgi_input, '_stream', None)
            if raw is not None:
                _sock_fd = raw.fileno()
        except Exception:
            pass

    def generate():
        try:
            # Full state on connect
            yield f"data: {json.dumps(_build_full_state())}\n\n"
            while True:
                # Non-blocking check: if socket is readable the client sent FIN
                if _sock_fd is not None:
                    try:
                        r, _, _ = _select.select([_sock_fd], [], [], 0)
                        if r:
                            return
                    except Exception:
                        return
                if q:
                    yield f"data: {q.popleft()}\n\n"
                else:
                    time.sleep(0.05)
        finally:
            with _subscribers_lock:
                try:
                    _subscribers.remove(q)
                except ValueError:
                    pass

    return Response(
        generate(),
        mimetype='text/event-stream',
        headers={'Cache-Control': 'no-cache', 'X-Accel-Buffering': 'no'}
    )


def _parse_rtig_cfg():
    """Read rtig.cfg + rtig_rules.json for the editor."""
    import configparser as _cp
    cfg = _cp.ConfigParser(inline_comment_prefixes=('#',))
    cfg.optionxform = str   # preserve key case (RTIG1, not rtig1)
    cfg.read(_RTIG_CFG_PATH)
    settings = {
        'port':          cfg.getint  ('RTIG', 'port',          fallback=9010),
        'pulse_seconds': cfg.getfloat('RTIG', 'pulse_seconds', fallback=2.0),
    }
    signal_map = dict(cfg.items('SIGNAL_MAP')) if cfg.has_section('SIGNAL_MAP') else {}
    with open(_RTIG_RULES_PATH) as f:
        rules = json.load(f)
    return {'settings': settings, 'signal_map': signal_map, 'rules': rules}


def _write_rtig_cfg(signal_map, settings):
    """Write rtig.cfg — requires CM5 restart to take effect."""
    port  = int(settings.get('port', 9010))
    pulse = float(settings.get('pulse_seconds', 2.0))
    lines = [
        '# rtig/rtig.cfg\n',
        '# RTIG service configuration\n\n',
        '[RTIG]\n',
        f'port          = {port}\n',
        f'pulse_seconds = {pulse}\n',
        'rules_file    = rtig_rules.json\n\n',
        '[SIGNAL_MAP]\n',
    ]
    for name, sig in signal_map.items():
        name, sig = name.strip(), sig.strip()
        if name and sig:
            lines.append(f'{name} = {sig}\n')
    with open(_RTIG_CFG_PATH, 'w') as f:
        f.writelines(lines)


@app.route('/api/rtig_cfg', methods=['GET'])
@_auth.require_auth
def api_rtig_cfg_get():
    try:
        return Response(json.dumps(_parse_rtig_cfg()), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=500, mimetype='application/json')


@app.route('/api/rtig_cfg', methods=['POST'])
@_auth.require_admin
def api_rtig_cfg_post():
    try:
        data = request.get_json(force=True)
        _write_rtig_cfg(data.get('signal_map', {}), data.get('settings', {}))
        log.info("rtig.cfg updated via web UI — restart required")
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400, mimetype='application/json')


@app.route('/api/rtig_rules', methods=['POST'])
@_auth.require_admin
def api_rtig_rules_post():
    try:
        new_rules = request.get_json(force=True)
        if not isinstance(new_rules, list):
            raise ValueError("Expected JSON array")
        # Basic validation — rules must have a signal key
        valid = [r for r in new_rules if r.get('signal')]
        with open(_RTIG_RULES_PATH, 'w') as f:
            json.dump(valid, f, indent=2)
        if _rtig_svc:
            _rtig_svc.rules = valid
        log.info("RTIG rules updated via web UI (%d rules)", len(valid))
        return Response(json.dumps({'ok': True, 'rules': len(valid)}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400, mimetype='application/json')


def _parse_offline_plan_cfg():
    """Return plan JSON + per-SCN column defs derived from ug405_cfg control mapping."""
    with open(_OFFLINE_PLAN_PATH) as f:
        raw = json.load(f)

    settings = raw.get('settings', {})
    raw_scns = raw.get('scns', {})

    scns_out = {}
    for scn_name, scn_data in raw_scns.items():
        # Build plan_cols: which control OIDs exist for this SCN
        plan_cols = []
        if _ug405_cfg and scn_name in _ug405_cfg.get('control', {}):
            ctrl = _ug405_cfg['control'][scn_name]
            for field in _PLAN_FIELDS:
                if field in ctrl and ctrl[field]:
                    bits = sorted(ctrl[field].keys()) if field in _CTRL_BITMASK else None
                    plan_cols.append({'field': field, 'bits': bits})

        # Pre-parse bitmask strings to int for the editor
        plans_out = {}
        for plan_name, plan in scn_data.get('plans', {}).items():
            offsets = []
            for off in plan.get('offsets', []):
                entry = {'at': off['at']}
                for k in ('fn', 'dn', 'sfn'):
                    if k in off:
                        v = off[k]
                        entry[k] = int(v, 0) if isinstance(v, str) else int(v)
                for k in ('go', 'mo', 'pv', 'px', 'ts', 'so', 'dx', 'fm'):
                    if k in off:
                        entry[k] = int(off[k])
                offsets.append(entry)
            plans_out[plan_name] = {'cycle_secs': plan['cycle_secs'], 'offsets': offsets}

        scns_out[scn_name] = {
            'plan_cols': plan_cols,
            'plans':     plans_out,
        }

    return {'settings': settings, 'timetable': raw.get('timetable', []), 'scns': scns_out}


def _write_offline_plan_cfg(data):
    """Write plan editor data back to offline_plan.json. Service hot-reloads automatically."""
    settings = data.get('settings', {})
    # Convert display base_time "dd/mm/yyyy hh:mm" → "ddmmyyyy hhmm"
    bt = settings.get('base_time', '')
    import re as _re
    m = _re.match(r'(\d{2})/(\d{2})/(\d{4}) (\d{2}):(\d{2})', bt)
    if m:
        settings = dict(settings)
        settings['base_time'] = f'{m.group(1)}{m.group(2)}{m.group(3)} {m.group(4)}{m.group(5)}'

    # Build global timetable — normalise start times (remove colon if present)
    timetable = []
    for entry in data.get('timetable', []):
        timetable.append({
            'days':  entry.get('days', []),
            'start': entry.get('start', '0000').replace(':', ''),
            'plan':  entry.get('plan') or None,
        })

    plan_json = {'settings': settings, 'timetable': timetable, 'scns': {}}
    for scn_name, scn_data in data.get('scns', {}).items():
        plans = {}
        for plan_name, plan in scn_data.get('plans', {}).items():
            offsets = []
            for off in plan.get('offsets', []):
                entry = {'at': int(off['at'])}
                for k in ('fn', 'dn', 'sfn'):
                    if k in off:
                        v = off[k]
                        v = int(v, 0) if isinstance(v, str) else int(v)
                        entry[k] = hex(v)
                for k in ('go', 'mo', 'pv', 'px', 'ts', 'so', 'dx', 'fm'):
                    if k in off:
                        v = off[k]
                        if (int(v, 0) if isinstance(v, str) else int(v)):
                            entry[k] = 1
                offsets.append(entry)
            if plan_name.strip():
                plans[plan_name.strip()] = {'cycle_secs': int(plan['cycle_secs']), 'offsets': offsets}
        plan_json['scns'][scn_name] = {'plans': plans}
    with open(_OFFLINE_PLAN_PATH, 'w') as f:
        json.dump(plan_json, f, indent=2)


@app.route('/api/offline_plan_cfg', methods=['GET'])
@_auth.require_auth
def api_offline_plan_cfg_get():
    try:
        return Response(json.dumps(_parse_offline_plan_cfg()), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=500, mimetype='application/json')


@app.route('/api/offline_plan_cfg', methods=['POST'])
@_auth.require_admin
def api_offline_plan_cfg_post():
    try:
        _write_offline_plan_cfg(request.get_json(force=True))
        log.info("offline_plan.json updated via web UI")
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=400, mimetype='application/json')


_CTRL_BITMASK = {'Fn', 'Dn', 'SFn'}
_REPL_BITMASK = {'Gn', 'SDn', 'LFn', 'CSn', 'VSn', 'BDn', 'TPn', 'GPn', 'SCn'}
_CTRL_SINGLE  = ['DX','GO','FM','TS','SO','MO','PV','PX','SG','LO','LL','TO','HI','CP','EP','FF']
_REPL_SINGLE  = ['MC','CF','DF','GX','FC','HC','WI','CS','RR','FR','SB','LC',
                  'PC','PR','CG','GR1','LE','RF1','RF2','EV','VC','VQ','CA','CR',
                  'CL','TF','VO','CO','EC','MR','MF','ML']


def _parse_ug405_scns():
    # Line-by-line — configparser can't handle multiple [SCN] sections.
    scns = []
    cur = None
    with open(_UG405_CFG_PATH) as f:
        for raw in f:
            line = raw.split('#')[0].strip()
            if not line:
                continue
            if line.upper() == '[SCN]':
                cur = {'name': '', 'control': [], 'reply': []}
                scns.append(cur)
                continue
            if line.startswith('['):
                cur = None
                continue
            if cur is None or '=' not in line:
                continue
            k, v = [x.strip() for x in line.split('=', 1)]
            inverted = k.endswith('!')
            if inverted:
                k = k[:-1].strip()
            v = v.strip()
            if k.lower() == 'name':
                cur['name'] = v
                continue
            import re as _re
            m = _re.match(r'(utcControl|utcReply)([A-Za-z0-9]+?)(?:\[(\d+)\])?$', k)
            if not m:
                continue
            direction, field, bit = m.group(1), m.group(2), m.group(3)
            bit = int(bit) if bit else None
            if direction == 'utcControl':
                cur['control'].append({'field': field, 'bit': bit, 'signal': v})
            else:
                cur['reply'].append({'field': field, 'bit': bit, 'signal': v, 'inverted': inverted})

    return {
        'scns':          scns,
        'ctrl_bitmask':  sorted(_CTRL_BITMASK),
        'ctrl_single':   sorted(_CTRL_SINGLE),
        'repl_bitmask':  sorted(_REPL_BITMASK),
        'repl_single':   sorted(_REPL_SINGLE),
    }


def _write_ug405_scns(scns):
    # Preserve [SERVICES] header and [LIVE] tail exactly.
    with open(_UG405_CFG_PATH) as f:
        lines = f.readlines()

    header, live = [], []
    zone = 'header'
    for line in lines:
        tag = line.split('#')[0].strip().upper()
        if tag == '[SCN]':
            zone = 'scn'
        elif tag == '[LIVE]':
            zone = 'live'
        elif tag.startswith('['):
            zone = 'header' if zone == 'header' else zone

        if zone == 'header':
            header.append(line)
        elif zone == 'live':
            live.append(line)

    out = header[:]
    for scn in scns:
        out += ['\n', '[SCN]\n', f'name = {scn["name"]}\n']
        for r in scn.get('reply', []):
            op  = '!=' if r.get('inverted') else ' ='
            bit = f'[{r["bit"]}]' if r.get('bit') is not None else ''
            out.append(f'utcReply{r["field"]}{bit:<6}{op} {r["signal"]}\n')
        if scn.get('control'):
            out.append('\n')
        for r in scn.get('control', []):
            bit = f'[{r["bit"]}]' if r.get('bit') is not None else ''
            out.append(f'utcControl{r["field"]}{bit:<4}= {r["signal"]}\n')
    out.append('\n\n')
    out.extend(live)
    with open(_UG405_CFG_PATH, 'w') as f:
        f.writelines(out)


@app.route('/api/ug405_scns', methods=['GET'])
@_auth.require_auth
def api_ug405_scns_get():
    try:
        return Response(json.dumps(_parse_ug405_scns()), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=500, mimetype='application/json')


@app.route('/api/ug405_scns', methods=['POST'])
@_auth.require_admin
def api_ug405_scns_post():
    try:
        scns = request.get_json(force=True)
        if not isinstance(scns, list):
            return Response(json.dumps({'error': 'Expected list of SCNs'}),
                            status=400, mimetype='application/json')
        for s in scns:
            if not s.get('name', '').strip():
                return Response(json.dumps({'error': 'Each SCN must have a name'}),
                                status=400, mimetype='application/json')
        # Conflict check: control signals must be unique across all SCNs
        seen = {}
        for s in scns:
            for r in s.get('control', []):
                sig = r.get('signal', '').strip()
                if not sig:
                    continue
                key = f'{r["field"]}[{r.get("bit")}]' if r.get('bit') is not None else r['field']
                if sig in seen:
                    return Response(json.dumps({
                        'error': f'Signal conflict: {sig} used by both '
                                 f'{seen[sig]} and {s["name"]}.utcControl{key}'}),
                        status=400, mimetype='application/json')
                seen[sig] = f'{s["name"]}.utcControl{key}'
        _write_ug405_scns(scns)
        log.info("ug405.cfg SCN mappings updated via web UI (%d SCNs)", len(scns))
        return Response(json.dumps({'ok': True}), mimetype='application/json')
    except Exception as e:
        return Response(json.dumps({'error': str(e)}), status=500, mimetype='application/json')


# ── Startup ───────────────────────────────────────────────────────────────────

# ── MOVA API routes ───────────────────────────────────────────────────────────

@app.route('/mova/stream/<int:n>')
def mova_stream_page(n):
    return _render_page()


@app.route('/api/mova/stream/<int:n>/cmd', methods=['POST'])
def mova_cmd(n):
    if _kernel_registry is None:
        return {'ok': False, 'err': 'no kernel registry'}
    data = request.get_json(force=True)
    cmd  = data.get('cmd', '')
    args = data.get('args', [])
    client = _kernel_registry.get(n)
    if client is None:
        return {'ok': False, 'err': f'stream {n} not found'}
    resp = client.send_command(cmd, *args)
    return resp


@app.route('/api/mova/stream/<int:n>/io_map', methods=['GET'])
def mova_get_io_map(n):
    import sys, os as _os
    try:
        mc_root = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
        if mc_root not in sys.path:
            sys.path.insert(0, mc_root)
        from pci_mova.kernel_main import _load_streams_json
        cfg = _load_streams_json()
        return {'ok': True, 'io': cfg.get(str(n), {}).get('io', {})}
    except Exception as exc:
        return {'ok': False, 'err': str(exc)}


@app.route('/api/mova/stream/<int:n>/io_map', methods=['POST'])
def mova_save_io_map(n):
    import sys, os as _os
    try:
        mc_root = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
        if mc_root not in sys.path:
            sys.path.insert(0, mc_root)
        from pci_mova.kernel_main import _save_streams_json
        data = request.get_json(force=True)
        _save_streams_json({str(n): {'io': data.get('io', {})}})
        return {'ok': True}
    except Exception as exc:
        return {'ok': False, 'err': str(exc)}


@app.route('/api/mova/datasets', methods=['GET'])
def mova_datasets():
    import glob as _glob, os as _os
    ds_dir = _mova_datasets_dir
    if not ds_dir or not _os.path.isdir(ds_dir):
        return []
    files = sorted(_glob.glob(_os.path.join(ds_dir, '*.mxds')))
    return [{'name': _os.path.basename(f), 'path': f} for f in files]


@app.route('/api/mova/dataset_streams', methods=['POST'])
def mova_dataset_streams():
    """Parse a .mxds file and return the available controller stream IDs."""
    import sys, os as _os
    try:
        mc_root = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
        if mc_root not in sys.path:
            sys.path.insert(0, mc_root)
        from pci_mova.dataset.parser import load_all
        data = request.get_json(force=True)
        path = data.get('path', '')
        if not _os.path.isfile(path):
            return {'ok': False, 'err': 'file not found'}
        streams = load_all(path)
        return {'ok': True, 'streams': list(streams.keys())}
    except Exception as exc:
        return {'ok': False, 'err': str(exc)}


@app.route('/api/mova/upload_dataset', methods=['POST'])
def mova_upload_dataset():
    import os as _os
    ds_dir = _mova_datasets_dir
    if not ds_dir:
        return {'ok': False, 'err': 'datasets directory not configured'}
    f = request.files.get('file')
    if not f or not f.filename.endswith('.mxds'):
        return {'ok': False, 'err': 'no .mxds file'}
    dest = _os.path.join(ds_dir, _os.path.basename(f.filename))
    f.save(dest)
    return {'ok': True, 'path': dest}


def start_web(port=12007, _io_bus=None, _mapping=None, _live_cfg=None,
              _op_mode_ref=None, _rtig=None, _conditioner=None,
              _flir_driver=None, _agd_driver=None, _autodim_driver=None,
              _offline_plan_drv=None,
              _mem_limit=0, _xkop=None, _rpdb=None,
              _mova_registry=None, _mova_count=0, _mova_ds_dir=None,
              **kwargs):
    global _io, _ug405_cfg, _live, _op_mode, _rtig_svc, _cond, _flir, _agd, _autodim, \
           _mem_limit_mb, _xkop_driver, _rpdb_driver, _offline_plan, \
           _kernel_registry, _mova_stream_count, _mova_datasets_dir

    if _io_bus           is not None: _io           = _io_bus
    if _mapping          is not None: _ug405_cfg    = _mapping
    if _live_cfg         is not None: _live         = _live_cfg
    if _op_mode_ref      is not None: _op_mode      = _op_mode_ref
    if _rtig             is not None: _rtig_svc     = _rtig
    if _conditioner      is not None: _cond         = _conditioner
    if _flir_driver      is not None: _flir         = _flir_driver
    if _agd_driver       is not None: _agd          = _agd_driver
    if _autodim_driver   is not None: _autodim      = _autodim_driver
    if _offline_plan_drv is not None: _offline_plan = _offline_plan_drv
    if _xkop             is not None: _xkop_driver  = _xkop
    if _rpdb             is not None: _rpdb_driver  = _rpdb
    _mem_limit_mb = _mem_limit
    if _mova_registry is not None:
        _kernel_registry  = _mova_registry
        _mova_stream_count = _mova_count
        if _mova_registry and _mova_count > 0:
            _mova_registry.start_all()
            log.info('MOVA registry started — %d streams', _mova_count)
    if _mova_ds_dir is not None:
        _mova_datasets_dir = _mova_ds_dir

    _auth.init_db()
    if _auth.needs_setup():
        log.warning('Auth DB has no users — visit /setup to create the admin account')

    # Subscribe to bus changes for instant UG405 signal updates
    if _io:
        _io.subscribe(_on_io_change)

    # Background IO/RTIG poller
    threading.Thread(target=_poll_loop, daemon=True, name='WebPoller').start()

    threading.Thread(
        target=lambda: app.run(host='0.0.0.0', port=port,
                               threaded=True, use_reloader=False),
        daemon=True,
        name='WebServer'
    ).start()

    log.info('CM5 dashboard at http://0.0.0.0:%d/', port)
    print(f'[WEB] Dashboard at http://192.168.71.22:{port}/')
