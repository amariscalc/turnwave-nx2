'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname,'../chromium-bridge/sync.js'),'utf8');

async function scenario(mode,lateBinding=false) {
  const messages=[];
  let tick=null;
  const window={MusicKit:{getInstance:()=>({isAuthorized:false})}};
  if (!lateBinding) window.gxTurnwaveSync=(message)=>messages.push(message);
  const navigator={
    requestMediaKeySystemAccess:async (_system, _config)=>{
      if(mode==='rejected') throw Object.assign(new Error('not supported'),{name:'NotSupportedError'});
      return {
        createMediaKeys:async ()=>{
          if(mode==='create-failed')
            throw Object.assign(new Error('invalid state'),{name:'InvalidStateError'});
          return {};
        }
      };
    }
  };
  const context={window,navigator,location:{hostname:'music.apple.com'},
    console:{info:()=>{},warn:()=>{}},
    setInterval:(fn)=>{tick=fn},setTimeout:()=>{},
    encodeURIComponent,String,Number,Object,Array,Error,Promise};
  vm.runInNewContext(source,context,{timeout:1500});
  for(let i=0;i<8;i++) await Promise.resolve();
  if(lateBinding) {
    assert.equal(messages.length,0);
    window.gxTurnwaveSync=(message)=>messages.push(message);
    assert.equal(typeof tick,'function');
    tick();
    for(let i=0;i<6;i++) await Promise.resolve();
  }
  assert.ok(messages.includes('DIAG|MUSICKIT|unauthorized'));
  assert.ok(messages.includes('DIAG|EME_API|present'));
  if(mode==='rejected') {
    assert.ok(messages.includes('DIAG|WIDEVINE_ACCESS|NotSupportedError'));
    assert.ok(!messages.some(x=>x.startsWith('DIAG|CDM_CREATE|')));
  } else {
    assert.ok(messages.includes('DIAG|WIDEVINE_ACCESS|supported'));
    assert.ok(messages.includes('DIAG|CDM_CREATE|starting'));
    assert.ok(messages.includes(mode==='create-failed'
      ?'DIAG|CDM_CREATE|InvalidStateError'
      :'DIAG|CDM_CREATE|success'));
  }
}
(async ()=>{
  await scenario('success');
  await scenario('rejected');
  await scenario('create-failed');
  await scenario('success',true);
  console.log('EME diagnostic: unauthorized, success, rejection, initialization failure and late CDP binding OK');
})().catch(e=>{console.error(e);process.exitCode=1});
