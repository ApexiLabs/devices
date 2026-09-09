#pragma once

// Match Logger's existing tokens and card density; show only live Dash features.
inline constexpr char kDashWebUi[] = R"html(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<link rel="icon" href="data:,"><title>Apexi Dash</title><style>
:root{--bg:#09131f;--surface:#111d2a;--border:#29394a;--text:#ecf2f8;--muted:#95a8ba;--accent:#6dd6ff;--ok:#73d5a2;--warn:#f4c46c;--bad:#ff8d8d}
*{box-sizing:border-box}body{margin:0;font-family:"Segoe UI",system-ui,sans-serif;background:var(--bg);color:var(--text)}
header{display:flex;align-items:flex-start;justify-content:space-between;gap:16px;padding:18px 20px;background:#102235;border-bottom:1px solid var(--border)}
h1{margin:0;font-size:1.4rem}.header-meta{margin-top:4px;color:var(--muted);font-size:.88rem}
main{width:min(100%,72rem);margin:auto;padding:16px}.grid{display:grid;gap:12px;grid-template-columns:repeat(2,minmax(0,1fr))}
.card{min-width:0;background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:14px}
h2{margin:0;color:var(--muted);font-size:.78rem;font-weight:700;letter-spacing:.08em;text-transform:uppercase}
.value{font-size:1rem;font-weight:600;margin-top:8px}.status{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;margin-top:11px;font-size:.94rem}
.status>:last-child{min-width:0;text-align:right;overflow-wrap:anywhere}.state{font-size:.78rem;font-weight:700;letter-spacing:.04em}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}.summary{margin:10px 0 0;color:var(--muted);line-height:1.45}
button{font:inherit;background:transparent;padding:9px 12px;border:1px solid var(--border);border-radius:8px;color:var(--text);cursor:pointer}
button:hover{background:var(--surface)}button:active{border-color:var(--accent)}button:disabled{opacity:.5;cursor:wait}button:focus-visible{outline:2px solid var(--accent);outline-offset:3px}
code{font-size:.86rem;overflow-wrap:anywhere}.wide{grid-column:1/-1}
a{color:var(--accent)}a:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--accent);outline-offset:3px}
label{display:block;margin:14px 0 6px}select,input{width:100%;padding:10px;background:var(--bg);color:var(--text);border:1px solid var(--border);border-radius:8px;font:inherit}
.reading{display:flex;justify-content:space-between;gap:16px;padding:12px 0;border-bottom:1px solid var(--border)}.reading:last-child{border-bottom:0}.reading strong{min-width:0;text-align:right;overflow-wrap:anywhere}
[hidden]{display:none!important}form button{margin-top:16px}
#lcd-state{padding-bottom:12px}
.gauge-fields{border:1px solid var(--border);margin:18px 0;padding:12px;min-width:0}.gauge-fields legend{padding:0 6px;font-weight:600}.colour-points{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.check{display:flex;gap:8px;align-items:center}.check input{width:auto}.alarm-banner{padding:10px;border:1px solid var(--bad);color:var(--bad);margin:12px 0}.gauge-fields input{min-width:0}
.grid:has(#display-form:not([hidden])){align-items:start}
.lcd-preview{display:block;width:240px;max-width:100%;aspect-ratio:1;margin:18px auto;background:#000;border-radius:50%;border:1px solid var(--border);image-rendering:pixelated}.lcd-preview.stale{opacity:.35}.lcd-actions{display:flex;align-items:center;gap:16px;flex-wrap:wrap}
@media(max-width:620px){header{padding:16px;flex-direction:column}main{padding:12px}.grid{grid-template-columns:1fr}}
</style></head><body><header><div><h1>Apexi Dash</h1><div class="header-meta" id="stamp" role="status">Connecting to Dash…</div></div><button id="refresh" type="button">Refresh status</button></header>
<main><section class="grid" aria-label="Dash status">
<article class="card"><h2>Live LCD</h2><img id="lcd-preview" class="lcd-preview" width="240" height="240" alt="Live rendered image from Dash's round LCD" hidden>
<p id="lcd-state" class="summary" role="status">Waiting for LCD image…</p><div class="lcd-actions"><button id="lcd-pause" type="button">Pause preview</button><a href="/api/lcd.bmp" target="_blank" rel="noopener">Open snapshot</a></div>
</article>
<article class="card"><h2>Sensor readings</h2><div id="readings">Waiting for Logger telemetry…</div>
<p id="alarm-state" class="alarm-banner" role="status" hidden></p>
<p class="summary">Refresh: <strong id="rate">—</strong></p>
<p><a href="/settings">Configure LCD readings</a></p>
<form id="display-form" hidden><p class="summary">Show one or two large values. Set either reading to Hidden for one centred, full-screen value. Keep both active for two large stacked values.</p><label for="slot0">First LCD reading</label><select id="slot0"></select>
<label for="slot1">Second LCD reading</label><select id="slot1"></select>
<label for="refresh-ms">Display refresh interval (milliseconds)</label><input id="refresh-ms" type="number" min="250" max="5000" step="250" required>
<p class="summary">Applies to LCD redraws and this page's polling, not sensor sampling. Settings are stored on Dash and survive reboot. Sign in as admin using the OTA password.</p>
<h2>Colours and alarms</h2><p class="summary">Set four increasing colour points in each sensor's units. Arcs keep their size and blend between these colours. Low/high alarms are separate, inclusive limits and start disabled. Only fresh valid readings trigger alarms, including sensors hidden from the LCD.</p>
<div id="gauge-settings"></div>
<button type="submit" id="save">Save display settings</button><p id="save-state" role="status"></p></form></article>
<article class="card"><h2>Dash Link</h2><div class="value" id="link">Checking…</div>
<div class="status"><span>Bluetooth</span><span class="state" id="ble">—</span></div>
<div class="status"><span>Logger handshake</span><span class="state" id="handshake">—</span></div>
<p class="summary">Logger connects automatically when it is available.</p>
<p><a href="/api/diagnostics" target="_blank" rel="noopener" download="dash-diagnostics.json">Download troubleshooting log</a></p>
<p class="summary">Logging is always on: latest 64 sensor-state transitions plus receive/link counters. RAM only; download before reboot or OTA.</p></article>
<article class="card"><h2>Connectivity</h2><div class="status"><span>IoT Wi-Fi</span><span class="state" id="wifi">—</span></div>
<div class="status"><span>Station IP</span><span id="ip">—</span></div><div class="status"><span>Recovery AP IP</span><span id="ap">—</span></div>
<div class="status"><span>Uptime</span><span id="uptime">—</span></div></article>
<article class="card wide"><h2>Firmware updates</h2><div class="status"><span>Password-protected OTA</span><span class="state" id="ota">—</span></div>
<div class="status"><span>Hostname</span><span>apexi-dash.local</span></div><div class="status"><span>Build</span><span id="build">—</span></div>
<p class="summary">Use the repository's <code>scripts/upload-dash-ota.py</code> helper from a trusted network. Updates restart Dash. Keep power connected throughout the update.</p></article>
</section></main><script>
const el=id=>document.getElementById(id);let busy=false,interval=1000,configured=false,catalogKey='',csrf='',lastStatus=null;
let gaugeControls=[] ,gaugesConfigured=false;
let lcdKey='',lcdBusy=false,lcdPaused=false,lcdUrl='',lcdLastFetch=0,lcdGeneration=0;
function lcdUnavailable(message){lcdGeneration++;lcdKey='';el('lcd-preview').classList.add('stale');el('lcd-state').textContent=message}
el('lcd-pause').addEventListener('click',()=>{lcdPaused=!lcdPaused;el('lcd-pause').textContent=lcdPaused?'Resume preview':'Pause preview';if(lcdPaused)lcdUnavailable('Preview paused — last captured image');else{lcdKey='';el('lcd-state').textContent='Resuming preview…'}});
async function updateLcd(s){
if(lcdPaused)return;if(!s.lcdBuffered){lcdUnavailable('LCD buffer unavailable');return}
const key=s.lcdBootId+'-'+s.lcdFrameCount;
if(lcdBusy||key===lcdKey||Date.now()-lcdLastFetch<1000||document.hidden)return;
lcdBusy=true;lcdLastFetch=Date.now();const generation=lcdGeneration;
try{const r=await fetch('/api/lcd.bmp',{cache:'no-store',signal:AbortSignal.timeout(5000)});
if(r.status===429)return;if(!r.ok)throw Error();const blob=await r.blob();const url=URL.createObjectURL(blob);
try{const image=new Image();image.src=url;await image.decode();if(image.naturalWidth!==240||image.naturalHeight!==240)throw Error();
if(generation!==lcdGeneration||lcdPaused){URL.revokeObjectURL(url);return}
el('lcd-preview').src=url;el('lcd-preview').hidden=false;el('lcd-preview').classList.remove('stale');if(lcdUrl)URL.revokeObjectURL(lcdUrl);lcdUrl=url;
lcdKey=r.headers.get('X-LCD-Boot')+'-'+r.headers.get('X-LCD-Frame');el('lcd-state').textContent='Live · frame '+r.headers.get('X-LCD-Frame')+' · captured '+new Date().toLocaleTimeString();
}catch(e){URL.revokeObjectURL(url);throw e}
}catch(e){lcdUnavailable('Preview unavailable — retrying. Last image may be stale.')}finally{lcdBusy=false}}
const editing=location.pathname==='/settings';el('display-form').hidden=!editing;
if(editing)fetch('/api/settings',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error();return r.json()}).then(s=>{csrf=s.csrf}).catch(()=>{el('save-state').textContent='Cannot authorize settings. Reload and sign in.'});
function renderSensors(s){
const alarms=s.sensors.filter(v=>v.valid&&v.fresh&&v.alarm).map(v=>v.name+' '+v.alarm);
el('alarm-state').hidden=!alarms.length;el('alarm-state').textContent=alarms.length?'Alarm: '+alarms.join(' · '):'';
renderGauges(s);
el('rate').textContent=s.refreshMs+' ms ('+(1000/s.refreshMs).toFixed(1)+' Hz)';interval=s.refreshMs;
const box=el('readings');box.replaceChildren();
if(!s.sensors.length)box.textContent='Waiting for Logger telemetry…';
for(const v of s.sensors){const row=document.createElement('div');row.className='reading';const name=document.createElement('span');name.textContent=v.name;const value=document.createElement('strong');
value.textContent=v.valid?v.value.toFixed(1)+' '+v.units:Number.isFinite(v.lastGoodValue)?v.lastGoodValue.toFixed(1)+' '+v.units+' · '+(v.displayState||'Stale')+' (held)':!v.fresh?'Unavailable — stale':v.fault==='none'?'No valid sample':v.fault.replaceAll('_',' ');
value.className=v.valid&&v.alarm?'bad':v.valid?'ok':'warn';row.append(name,value);box.append(row)}
const key=JSON.stringify(s.sensors.map(v=>[v.id,v.name]));
if(!configured||key!==catalogKey){for(let i=0;i<2;i++){const select=el('slot'+i),selected=configured?select.value:s.slots[i];select.replaceChildren();
for(const [id,label] of [['','Automatic (sensor '+(i+1)+')'],['-','Hidden'],...s.sensors.map(v=>[v.id,v.name])]){const o=document.createElement('option');o.value=id;o.textContent=label;select.append(o)}
if(selected&&!Array.from(select.options).some(o=>o.value===selected)){const o=document.createElement('option');o.value=selected;o.textContent='Unavailable: '+selected;select.append(o)}select.value=selected}
catalogKey=key}
if(!configured)el('refresh-ms').value=s.refreshMs;configured=true;el('save').disabled=!s.settingsWritable;
}
function renderGauges(s){
if(!editing||!Array.isArray(s.gauges))return;
const rules=s.gauges.map(r=>({...r}));
for(const sensor of s.sensors)if(rules.length<8&&!rules.some(r=>r.id===sensor.id))rules.push({id:sensor.id,units:sensor.units,points:[0,33,66,100],low:0,high:100,lowEnabled:false,highEnabled:false});
if(!rules.length){el('gauge-settings').textContent='Connect Logger to configure sensor rules.';return}
if(!gaugesConfigured){gaugesConfigured=true;gaugeControls=[];el('gauge-settings').replaceChildren();}
rules.filter(r=>!gaugeControls.some(f=>f.id===r.id)).slice(0,8-gaugeControls.length).forEach(r=>{
 const index=gaugeControls.length;
 const group=document.createElement('fieldset');group.className='gauge-fields';const legend=document.createElement('legend');
 const sensor=s.sensors.find(v=>v.id===r.id);legend.textContent=(sensor?sensor.name:r.id)+' ('+(r.units||'unitless')+')';group.append(legend);
 const fields={id:r.id,units:r.units};
 const number=(parent,key,label,value)=>{const wrap=document.createElement('div'),l=document.createElement('label'),input=document.createElement('input');input.id='gauge-'+index+'-'+key;l.htmlFor=input.id;l.textContent=label;input.type='number';input.step='any';input.min=-1000000;input.max=1000000;input.required=true;input.value=value;fields[key]=input;wrap.append(l,input);parent.append(wrap)};
 const points=document.createElement('div');points.className='colour-points';['Blue','Green','Yellow','Red'].forEach((label,j)=>number(points,'point'+j,label+' at',r.points[j]));group.append(points);
 for(const side of ['low','high']){const label=document.createElement('label');label.className='check';const input=document.createElement('input');input.type='checkbox';input.checked=r[side+'Enabled'];fields[side+'Enabled']=input;const text=document.createElement('span');text.textContent='Enable '+side+' alarm';label.append(input,text);group.append(label);number(group,side,side==='low'?'Alarm at or below':'Alarm at or above',r[side]);}
 const help=document.createElement('p');help.className='summary';help.textContent='Rule: '+r.id+'. Review colour points and alarm limits for your sensor; these are not safety-certified limits.';group.append(help);
 if(sensor&&sensor.units!==r.units){help.textContent+=' Units changed: remove this rule and save, then reload to configure the new units.';}
 const removeLabel=document.createElement('label');removeLabel.className='check';const remove=document.createElement('input');remove.type='checkbox';const removeText=document.createElement('span');removeText.textContent='Remove this rule';removeLabel.append(remove,removeText);group.append(removeLabel);fields.remove=remove;
 gaugeControls.push(fields);el('gauge-settings').append(group);
});
}
function readGaugeRules(){return gaugeControls.filter(f=>!f.remove.checked).map(f=>{
const n=key=>{if(String(f[key].value).trim()==='')throw Error('Enter a number for '+f.id+' '+key);const v=Number(f[key].value);if(!Number.isFinite(v)||Math.abs(v)>1000000)throw Error('Invalid number for '+f.id);return v};
const r={id:f.id,units:f.units,points:[0,1,2,3].map(i=>n('point'+i)),low:n('low'),high:n('high'),lowEnabled:f.lowEnabled.checked,highEnabled:f.highEnabled.checked};
if(r.points.some((v,i)=>i&&v<=r.points[i-1]))throw Error(f.id+': colour points must increase blue → green → yellow → red');
if(r.lowEnabled&&r.highEnabled&&r.low>=r.high)throw Error(f.id+': low alarm must be below high alarm');return r;
})}
el('display-form').addEventListener('submit',async e=>{e.preventDefault();el('save').disabled=true;el('save-state').textContent='Saving…';try{
const payload={csrf,refreshMs:Number(el('refresh-ms').value),slots:[el('slot0').value,el('slot1').value]};if(gaugesConfigured)payload.gauges=readGaugeRules();
const r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
if(!r.ok)throw Error(await r.text());configured=false;el('save-state').textContent='Saved on Dash.';await refresh();
}catch(err){el('save-state').textContent='Not saved: '+err.message}finally{el('save').disabled=false}});
function state(id,text,level){el(id).textContent=text;el(id).className='state '+level}
async function refresh(manual=false){if(busy)return;busy=true;if(manual)el('refresh').disabled=true;
try{const r=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(5000)});if(!r.ok)throw Error();const s=await r.json();
renderSensors(s);
lastStatus=s;
updateLcd(s);
el('link').textContent=s.loggerReady?'Connected OK':s.bleConnected?'Handshaking':'Waiting for Logger';
state('ble',s.bleConnected?'CONNECTED':'DISCONNECTED',s.bleConnected?'ok':'warn');state('handshake',s.loggerReady?'READY':'WAITING',s.loggerReady?'ok':'warn');
state('wifi',s.wifiConnected?'CONNECTED':'OFFLINE',s.wifiConnected?'ok':'warn');el('ip').textContent=s.stationIp||'Not assigned';el('ap').textContent=s.apIp;
state('ota',s.otaReady?'READY':s.otaEnabled?'WAITING FOR WI-FI':'DISABLED',s.otaReady?'ok':'warn');
el('build').textContent=s.build;el('uptime').textContent=Math.floor(s.uptimeSeconds/60)+'m '+s.uptimeSeconds%60+'s';
el('stamp').textContent='Live · updated '+new Date().toLocaleTimeString();
}catch(e){lcdUnavailable('Dash unreachable — last image may be stale.');el('stamp').textContent='Dash unreachable — reconnecting. Last good readings held.';
if(lastStatus)renderSensors({...lastStatus,sensors:lastStatus.sensors.map(v=>({...v,valid:false,fresh:false,lastGoodValue:v.valid?v.value:v.lastGoodValue,displayState:'Dash offline'}))});else el('readings').textContent='Unavailable — Dash unreachable';
el('link').textContent='Unavailable';for(const id of ['ble','handshake','wifi','ota'])state(id,'UNKNOWN','warn');for(const id of ['ip','ap','uptime','build'])el(id).textContent='—';}
finally{busy=false;if(manual)el('refresh').disabled=false}}
el('refresh').addEventListener('click',()=>refresh(true));async function poll(){await refresh();setTimeout(poll,interval)}poll();
</script></body></html>)html";
