import {chromium} from '@playwright/test';
import {createServer} from 'node:http';import {readFile} from 'node:fs/promises';import assert from 'node:assert/strict';
import {validMac} from '../espnow.mjs';
assert.ok(validMac('02:11:22:33:44:55'));assert.ok(!validMac('FF:FF:FF:FF:FF:FF'));assert.ok(!validMac('00:00:00:00:00:00'));
let now={mac:'02:00:00:00:00:01',enabled:false,ready:false,channel:6,revision:0,peers:[],error:''},sends=0,saves=0;
const server=createServer(async(req,res)=>{try{const u=new URL(req.url,'http://test');let data;
 if(u.pathname.startsWith('/api/')){let raw='';for await(const c of req)raw+=c;const form=new URLSearchParams(raw);
  if(u.pathname==='/api/session')data=req.method==='POST'?{token:'test'}:{setup:false};
  else {if(req.headers.authorization!=='Bearer test'){res.writeHead(401,{'Content-Type':'application/json'});res.end('{"error":"Sign in"}');return;}
   if(u.pathname==='/api/status')data={device:'test',settings:{colors:[]},ai:{provider:'gemini-live'}};
   else if(u.pathname==='/api/espnow'){if(req.method==='POST'){assert.equal(req.headers['x-tofan-client'],'portal');const config=JSON.parse(form.get('values'));now={...now,...config,ready:config.enabled,revision:now.revision+1,peers:config.peers.map(p=>({...p,registered:p.enabled,online:false,tx:0,rx:0,failed:0}))};++saves;}data=now;}
   else if(u.pathname==='/api/espnow/send'){assert.equal(form.get('hex'),'00FF2A');++sends;++now.revision;Object.assign(now.peers[0],{tx:1,rx:1,online:true,payloadHex:'00FF2A'});data={ok:true};}
   else data={ok:true};
  }res.setHeader('Content-Type','application/json');res.end(JSON.stringify(data));return;
 }
 if(u.pathname==='/favicon.ico'){res.writeHead(204);res.end();return;}const path=u.pathname==='/'?'index.html':u.pathname.slice(1);res.setHeader('Content-Type',path.endsWith('.js')?'text/javascript':path.endsWith('.css')?'text/css':path.endsWith('.woff2')?'font/woff2':'text/html');res.end(await readFile(new URL('../dist/'+path,import.meta.url)));
 }catch(e){console.error(e);res.writeHead(500);res.end('{}');}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});const page=await browser.newPage({viewport:{width:1200,height:1000}});const errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('dialog',async d=>{errors.push('Native dialog');await d.dismiss();});
try{
 await page.goto(origin+'/#espnow');assert.ok(!(await page.locator('body').textContent()).includes(now.mac));assert.equal((await page.request.get(origin+'/api/espnow')).status(),401);
 await page.locator('#loginForm [name=username]').fill('owner');await page.locator('#loginForm [name=password]').fill('password');await page.locator('#loginForm button').click();await page.locator('#nowMac').filter({hasText:now.mac}).waitFor();
 await page.locator('#nowAdd').click();await page.locator('.now-peer [name=name]').fill('Desk <b>lamp</b>');await page.locator('.now-peer [name=mac]').fill('02:11:22:33:44:55');await page.locator('#nowEnabled').check();await page.locator('#nowSave').click();await page.waitForFunction(()=>document.querySelector('#notice')?.textContent.includes('บันทึก ESP-NOW แล้ว'));assert.equal(saves,1);assert.equal(await page.locator('#nowTelemetry b').count(),0);
 await page.locator('#nowSend [name=hex]').fill('00FF2A');await page.locator('#nowSend button.primary').click();await page.waitForFunction(()=>document.querySelector('#nowTelemetry')?.textContent.includes('00FF2A'));assert.equal(sends,1);
 await page.locator('.now-peer [name=name]').fill('Desk lamp updated');await page.waitForTimeout(2200);assert.equal(await page.locator('.now-peer [name=name]').inputValue(),'Desk lamp updated');await page.locator('#nowSave').click();await page.waitForFunction(()=>document.querySelector('#nowTelemetry')?.textContent.includes('Desk lamp updated'));
 await page.screenshot({path:'../docs/build/web-preview/espnow.png'});await page.setViewportSize({width:360,height:800});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
 await page.locator('[data-remove]').click();await page.locator('.action-modal').waitFor();await page.keyboard.press('Escape');assert.equal(await page.locator('.now-peer').count(),1);
 await page.locator('[data-remove]').click();await page.locator('.action-modal button.primary').click();assert.equal(await page.locator('.now-peer').count(),0);await page.locator('#nowSave').click();await page.waitForFunction(()=>document.querySelector('#nowTelemetry')?.children.length===0);assert.equal(now.peers.length,0);
 await page.reload();await page.locator('#nowMac').filter({hasText:now.mac}).waitFor();assert.equal(await page.locator('.now-peer').count(),0);assert.deepEqual(errors,[]);
 console.log('PASS: login-gated MAC, add/edit/delete, modal cancellation, binary send/status, escaping, polling preserves edits, reload and mobile fit');
}finally{await browser.close();await new Promise(r=>server.close(r));}
