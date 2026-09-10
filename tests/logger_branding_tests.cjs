const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../src/WebUi.cpp'),'utf8');
const branding=fs.readFileSync(path.join(__dirname,'../include/LoggerBranding.h'),'utf8');
const [head,mark]=[...branding.matchAll(/R"BRAND\(([\s\S]*?)\)BRAND"/g)].map(m=>m[1]);
assert.match(head,/family=Inter/);assert.match(head,/system-ui/);
assert.match(mark,/aria-label="ApexiLabs Logger home"/);
for(const p of ['M128 28 L232 216 H24 Z','M128 58 L205 200 H51 Z','M128 28 L232 216 H150 Z'])assert.ok(mark.includes(p));
assert.match(mark,/<em>Logger<\/em>/);
assert.match(source,/return loggerBranding\(htmlStart \+ sensorCardsHtml\(\) \+ htmlEnd\)/);
assert.match(source,/sendLogged\(200, "text\/html", loggerBranding\(html\)\)/);
assert.match(source,/sendLogged\(200,"text\/html",loggerBranding\(R"HTML/);
assert.match(source,/diagnosticsHtml\(\) const \{\s+return loggerBranding/);
assert.equal((source.match(/id="stamp"/g)||[]).length,2);
if(process.argv[2]){
 const directory=process.argv[2];fs.mkdirSync(directory,{recursive:true});
 const dashboard=source.match(/const String htmlStart = R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1]+source.match(/const String htmlEnd = R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];
 const diagnostics=source.match(/diagnosticsHtml\(\) const \{\s+return loggerBranding\(R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1];
 const settings=source.match(/String html = R"rawliteral\(([\s\S]*?)\)rawliteral"/)[1]+'</form></body></html>';
 const logs=source.match(/loggerBranding\(R"HTML\(([\s\S]*?)\)HTML"/)[1];
 for(const [name,raw] of Object.entries({dashboard,diagnostics,settings,logs})){
  let html=raw.replaceAll('</style>','</style>'+head).replace(/<h1>(Apexi Logger(?: Diagnostics| settings)?|System logs)<\/h1>/,()=>'<h1>'+mark+(name==='dashboard'?'':'<span class="page-label">'+({logs:'System logs',settings:'Settings',diagnostics:'Diagnostics'}[name])+'</span>')+'</h1>');
  html=html.replace(/<script>[\s\S]*?<\/script>/g,'').replace('Waiting for data...','2026-09-10 13:30:00 AWST | uptime 00:01:02:03');
  fs.writeFileSync(path.join(directory,name+'.html'),html);
 }
}
console.log('Logger branding tests passed');
