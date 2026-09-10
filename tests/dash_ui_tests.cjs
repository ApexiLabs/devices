const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../include/DashWebUi.h'),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
assert.match(source,/<article class="card"><h2>Live LCD/);
assert.match(source,/<article class="card settings-card"><h2 id="readings-heading">Sensor readings/);
assert.ok(!source.includes('card wide'));
assert.ok(!source.includes('Configure LCD readings'));
assert.match(source,/download="dash-diagnostics.log"/);
assert.equal((source.match(/<details class="help">/g)||[]).length,3);
assert.match(source,/font-family:Inter/);
assert.match(source,/M128 28 L232 216 H24 Z/);
assert.match(script,/\(browser\) \| uptime/);
assert.ok(!source.includes('Actual display buffer'));
assert.ok(!source.includes('Logger transmission target:'));
assert.match(source,/Refresh: <strong id="rate">/);
assert.match(source,/#lcd-state\{padding-bottom:12px\}/);
class Node {
  constructor(){this.children=[];this.textContent='';this.value='';this.events={};this.classList={add(){},remove(){},toggle(){}};}
  setAttribute(key,value){this[key]=value;}
  replaceChildren(){this.children=[];this.textContent='';}
  append(...nodes){this.children.push(...nodes);}
  get options(){return this.children;}
  addEventListener(name,callback){this.events[name]=callback;}
}
const nodes=new Map();
const context=vm.createContext({document:{body:new Node(),addEventListener(){},getElementById(id){if(!nodes.has(id))nodes.set(id,new Node());return nodes.get(id)},createElement(){return new Node()}},location:{pathname:'/'},setTimeout(){}});
vm.runInContext(script.replace(/poll\(\);\s*$/,''),context);
for(const [value,label,level] of [['accepted','Server accepted','ok'],['unconfirmed','Sent · unconfirmed','warn'],['failed','Upload failed','bad'],['disabled','Disabled',''],['stale','Status stale',''],['unknown','Unknown','']]){
  context.renderUpload({state:value});assert.equal(nodes.get('upload-state').textContent,label);assert.equal(nodes.get('upload-state').className,'upload-indicator '+level);
}
context.renderUpload();assert.equal(nodes.get('upload-state').textContent,'Unknown');
context.renderBattery({battery_percent:80,battery_voltage:4.05,battery_trend:'steady'});
assert.equal(nodes.get('batteryPercent').textContent,'~80%');assert.equal(nodes.get('batteryVoltage').textContent,'4.05 V');
assert.ok(!source.includes('id="externalPower"'));assert.ok(!source.includes('id="batteryState"'));
context.renderBattery(null);assert.equal(nodes.get('batteryPercent').textContent,'Unavailable');assert.equal(nodes.get('batteryVoltage').textContent,'--');
assert.match(script,/renderUpload\(s.bleConnected&&s.loggerReady\?s.upload:null\)/);
assert.match(script,/catch\(e\)\{lcdUnavailable[\s\S]*?renderUpload\(null\)/);
const data={refreshMs:500,slots:['oil_pressure',''],settingsWritable:true,sensors:[{id:'oil_pressure',name:'Oil Pressure',units:'bar',value:4.25,valid:true,fresh:true,warning:false,fault:'none'}]};
context.renderSensors(data);
assert.equal(nodes.get('rate').textContent,'500 ms (2.0 Hz)');
context.renderSensors({...data,sensors:[{id:'oil_temperature',name:'Oil Temp',units:'C',value:90,valid:true,fresh:true}]});
assert.equal(nodes.get('readings').children[0].children[1].textContent,'90.0 °C');
context.renderSensors(data);
assert.equal(nodes.get('readings').children[0].children[1].textContent,'4.25 bar');
assert.equal(nodes.get('slot0').value,'oil_pressure');
nodes.get('slot0').value='-';nodes.get('refresh-ms').value=2000;
data.sensors[0].valid=false;data.sensors[0].value=null;data.sensors[0].fault='adc_unavailable';
context.renderSensors(data);
assert.equal(nodes.get('readings').children[0].children[1].textContent,'adc unavailable');
assert.equal(nodes.get('slot0').value,'-');assert.equal(nodes.get('refresh-ms').value,2000);
data.sensors[0].fresh=false;context.renderSensors(data);
assert.equal(nodes.get('readings').children[0].children[1].textContent,'Unavailable — stale');
data.sensors=[];context.renderSensors(data);
context.renderSensors({...data,sensors:[{id:'oil_pressure',name:'Oil Pressure',units:'bar',valid:false,fresh:false,lastGoodValue:4.25,displayState:'Stale'}]});
assert.equal(nodes.get('readings').children[0].children[1].textContent,'4.25 bar · Stale (held)');
assert.equal(nodes.get('readings').children[0].children[1].className,'warn');
context.renderSensors(data);
assert.equal(nodes.get('readings').textContent,'Waiting for Logger telemetry…');
async function previewTests(){
  let calls=0,now=10000,frame=1;
  context.Date=class extends Date {static now(){return now}};
  context.AbortSignal={timeout(){return undefined}};
  context.URL={createObjectURL(){return 'blob:test'},revokeObjectURL(){}};
  context.Image=class {constructor(){this.naturalWidth=240;this.naturalHeight=240}async decode(){}};
  context.fetch=async()=>{calls++;return {ok:true,status:200,blob:async()=>({}),headers:{get(name){return name==='X-LCD-Boot'?'boot':String(frame)}}}};
  const status={lcdBuffered:true,lcdBootId:'boot',lcdFrameCount:1};
  await context.updateLcd(status);assert.equal(calls,1);assert.equal(nodes.get('lcd-preview').hidden,false);
  now+=2000;await context.updateLcd(status);assert.equal(calls,1,'Unchanged frame is not downloaded');
  frame=2;status.lcdFrameCount=2;await context.updateLcd(status);assert.equal(calls,2);
  frame=3;status.lcdFrameCount=3;await context.updateLcd(status);assert.equal(calls,2,'Rate capped');
  nodes.get('lcd-pause').events.click();now+=2000;await context.updateLcd(status);assert.equal(calls,2,'Paused');
  nodes.get('lcd-pause').events.click();await context.updateLcd(status);assert.equal(calls,3,'Resumed');
  context.document.hidden=true;now+=2000;status.lcdFrameCount=4;await context.updateLcd(status);assert.equal(calls,3,'Hidden tab');
  context.document.hidden=false;context.fetch=async()=>{throw Error('offline')};
  await context.updateLcd(status);assert.match(nodes.get('lcd-state').textContent,/stale/);
  vm.runInContext("lastStatus={refreshMs:500,slots:['',''],settingsWritable:true,sensors:[{id:'p',name:'Pressure',units:'bar',valid:true,value:4.25}]}",context);
  await context.refresh();
  assert.equal(nodes.get('readings').children[0].children[1].textContent,'4.25 bar · Dash offline (held)');
  let disabledChanges=[];
  Object.defineProperty(nodes.get('refresh'),'disabled',{set(value){disabledChanges.push(value)}});
  await context.refresh();assert.deepEqual(disabledChanges,[],'Polling must not flash the button');
  await context.refresh(true);assert.deepEqual(disabledChanges,[true,false],'Manual refresh retains busy feedback');
  console.log('Dash UI tests passed');
}
previewTests().catch(error=>{console.error(error);process.exitCode=1});

const settingsNodes=new Map();
const settingsContext=vm.createContext({document:{body:new Node(),addEventListener(){},getElementById(id){if(!settingsNodes.has(id))settingsNodes.set(id,new Node());return settingsNodes.get(id)},createElement(){return new Node()}},location:{pathname:'/settings'},setTimeout(){},fetch:async()=>({ok:true,json:async()=>({csrf:'test'})})});
vm.runInContext(script.replace(/poll\(\);\s*$/,''),settingsContext);
assert.equal(settingsNodes.get('settings-nav')['aria-current'],'page');
assert.equal(settingsNodes.get('readings-heading').textContent,'Display settings');
assert.equal(nodes.get('dashboard-nav')['aria-current'],'page');
const rule={id:'oil_pressure',units:'bar',points:[0,2,6,8],low:1,high:7,lowEnabled:false,highEnabled:false};
settingsContext.renderGauges({gauges:[rule],sensors:[]});
assert.equal(settingsContext.readGaugeRules()[0].highEnabled,false);
vm.runInContext('gaugeControls[0].point1.value=3',settingsContext);
settingsContext.renderGauges({gauges:[rule],sensors:[{id:'new_sensor',name:'New',units:'C'}]});
assert.equal(settingsContext.readGaugeRules().length,2,'Late sensor gets a rule without losing edits');
assert.equal(settingsContext.readGaugeRules()[0].points[1],3);
vm.runInContext('gaugeControls[0].point1.value=0',settingsContext);
assert.throws(()=>settingsContext.readGaugeRules(),/increase/);
vm.runInContext('gaugeControls[0].point1.value=3;gaugeControls[0].lowEnabled.checked=true;gaugeControls[0].highEnabled.checked=true;gaugeControls[0].low.value=9',settingsContext);
assert.throws(()=>settingsContext.readGaugeRules(),/below/);
vm.runInContext('gaugeControls[0].remove.checked=true',settingsContext);
assert.equal(settingsContext.readGaugeRules().length,1);
context.renderSensors({...data,sensors:[{id:'x',name:'Hidden Sensor',units:'bar',valid:true,fresh:true,value:9,alarm:'HIGH'}]});
assert.equal(nodes.get('alarm-state').hidden,false);
assert.equal(nodes.get('alarm-state').textContent,'Alarm: Hidden Sensor HIGH');
context.renderSensors({...data,sensors:[{id:'x',name:'X',valid:false,fresh:false,alarm:'HIGH'}]});
assert.equal(nodes.get('alarm-state').hidden,true,'Stale data cannot show alarm');
