'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const source=fs.readFileSync(require('node:path').join(__dirname,'../chromium-bridge/sync.js'),'utf8');
async function run() {
  const messages=[], queue=[];
  const mk={
    isAuthorized:true,
    isPlaying:true,
    currentPlaybackTime:7,
    api:{music:async (path) => ({data:{data:path.includes('/songs')?[{
      id:'i.EXAMPLE_LIBRARY_ID',
      attributes:{name:'Example Track',artistName:'Example Artist',playParams:{catalogId:'123456789'}}
    }]:[]}})},
    setQueue:async (request)=>{queue.push(request)},
    play:async ()=>{}
  };
  const window={gxTurnwaveSync:(x)=>messages.push(x),
      MusicKit:{getInstance:()=>mk},__twPlayCatalogId:'123456789'};
  const context={window,location:{hostname:'music.apple.com'},
      MusicKit:window.MusicKit,
      console:{info:()=>{},warn:()=>{}},
      setInterval:()=>{},setTimeout:(fn)=>fn(),encodeURIComponent,
      String,Number,Object,Array,Error,Promise};
  vm.runInNewContext(source,context,{timeout:1500});
  await new Promise(r=>setImmediate(r));
  await new Promise(r=>setImmediate(r));
  assert.equal(queue.length,1);
  assert.equal(JSON.stringify(queue[0]),'{"song":"123456789"}');
  assert.ok(messages.includes('BEGIN|2'));
  assert.ok(messages.some(x=>x.includes('ITEM|S|i.EXAMPLE_LIBRARY_ID|123456789|Example%20Track|Example%20Artist')));
  assert.ok(messages.includes('END'));
  assert.ok(messages.includes('PLAY|REQUESTED|123456789'));
  assert.ok(messages.some(x=>x.startsWith('PLAY|STATE|playing|7')));
  console.log('Mock MusicKit: V2 IDs + playback request + status OK');
}
run().catch(error=>{console.error(error);process.exitCode=1});
