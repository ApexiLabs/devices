const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require('node:path').join(__dirname, '../logger/firmware/src/WebUi.cpp'), 'utf8');
const queuePolicy = source.match(/function normalUploadQueue\(system\) \{[\s\S]*?\n    \}/)[0];
const issueBlock = source.slice(source.indexOf('const issues = [];'), source.indexOf('if (issues.length)'));
const healthy = {adc_ready:true,upload_enabled:true,upload_connected:true,store_forward_enabled:true,store_forward_ready:true,store_forward_pending_records:2,upload_success_age_ms:500,last_upload_error:'Replaying onboard queue: 2 pending'};
for (const [overrides, expected] of [
  [{},0],
  [{last_upload_error:'Replaying onboard queue: 3 pending'},0],
  [{store_forward_pending_records:1,last_upload_error:'Replaying onboard queue: 1 pending'},0],
  [{store_forward_pending_records:3,last_upload_error:'Replaying onboard queue: 3 pending'},1],
  [{upload_connected:false},1],
  [{upload_success_age_ms:10001},1],
  [{upload_success_age_ms:null},1],
  [{last_upload_error:'HTTPS request failed (401); queued data retained'},1],
  [{store_forward_ready:false},2],
  [{store_forward_error:'Queue persistence failed'},1],
]) {
  const sandbox={data:{sensors:[],system:{...healthy,...overrides}}};
  vm.runInNewContext(queuePolicy+'\n'+issueBlock+'\nglobalThis.issueCount=issues.length;',sandbox);
  assert.equal(sandbox.issueCount,expected,JSON.stringify(overrides));
}
const sensorValueExpression = source.match(/document\.getElementById\('sensor-value-' \+ sensor\.id\)\.textContent = [^;]+;/)[0];
for (const [value, units, expected] of [[0, 'bar', '0.00 bar'], [0.126, 'bar', '0.13 bar'], [8, 'bar', '8.00 bar'], [21.16, 'C', '21.2 C']]) {
  const node = {};
  vm.runInNewContext(sensorValueExpression, {sensor:{id:'test',value,units},document:{getElementById:()=>node}});
  assert.equal(node.textContent, expected);
}
const fileSizeExpression = source.match(/a\.textContent = file\.name \+[^;]+;/)[0];
for (const [size, expected] of [[3516819, '3.52 MB'], [0, '0.00 MB'], [1000000, '1.00 MB']]) {
  const fileContext = {file:{name:'logs-20260908.csv',size},a:{}};
  vm.runInNewContext(fileSizeExpression, fileContext);
  assert.equal(fileContext.a.textContent, `logs-20260908.csv (${expected})`);
}
const html = source.slice(source.indexOf('<h1>Apexi Logger Diagnostics'));
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const nodes = new Map();
let system = {upload_protocol:'https', upload_server:'app-dev.apexilabs.com:443/api/v1/device/loggers/ingest'};
const context = vm.createContext({URL, document:{
  getElementById(id) { if(!nodes.has(id)) nodes.set(id, {}); return nodes.get(id); }
}, fetch:async()=>({json:async()=>({system,sensors:[]})}), setInterval:()=>{}});
vm.runInContext(script.replace('refreshSafely(); setInterval(refreshSafely,1000);',''), context);
for(const [input,expected] of [
  [system.upload_server,'app-dev.apexilabs.com'],
  ['https://example.com:8443/path?q=test','example.com'],
  ['broker.local:1883','broker.local'], ['10.0.40.177:443/path','10.0.40.177'],
  ['[2001:db8::1]:443/path','[2001:db8::1]'], ['', 'Not configured'],
  ['http://[invalid','Invalid server']
]) assert.equal(context.serverHostname(input),expected);
(async()=>{
  for(const [enabled,connected,status,expected,tone] of [
    [true,true,'connected','CONNECTED','ok'],
    [true,false,'scanning','DISCONNECTED','warn'],
    [false,false,'disabled','DISABLED','']
  ]) {
    Object.assign(system,{dash_enabled:enabled,dash_connected:connected,dash_status:status});
    await context.refresh();
    assert.equal(nodes.get('dashStatus').textContent,expected);
    assert.equal(nodes.get('dashStatus').className,'state'+(tone?' '+tone:''));
    assert.equal(nodes.get('dashDetail').textContent,status);
    assert.equal(nodes.get('uploadServer').textContent,'app-dev.apexilabs.com');
  }
  console.log('Logger diagnostics tests passed');
  Object.assign(system,{battery_supported:true,battery_percent:65,battery_voltage:3.95,external_power:true,battery_trend:'rising',battery_state:'likely charging'});
  await context.refresh();
  assert.equal(nodes.get('batteryPercent').textContent,'~65%');
  assert.equal(nodes.get('batteryVoltage').textContent,'3.95 V');
  assert.equal(nodes.get('externalPower').textContent,'Present');
  assert.equal(nodes.get('batteryState').textContent,'likely charging');
  Object.assign(system,{battery_supported:false,battery_percent:null,battery_voltage:null});
  await context.refresh();
  assert.equal(nodes.get('batteryPercent').textContent,'Unavailable');
  assert.equal(nodes.get('externalPower').textContent,'Unsupported');
})().catch(error=>{console.error(error);process.exitCode=1;});
