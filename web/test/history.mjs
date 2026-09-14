import {chromium} from '@playwright/test';
import {createServer} from 'node:http';import {readFile} from 'node:fs/promises';import assert from 'node:assert/strict';
import {historyWarnings} from '../chat-history.mjs';
assert.ok(historyWarnings({storage:'ram'}).length);assert.ok(historyWarnings({full:true,long:true}).some(t=>t.includes('เต็ม')));
let records=Array.from({length:12},(_,i)=>({id:i+1,role:i%2?'model':'user',text:i===11?'<img src=x onerror=alert(1)>':'ข้อความทดสอบ '+(i+1),time:0,uptime:i*10,partial:i===10,truncated:false})),resetCalls=0,resetting=false,revision=0;
const state={device:'test',settings:{colors:[]},ai:{provider:'gemini-live'}};
const server=createServer(async(req,res)=>{try{const u=new URL(req.url,'http://test');let data;
 if(u.pathname.startsWith('/api/')){
  let raw='';for await(const c of req)raw+=c;
  if(u.pathname==='/api/session')data=req.method==='POST'?{token:'test'}:{setup:false};
  else if(u.pathname==='/api/status')data=state;
  else if(u.pathname==='/api/history/reset'){assert.equal(new URLSearchParams(raw).get('confirm'),'reset');++resetCalls;resetting=true;setTimeout(()=>{records=[];resetting=false;++revision;},400);data={ok:true};}
  else if(u.pathname==='/api/history'){const before=+u.searchParams.get('before')||records.length,end=Math.min(before,records.length),start=Math.max(0,end-4);data={storage:'sd',count:records.length,bytes:records.length*120,maxBytes:1048576,long:records.length>8,resetting,resetRevision:revision,before:start,hasOlder:start>0,...(u.searchParams.has('summary')?{}:{messages:records.slice(start,end)})};}
  else data={ok:true};res.setHeader('Content-Type','application/json');res.end(JSON.stringify(data));return;
 }
 if(u.pathname==='/favicon.ico'){res.writeHead(204);res.end();return;}
 const path=u.pathname==='/'?'index.html':u.pathname.slice(1);res.setHeader('Content-Type',path.endsWith('.js')?'text/javascript':path.endsWith('.css')?'text/css':path.endsWith('.woff2')?'font/woff2':'text/html');res.end(await readFile(new URL('../dist/'+path,import.meta.url)));
}catch(error){console.error(error);res.writeHead(500);res.end('{}');}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});const page=await browser.newPage({viewport:{width:1200,height:1000}});const errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('dialog',async d=>{errors.push('Native dialog');await d.dismiss();});
try{
 await page.goto(origin+'/#ai');await page.locator('#loginForm [name=username]').fill('owner');await page.locator('#loginForm [name=password]').fill('test-password');await page.locator('#loginForm button').click();await page.waitForFunction(()=>document.querySelector('#historyStats')?.textContent.includes('12'));
 assert.ok((await page.locator('#historyWarnings').textContent()).includes('ยาว'));
 await page.locator('#readHistory').click();await page.locator('.chat-message').first().waitFor();assert.equal(await page.locator('.chat-message').count(),4);assert.equal(await page.locator('.chat-message img').count(),0);assert.ok((await page.locator('.chat-message').last().textContent()).includes('<img'));
 await page.locator('[data-older]').click();await page.waitForFunction(()=>document.querySelector('.chat-message')?.textContent.includes('ข้อความทดสอบ 5'));
 await page.locator('[data-newer]').click();await page.waitForFunction(()=>document.querySelector('.chat-message')?.textContent.includes('ข้อความทดสอบ 9'));
 await page.locator('[data-reset]').click();await page.locator('.action-modal').waitFor();await page.keyboard.press('Escape');assert.equal(resetCalls,0);assert.equal(records.length,12);
 await page.screenshot({path:'../docs/build/web-preview/chat-history.png'});
 await page.setViewportSize({width:320,height:568});assert.equal(await page.locator('.history-modal').evaluate(e=>e.scrollWidth<=e.clientWidth+1),true);
 await page.locator('[data-reset]').click();await page.locator('.action-modal .primary').click();await page.waitForFunction(()=>document.querySelector('[data-messages]')?.textContent.includes('ยังไม่มีข้อความ'));
 assert.equal(resetCalls,1);assert.equal(records.length,0);await page.locator('[data-close]').click();await page.reload();await page.waitForFunction(()=>document.querySelector('#historyStats')?.textContent.includes('0 ข้อความ'));assert.deepEqual(errors,[]);
 console.log('PASS: long-history warning, safe text reader, pagination, modal cancel/confirm, reset completion, reload and mobile fit');
}finally{await browser.close();server.close();}
