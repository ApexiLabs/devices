const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../include/DashWebUi.h'),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
assert.match(source,/<article class="card"><h2>Live LCD/);
assert.match(source,/<article class="card"><h2>Sensor readings/);
assert.ok(!source.includes('Actual display buffer'));
assert.ok(!source.includes('Logger transmission target:'));
assert.match(source,/Refresh: <strong id="rate">/);
assert.match(source,/#lcd-state\{padding-bottom:12px\}/);
class Node {
  constructor(){this.children=[];this.textContent='';this.value='';this.events={};this.classList={add(){},remove(){}};}
  replaceChildren(){this.children=[];this.textContent='';}
  append(...nodes){this.children.push(...nodes);}
  get options(){return this.children;}
  addEventListener(name,callback){this.events[name]=callback;}
}
const nodes=new Map();
const context=vm.createContext({document:{getElementById(id){if(!nodes.has(id))nodes.set(id,new Node());return nodes.get(id)},createElement(){return new Node()}},location:{pathname:'/'},setTimeout(){}});
vm.runInContext(script.replace(/poll\(\);\s*$/,''),context);
const data={refreshMs:500,slots:['oil_pressure',''],settingsWritable:true,sensors:[{id:'oil_pressure',name:'Oil Pressure',units:'bar',value:4.25,valid:true,fresh:true,warning:false,fault:'none'}]};
context.renderSensors(data);
assert.equal(nodes.get('rate').textContent,'500 ms (2.0 Hz)');
assert.equal(nodes.get('readings').children[0].children[1].textContent,'4.3 bar');
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
assert.equal(nodes.get('readings').children[0].children[1].textContent,'4.3 bar · Stale (held)');
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
  assert.equal(nodes.get('readings').children[0].children[1].textContent,'4.3 bar · Dash offline (held)');
  let disabledChanges=[];
  Object.defineProperty(nodes.get('refresh'),'disabled',{set(value){disabledChanges.push(value)}});
  await context.refresh();assert.deepEqual(disabledChanges,[],'Polling must not flash the button');
  await context.refresh(true);assert.deepEqual(disabledChanges,[true,false],'Manual refresh retains busy feedback');
  console.log('Dash UI tests passed');
}
previewTests().catch(error=>{console.error(error);process.exitCode=1});
