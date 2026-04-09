#pragma once
#include <pgmspace.h>

static const char indexData[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CFtag — RFID Spool Programmer</title>
<style>
  :root{--blue:#0047AB;--blue2:#1a5cc8;--green:#2e7d32;--red:#c62828;--bg:#f5f7fa;--card:#fff;--border:#dde3ec;--text:#1a1a2e;--muted:#6b7280;}
  *{box-sizing:border-box;margin:0;padding:0;}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
  header{background:var(--blue);color:#fff;padding:14px 20px;display:flex;align-items:center;gap:12px;}
  header h1{font-size:1.1rem;font-weight:600;}
  header button{margin-left:auto;background:rgba(255,255,255,.2);border:none;color:#fff;padding:6px 12px;border-radius:6px;cursor:pointer;font-size:.85rem;}
  .container{max-width:600px;margin:0 auto;padding:16px;}
  .card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;margin-bottom:14px;}
  .card-title{font-size:.7rem;font-weight:700;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin-bottom:12px;}
  .tag-bar{display:flex;align-items:center;gap:10px;flex-wrap:wrap;}
  .tag-uid{font-family:monospace;font-size:1rem;font-weight:700;letter-spacing:.05em;}
  .badge{font-size:.7rem;padding:3px 8px;border-radius:99px;font-weight:600;}
  .badge-enc{background:#fff3cd;color:#856404;}
  .badge-plain{background:#d1ecf1;color:#0c5460;}
  .badge-green{background:#d4edda;color:var(--green);}
  .badge-red{background:#f8d7da;color:var(--red);}
  .tag-data{font-family:monospace;font-size:.72rem;color:var(--muted);word-break:break-all;margin-top:6px;}
  .event-msg{margin-top:8px;font-size:.85rem;font-weight:600;padding:6px 10px;border-radius:6px;}
  .event-ok{background:#d4edda;color:var(--green);}
  .event-err{background:#f8d7da;color:var(--red);}
  .event-read{background:#e8f4fd;color:#0c5460;}
  label{display:block;font-size:.8rem;font-weight:600;color:var(--muted);margin-bottom:4px;margin-top:10px;}
  select,input[type=text],input[type=number],input[type=color],input[type=password]{width:100%;padding:9px 10px;border:1px solid var(--border);border-radius:8px;font-size:.9rem;background:#fff;color:var(--text);}
  input[type=color]{padding:4px;height:38px;}
  .row{display:flex;gap:10px;}
  .row>*{flex:1;}
  button.primary{width:100%;padding:11px;background:var(--blue);color:#fff;border:none;border-radius:8px;font-size:.95rem;font-weight:600;cursor:pointer;margin-top:10px;}
  button.primary:hover{background:var(--blue2);}
  button.primary:disabled{opacity:.5;cursor:not-allowed;}
  button.secondary{width:100%;padding:10px;background:#fff;color:var(--blue);border:2px solid var(--blue);border-radius:8px;font-size:.9rem;font-weight:600;cursor:pointer;margin-top:8px;}
  .sm-status{font-size:.82rem;color:var(--muted);margin-top:8px;padding:6px 10px;border-radius:6px;background:#f0f4ff;}
  .sm-status strong{color:var(--blue);}
  .pending-bar{background:#fff3cd;border:1px solid #ffc107;border-radius:8px;padding:8px 12px;margin-top:10px;font-size:.85rem;color:#856404;}
  dialog{border:none;border-radius:14px;padding:20px;width:min(94vw,460px);box-shadow:0 8px 40px rgba(0,0,0,.18);}
  dialog::backdrop{background:rgba(0,0,0,.4);}
  dialog h2{font-size:1rem;font-weight:700;margin-bottom:14px;}
  dialog button.close{position:absolute;top:14px;right:16px;background:none;border:none;font-size:1.2rem;cursor:pointer;color:var(--muted);}
  .spinner{display:inline-block;width:14px;height:14px;border:2px solid #fff;border-top-color:transparent;border-radius:50%;animation:spin .7s linear infinite;vertical-align:middle;margin-right:6px;}
  @keyframes spin{to{transform:rotate(360deg)}}
  #sm-section{display:none;}
  .tabs{display:flex;gap:0;margin-bottom:14px;border-bottom:2px solid var(--border);}
  .tab{padding:8px 16px;cursor:pointer;font-size:.85rem;font-weight:600;color:var(--muted);border-bottom:2px solid transparent;margin-bottom:-2px;}
  .tab.active{color:var(--blue);border-bottom-color:var(--blue);}
  .tab-panel{display:none;}
  .tab-panel.active{display:block;}
</style>
</head>
<body>
<header>
  <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2"><rect x="2" y="5" width="20" height="14" rx="2"/><path d="M6 9h1M6 12h1M6 15h1M10 9h8M10 12h8M10 15h4"/></svg>
  <h1>CFtag — RFID Spool Programmer</h1>
  <button onclick="document.getElementById('cfg-dialog').showModal()">&#9881; Settings</button>
</header>

<div class="container">

  <!-- Tag status card -->
  <div class="card">
    <div class="card-title">Last Tag</div>
    <div class="tag-bar">
      <span class="tag-uid" id="uid">—</span>
      <span class="badge" id="enc-badge" style="display:none"></span>
      <span class="badge" id="event-badge" style="display:none"></span>
    </div>
    <div class="tag-data" id="tag-data"></div>
    <div id="pending-bar" class="pending-bar" style="display:none">
      ⏳ Ready to write — tap a tag now
      <span id="write-count-msg"></span>
    </div>
  </div>

  <!-- Spoolman section (shown when SM enabled) -->
  <div id="sm-section">
    <div class="card">
      <div class="card-title">Spoolman Integration</div>
      <div class="tabs">
        <div class="tab active" onclick="switchTab('link')">Link Existing Spool</div>
        <div class="tab" onclick="switchTab('create')">Create New Spool</div>
      </div>

      <!-- Tab: link existing -->
      <div class="tab-panel active" id="tab-link">
        <label>Select Spool</label>
        <select id="spool-select"><option value="">Loading spools…</option></select>
        <div id="spool-detail" class="tag-data" style="margin-top:6px;"></div>
        <div id="db-link" style="display:none">
          <label>Brand (from DB)</label>
          <select id="link-db-brand" onchange="filterDbFilaments('link-db-brand','link-db-filament')"><option value="">— brand —</option></select>
          <label>Filament (from DB)</label>
          <select id="link-db-filament" onchange="applyDbFilament('link-db-filament','link-vendor-id','link-filament-id')"><option value="">— filament —</option></select>
        </div>
        <label>Vendor ID (4-char hex)</label>
        <input type="text" id="link-vendor-id" value="0276" maxlength="4" placeholder="0276">
        <label>Filament ID (6-char hex)</label>
        <input type="text" id="link-filament-id" value="101001" maxlength="6" placeholder="101001">
        <div class="row">
          <div>
            <label>Color</label>
            <input type="color" id="link-color" value="#ffffff">
          </div>
          <div>
            <label>Weight (g)</label>
            <input type="number" id="link-weight" value="1000" min="50" max="5000">
          </div>
        </div>
        <button class="primary" id="btn-link" onclick="queueExistingSpool()">⬆ Queue for Writing</button>
        <div class="sm-status" id="link-status" style="display:none"></div>
      </div>

      <!-- Tab: create new -->
      <div class="tab-panel" id="tab-create">
        <div id="db-create" style="display:none">
          <label>Brand (from DB)</label>
          <select id="create-db-brand" onchange="filterDbFilaments('create-db-brand','create-db-filament')"><option value="">— brand —</option></select>
          <label>Filament (from DB)</label>
          <select id="create-db-filament" onchange="applyDbFilamentCreate()"><option value="">— filament —</option></select>
        </div>
        <label>Vendor / Brand</label>
        <input type="text" id="new-vendor" placeholder="e.g. Creality">
        <label>Material</label>
        <input type="text" id="new-material" placeholder="e.g. PLA">
        <label>Color Name</label>
        <input type="text" id="new-color-name" placeholder="e.g. White">
        <div class="row">
          <div>
            <label>Color</label>
            <input type="color" id="new-color" value="#ffffff">
          </div>
          <div>
            <label>Weight (g)</label>
            <input type="number" id="new-weight" value="1000" min="50" max="5000">
          </div>
        </div>
        <label>Vendor ID (tag field, 4-char hex)</label>
        <input type="text" id="new-vendor-id" value="0276" maxlength="4">
        <label>Filament ID (tag field, 6-char hex)</label>
        <input type="text" id="new-filament-id" value="101001" maxlength="6">
        <button class="primary" id="btn-create" onclick="createAndQueueSpool()">
          <span id="create-spinner" class="spinner" style="display:none"></span>
          ✚ Create Spool &amp; Queue for Writing
        </button>
        <div class="sm-status" id="create-status" style="display:none"></div>
      </div>
    </div>
  </div>

  <!-- Manual section -->
  <div class="card">
    <div class="card-title">Manual Write (no Spoolman)</div>
    <div id="db-manual" style="display:none">
      <label>Brand (from DB)</label>
      <select id="m-db-brand" onchange="filterDbFilaments('m-db-brand','m-db-filament')"><option value="">— brand —</option></select>
      <label>Filament (from DB)</label>
      <select id="m-db-filament" onchange="applyDbFilamentManual()"><option value="">— filament —</option></select>
    </div>
    <label>Material Type (3–5 chars)</label>
    <input type="text" id="m-type" value="PLA" maxlength="5">
    <input type="hidden" id="m-filament-id" value="">
    <input type="hidden" id="m-vendor-id" value="">
    <div class="row">
      <div>
        <label>Color</label>
        <input type="color" id="m-color" value="#ffffff">
      </div>
      <div>
        <label>Weight (g)</label>
        <input type="number" id="m-weight" value="1000" min="50" max="5000">
      </div>
    </div>
    <button class="primary" onclick="manualWrite()">✎ Set Tag Data &amp; Write on Tap</button>
    <div class="sm-status" id="manual-status" style="display:none"></div>
  </div>

  <!-- Update section -->
  <div class="card">
    <div class="card-title">Device Updates</div>
    <button class="secondary" onclick="document.getElementById('fw-dialog').showModal()">Upload Firmware (.bin)</button>
    <button class="secondary" onclick="document.getElementById('db-dialog').showModal()">Upload Material DB (.gz)</button>
    <button class="secondary" id="btn-fetchdb" onclick="fetchDb()">Fetch Material DB from Creality Cloud</button>
    <div id="fetchdb-msg" class="sm-status" style="display:none;margin-top:8px;"></div>
  </div>

</div><!-- /container -->

<!-- Config dialog -->
<dialog id="cfg-dialog">
  <button class="close" onclick="this.closest('dialog').close()">&#215;</button>
  <h2>&#9881; Settings</h2>
  <label>AP SSID</label><input type="text" id="c-ap-ssid">
  <label>AP Password</label><input type="password" id="c-ap-pass" value="********">
  <label>WiFi SSID</label><input type="text" id="c-wifi-ssid">
  <label>WiFi Password</label><input type="password" id="c-wifi-pass" value="********">
  <label>Hostname (.local)</label><input type="text" id="c-wifi-host">
  <label>Printer Hostname</label><input type="text" id="c-printer-host">
  <hr style="margin:12px 0;border-color:var(--border)">
  <label style="display:flex;align-items:center;gap:8px;cursor:pointer;">
    <input type="checkbox" id="c-sm-enable" onchange="toggleSmFields(this.checked)">
    Enable Spoolman Integration
  </label>
  <div id="sm-fields">
    <label>Spoolman Host (IP or hostname)</label><input type="text" id="c-sm-host" placeholder="192.168.1.100">
    <label>Spoolman Port</label><input type="number" id="c-sm-port" value="7912">
  </div>
  <button class="primary" onclick="saveConfig()">Save &amp; Reboot</button>
</dialog>

<!-- Firmware update dialog -->
<dialog id="fw-dialog">
  <button class="close" onclick="this.closest('dialog').close()">&#215;</button>
  <h2>Upload Firmware</h2>
  <input type="file" id="fw-file" accept=".bin">
  <button class="primary" onclick="uploadFw()">Upload</button>
  <div id="fw-msg" class="sm-status" style="display:none;margin-top:8px;"></div>
</dialog>

<!-- DB update dialog -->
<dialog id="db-dialog">
  <button class="close" onclick="this.closest('dialog').close()">&#215;</button>
  <h2>Upload Material Database</h2>
  <p style="font-size:.82rem;color:var(--muted);margin-bottom:8px;">Upload a gzip-compressed <code>material_database.json</code> file.</p>
  <input type="file" id="db-file" accept=".gz,.json">
  <button class="primary" onclick="uploadDb()">Upload</button>
  <div id="db-msg" class="sm-status" style="display:none;margin-top:8px;"></div>
</dialog>

<script>
// ── State ──────────────────────────────────────────────────────────────────
let smEnabled = false;
let spools = [];
let lastEventSeen = '';
let matDb = []; // [{id,name,brand,vendorId}]

// ── Material DB ─────────────────────────────────────────────────────────────
function loadMaterialDb() {
  fetch('/material_database.json').then(r=>{
    if (!r.ok) return;
    return r.json();
  }).then(data=>{
    if (!data) return;
    const list = data?.result?.list || data?.list || (Array.isArray(data) ? data : []);
    matDb = list.map(item=>{
      const base = item.base || item;
      return { id: base.id||'', name: base.name||'', brand: base.brand||'', vendorId: base.vendorId||'0276' };
    }).filter(x=>x.id && x.name && x.brand);
    if (matDb.length === 0) return;
    // Show DB pickers and populate brand dropdowns
    ['db-link','db-create','db-manual'].forEach(id=>{
      const el = document.getElementById(id);
      if (el) el.style.display = 'block';
    });
    populateBrandSelect('link-db-brand');
    populateBrandSelect('create-db-brand');
    populateBrandSelect('m-db-brand');
  }).catch(()=>{});
}

function populateBrandSelect(selectId) {
  const sel = document.getElementById(selectId);
  if (!sel) return;
  const brands = [...new Set(matDb.map(x=>x.brand))].sort();
  sel.innerHTML = '<option value="">— brand —</option>';
  brands.forEach(b=>{ const o=document.createElement('option'); o.value=b; o.textContent=b; sel.appendChild(o); });
}

function filterDbFilaments(brandSelId, filamentSelId) {
  const brand = document.getElementById(brandSelId).value;
  const sel = document.getElementById(filamentSelId);
  sel.innerHTML = '<option value="">— filament —</option>';
  const filtered = brand ? matDb.filter(x=>x.brand===brand) : matDb;
  filtered.sort((a,b)=>a.name.localeCompare(b.name)).forEach(x=>{
    const o = document.createElement('option');
    o.value = x.id;
    o.dataset.brand = x.brand;
    o.dataset.name = x.name;
    o.dataset.vendorId = x.vendorId;
    o.textContent = x.name + ' [' + x.id + ']';
    sel.appendChild(o);
  });
}

// Apply selected DB filament to link/create vendor+filament ID fields
function applyDbFilament(filamentSelId, vendorIdFieldId, filamentIdFieldId) {
  const sel = document.getElementById(filamentSelId);
  const opt = sel.options[sel.selectedIndex];
  if (!opt || !opt.value) return;
  document.getElementById(vendorIdFieldId).value = opt.dataset.vendorId || '0276';
  document.getElementById(filamentIdFieldId).value = opt.value;
}

// Apply selected DB filament to "Create New Spool" tab
function applyDbFilamentCreate() {
  const sel = document.getElementById('create-db-filament');
  const opt = sel.options[sel.selectedIndex];
  if (!opt || !opt.value) return;
  document.getElementById('new-vendor-id').value = opt.dataset.vendorId || '0276';
  document.getElementById('new-filament-id').value = opt.value;
  // Also fill vendor name and material from the DB name
  const entry = matDb.find(x=>x.id===opt.value);
  if (entry) {
    document.getElementById('new-vendor').value = entry.brand;
    // Extract material type (first word of name, e.g. "PLA" from "PLA Hyper")
    const mat = entry.name.split(' ')[0];
    document.getElementById('new-material').value = mat;
  }
}

// Apply selected DB filament to manual write section
function applyDbFilamentManual() {
  const sel = document.getElementById('m-db-filament');
  const opt = sel.options[sel.selectedIndex];
  if (!opt || !opt.value) return;
  document.getElementById('m-filament-id').value = opt.value;
  document.getElementById('m-vendor-id').value = opt.dataset.vendorId || '0276';
  const entry = matDb.find(x=>x.id===opt.value);
  if (entry) {
    const mat = entry.name.split(' ')[0].toUpperCase().substring(0,5);
    document.getElementById('m-type').value = mat;
  }
}

loadMaterialDb();

// ── Tab switching ───────────────────────────────────────────────────────────
function switchTab(name) {
  document.querySelectorAll('.tab').forEach((t,i) => t.classList.toggle('active', (i===0&&name==='link')||(i===1&&name==='create')));
  document.getElementById('tab-link').classList.toggle('active', name==='link');
  document.getElementById('tab-create').classList.toggle('active', name==='create');
}

// ── Status polling ──────────────────────────────────────────────────────────
function poll() {
  fetch('/api/status').then(r=>r.json()).then(d=>{
    smEnabled = d.smEnable;
    document.getElementById('sm-section').style.display = smEnabled ? 'block' : 'none';

    // UID
    const uidEl = document.getElementById('uid');
    uidEl.textContent = d.uid || '—';

    // Encrypted badge
    const encBadge = document.getElementById('enc-badge');
    if (d.uid) {
      encBadge.style.display = '';
      encBadge.textContent = d.encrypted ? '🔐 Encrypted' : 'Plain';
      encBadge.className = 'badge ' + (d.encrypted ? 'badge-enc' : 'badge-plain');
    } else { encBadge.style.display = 'none'; }

    // Tag data
    document.getElementById('tag-data').textContent = d.tagData || '';

    // Event badge
    const evBadge = document.getElementById('event-badge');
    if (d.event && d.event !== lastEventSeen) {
      lastEventSeen = d.event;
      evBadge.style.display = '';
      if (d.event === 'write_ok') { evBadge.textContent = '✓ Write OK'; evBadge.className = 'badge badge-green'; }
      else if (d.event === 'write_error') { evBadge.textContent = '✗ Write Error'; evBadge.className = 'badge badge-red'; }
      else if (d.event === 'read') { evBadge.textContent = '◎ Read'; evBadge.className = 'badge badge-plain'; }
      else if (d.event === 'wrong_tag') { evBadge.textContent = '✗ Auth Failed'; evBadge.className = 'badge badge-red'; }
      else { evBadge.style.display = 'none'; }
    }

    // Pending write bar
    const pBar = document.getElementById('pending-bar');
    if (d.pendingWrite) {
      pBar.style.display = 'block';
      const remaining = 2 - d.tagWriteCount;
      document.getElementById('write-count-msg').textContent =
        d.tagWriteCount > 0 ? ` (tag ${d.tagWriteCount} written, tap ${remaining} more)` : '';
    } else { pBar.style.display = 'none'; }
  }).catch(()=>{});
}
setInterval(poll, 1000);
poll();

// ── Config ──────────────────────────────────────────────────────────────────
function loadConfig() {
  fetch('/config').then(r=>r.text()).then(t=>{
    const p = t.split('|-|');
    document.getElementById('c-ap-ssid').value      = p[0]||'';
    document.getElementById('c-wifi-ssid').value    = p[1]||'';
    document.getElementById('c-wifi-host').value    = p[2]||'';
    document.getElementById('c-printer-host').value = p[3]||'';
    const smOn = p[4]==='1';
    document.getElementById('c-sm-enable').checked  = smOn;
    document.getElementById('c-sm-host').value      = p[5]||'';
    document.getElementById('c-sm-port').value      = p[6]||'7912';
    toggleSmFields(smOn);
  });
}
document.getElementById('cfg-dialog').addEventListener('toggle', e=>{ if(e.newState==='open') loadConfig(); });

function toggleSmFields(on) {
  document.getElementById('sm-fields').style.display = on ? 'block' : 'none';
}

function saveConfig() {
  const params = new URLSearchParams({
    ap_ssid:      document.getElementById('c-ap-ssid').value,
    ap_pass:      document.getElementById('c-ap-pass').value,
    wifi_ssid:    document.getElementById('c-wifi-ssid').value,
    wifi_pass:    document.getElementById('c-wifi-pass').value,
    wifi_host:    document.getElementById('c-wifi-host').value,
    printer_host: document.getElementById('c-printer-host').value,
    sm_enable:    document.getElementById('c-sm-enable').checked ? '1' : '0',
    sm_host:      document.getElementById('c-sm-host').value,
    sm_port:      document.getElementById('c-sm-port').value,
  });
  fetch('/config', {method:'POST', body:params});
}

// ── Spoolman: load spool list ────────────────────────────────────────────────
function loadSpools() {
  if (!smEnabled) return;
  fetch('/api/spools').then(r=>r.json()).then(data=>{
    spools = data;
    const sel = document.getElementById('spool-select');
    sel.innerHTML = '<option value="">— Select a spool —</option>';
    data.forEach(s=>{
      const name = s.filament?.name || ('Spool #'+s.id);
      const color = s.filament?.color_hex ? ` [#${s.filament.color_hex}]` : '';
      const opt = document.createElement('option');
      opt.value = s.id;
      opt.textContent = `#${s.id} ${name}${color}`;
      sel.appendChild(opt);
    });
  }).catch(()=>{});
}
document.getElementById('spool-select').addEventListener('change', function() {
  const s = spools.find(x=>String(x.id)===this.value);
  const det = document.getElementById('spool-detail');
  if (s) {
    const f = s.filament||{};
    det.textContent = [f.vendor?.name, f.material, f.name, s.remaining_weight!=null?Math.round(s.remaining_weight)+'g remaining':''].filter(Boolean).join(' · ');
    if (f.color_hex) document.getElementById('link-color').value = '#'+(f.color_hex||'ffffff');
  } else { det.textContent = ''; }
});
setInterval(()=>{ if(smEnabled) loadSpools(); }, 10000);
poll(); // also triggers loadSpools via smEnabled flag
setTimeout(loadSpools, 1500);

// ── Queue existing spool ────────────────────────────────────────────────────
function queueExistingSpool() {
  const spoolId = document.getElementById('spool-select').value;
  if (!spoolId) { showStatus('link-status','Select a spool first','err'); return; }
  const vendorId   = document.getElementById('link-vendor-id').value.padEnd(4,'0').substring(0,4).toUpperCase();
  const filamentId = document.getElementById('link-filament-id').value.padEnd(6,'0').substring(0,6).toUpperCase();
  const colorHex   = document.getElementById('link-color').value.replace('#','');
  const weightG    = document.getElementById('link-weight').value;
  const params = new URLSearchParams({spoolId, vendorId, filamentId, colorHex, weightG});
  fetch('/api/queuespool', {method:'POST', body:params}).then(r=>r.json()).then(d=>{
    if (d.ok) showStatus('link-status','✓ Queued! Tap tag to write.','ok');
    else showStatus('link-status','Error queueing','err');
  });
}

// ── Create new spool ────────────────────────────────────────────────────────
function createAndQueueSpool() {
  const vendor   = document.getElementById('new-vendor').value.trim();
  const material = document.getElementById('new-material').value.trim();
  const colorName= document.getElementById('new-color-name').value.trim() || 'Custom';
  const colorHex = document.getElementById('new-color').value.replace('#','');
  const weightG  = document.getElementById('new-weight').value;
  const vendorId = document.getElementById('new-vendor-id').value.padEnd(4,'0').substring(0,4).toUpperCase();
  const filamentId=document.getElementById('new-filament-id').value.padEnd(6,'0').substring(0,6).toUpperCase();
  if (!vendor||!material) { showStatus('create-status','Fill in vendor and material','err'); return; }
  document.getElementById('btn-create').disabled = true;
  document.getElementById('create-spinner').style.display = 'inline-block';
  const params = new URLSearchParams({vendor, material, colorName, colorHex, weightG, vendorId, filamentId});
  fetch('/api/createspool', {method:'POST', body:params}).then(r=>{
    document.getElementById('btn-create').disabled = false;
    document.getElementById('create-spinner').style.display = 'none';
    if (r.ok) { r.json().then(d=>showStatus('create-status','✓ Spool #'+d.spoolId+' created! Tap tag to write.','ok')); }
    else      { showStatus('create-status','Spoolman error — check settings','err'); }
  });
}

// ── Manual write ────────────────────────────────────────────────────────────
function manualWrite() {
  const materialType  = document.getElementById('m-type').value.trim().toUpperCase().substring(0,5);
  const materialColor = document.getElementById('m-color').value;
  const materialWeight= document.getElementById('m-weight').value;
  const params = new URLSearchParams({materialType, materialColor, materialWeight});
  // Include DB-derived filamentId/vendorId if a DB filament was selected
  const fid = document.getElementById('m-filament-id').value;
  const vid = document.getElementById('m-vendor-id').value;
  if (fid.length === 6) params.set('filamentId', fid);
  if (vid.length === 4) params.set('vendorId', vid);
  fetch('/spooldata', {method:'POST', body:params}).then(r=>{
    if (r.ok) showStatus('manual-status','✓ Set! Tap tag to write.','ok');
    else showStatus('manual-status','Error','err');
  });
}

// ── Firmware upload ─────────────────────────────────────────────────────────
function uploadFw() {
  const f = document.getElementById('fw-file').files[0];
  if (!f) return;
  const fd = new FormData(); fd.append('file', f, f.name.endsWith('.bin') ? f.name : f.name+'.bin');
  fetch('/update.html', {method:'POST', body:fd}).then(r=>r.text()).then(t=>{
    document.getElementById('fw-msg').style.display='block';
    document.getElementById('fw-msg').textContent=t;
  });
}

// ── DB upload ───────────────────────────────────────────────────────────────
function uploadDb() {
  const f = document.getElementById('db-file').files[0];
  if (!f) return;
  const processUpload = (blob) => {
    const fd = new FormData(); fd.append('file', blob, 'material_database.json');
    fetch('/updatedb.html', {method:'POST', body:fd}).then(r=>r.text()).then(t=>{
      document.getElementById('db-msg').style.display='block';
      document.getElementById('db-msg').textContent=t;
    });
  };
  if (f.name.endsWith('.gz')) {
    processUpload(f);
  } else {
    // Compress JSON to gzip before upload
    const cs = new CompressionStream('gzip');
    const writer = cs.writable.getWriter();
    f.arrayBuffer().then(ab=>{ writer.write(ab); writer.close(); });
    new Response(cs.readable).blob().then(processUpload);
  }
}

// ── Fetch DB from Creality Cloud ────────────────────────────────────────────
function fetchDb() {
  const btn = document.getElementById('btn-fetchdb');
  btn.disabled = true;
  showStatus('fetchdb-msg', 'Fetching from Creality Cloud\u2026', 'info');
  fetch('/api/fetchdb').then(r=>r.json()).then(d=>{
    btn.disabled = false;
    if (d.ok) {
      showStatus('fetchdb-msg', '\u2713 Fetched ' + d.count + ' materials. Reloading\u2026', 'ok');
      setTimeout(()=>{ loadMaterialDb(); }, 1500);
    } else {
      showStatus('fetchdb-msg', '\u2717 ' + (d.error||'Unknown error'), 'err');
    }
  }).catch(()=>{
    btn.disabled = false;
    showStatus('fetchdb-msg', '\u2717 Request failed \u2014 check WiFi connection', 'err');
  });
}

// ── Helpers ─────────────────────────────────────────────────────────────────
function showStatus(id, msg, type) {
  const el = document.getElementById(id);
  el.style.display='block';
  el.textContent=msg;
  el.style.background = type==='ok' ? '#d4edda' : type==='err' ? '#f8d7da' : '#e8f4fd';
  el.style.color = type==='ok' ? '#2e7d32' : type==='err' ? '#c62828' : '#0c5460';
}
</script>
</body>
</html>
)rawhtml";
