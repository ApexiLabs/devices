const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../logger/firmware/src/WebUi.cpp'),'utf8');
const start=source.indexOf("(()=>{const button=document.getElementById('authorizeDevice');");
assert.ok(start>=0);
const script=source.slice(start,source.indexOf('</script>',start));
const nodes=new Map();const calls=[];
function node(id){if(!nodes.has(id))nodes.set(id,{dataset:{csrf:'test-csrf'},textContent:'',disabled:false});return nodes.get(id);}
let ok=true;
vm.runInNewContext(script,{URLSearchParams,document:{getElementById:node},setInterval:()=>{},fetch:async(url,options)=>{
 calls.push({url,options});return {ok:options.method==='POST'?ok:true,json:async()=>({status:'awaiting_approval',user_code:'ABCD-EFGH',expires_in:590,error:''})};
}});
(async()=>{
 await node('authorizeDevice').onclick();
 assert.equal(calls[0].url,'/api/authorization/start');
 assert.equal(calls[0].options.method,'POST');
 assert.equal(calls[0].options.body.toString(),'csrf=test-csrf');
 assert.deepEqual(Object.keys(calls[0].options.headers),['Content-Type']);
 assert.equal(calls[1].options.cache,'no-store');
 assert.equal(node('authorizationCode').textContent,'ABCD-EFGH');
 assert.equal(node('authorizationExpiry').textContent,'Code expires in 590 seconds');
 assert.equal(node('authorizationStatus').textContent,'awaiting approval');
 assert.equal(node('authorizeDevice').disabled,false);
 assert.match(source,/server_\.arg\("csrf"\)!=authorization_->csrfToken\(\)/);
 console.log('Logger authorization UI tests passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
