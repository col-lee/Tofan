import {chromium} from '@playwright/test';
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {personalities} from '../pet-personalities.mjs';
import {geminiVoices} from '../gemini-voices.mjs';
const firmware=await readFile('../src/core/PetPersonality.hpp','utf8');
assert.equal(personalities.length,9);
for(const p of personalities){assert.ok(geminiVoices.some(v=>v[0]===p.voice));assert.ok(Buffer.byteLength(p.prompt)<512);assert.ok(firmware.includes(p.background.replace('#','0x')));}
const state={device:'test',settings:{petPersonality:0,volume:50,volumeStep:5,colors:[]},ai:{provider:'gemini-live',voice:'Kore',systemInstruction:'My custom prompt',enabled:false}},saved=[];
const server=createServer(async(req,res)=>{try{
 if(req.url.startsWith('/api/')){
  let raw='';for await(const b of req)raw+=b;const body=new URLSearchParams(raw);let result={ok:true};
  if(req.url==='/api/session')result=req.method==='POST'?{token:'test'}:{setup:false};
  if(req.url==='/api/status')result=state;
  if(req.url==='/api/settings'){const update=JSON.parse(body.get('values'));if(update.customPet)state.customPet=update.customPet;else Object.assign(state.settings,update);saved.push(['settings',structuredClone(state.settings)]);}
  if(req.url==='/api/ai'){Object.assign(state.ai,JSON.parse(body.get('values')));saved.push(['ai',structuredClone(state.ai)]);}
  res.setHeader('Content-Type','application/json');res.end(JSON.stringify(result));return;
 }
 const path=req.url.split('?')[0]==='/'?'index.html':req.url.split('?')[0].slice(1);res.setHeader('Content-Type',path.endsWith('.js')?'text/javascript':path.endsWith('.css')?'text/css':path.endsWith('.woff2')?'font/woff2':'text/html');res.end(await readFile(new URL('../dist/'+path,import.meta.url)));
}catch{res.writeHead(404);res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});const page=await browser.newPage({viewport:{width:1280,height:1050}}),errors=[];
page.on('pageerror',e=>errors.push(e.message));
try{
 await page.goto('http://127.0.0.1:'+server.address().port+'/#ai');await page.locator('#loginForm [name=username]').fill('owner');await page.locator('#loginForm [name=password]').fill('test-password');await page.locator('#loginForm button').click();await page.locator('#petAppearance').waitFor();
 assert.equal(await page.locator('.pet-choice svg').count(),9);
 for(const p of personalities){
  await page.locator('#aiForm [name=systemInstruction]').fill('My custom prompt');
  await page.locator(`[name=petPersonality][value="${p.id}"]`).check();await page.waitForFunction(()=>!document.querySelector('#applyPetPreset').disabled);
  assert.equal(await page.locator('#aiForm [name=systemInstruction]').inputValue(),'My custom prompt');
  await page.locator('#applyPetPreset').click();assert.equal(await page.locator('#aiForm [name=voice]').inputValue(),p.voice);assert.equal(await page.locator('#aiForm [name=systemInstruction]').inputValue(),p.prompt);
  await page.locator('#aiForm .primary').click();await page.waitForFunction(prompt=>document.querySelector('#notice').textContent.includes('AI'),p.prompt);
  await page.waitForTimeout(50);assert.equal(state.ai.systemInstruction,p.prompt);assert.equal(state.settings.petPersonality,p.id);
 }
 await page.reload();await page.locator('#petAppearance').waitFor();assert.equal(await page.locator('[name=petPersonality]:checked').inputValue(),'8');assert.equal(await page.locator('#aiForm [name=systemInstruction]').inputValue(),personalities[8].prompt);
 await page.locator('#customPetForm [name=base]').selectOption('7',{force:true});
 await page.locator('#customPetForm [name=background]').evaluate(e=>{e.value='#123456';e.dispatchEvent(new Event('input',{bubbles:true}));});
 await page.locator('#customPetForm [name=eyeWidth]').evaluate(e=>{e.value='125';e.dispatchEvent(new Event('input',{bubbles:true}));});
 await page.locator('#customPetForm [name=voice]').selectOption('Puck',{force:true});
 await page.locator('#customPetForm [name=prompt]').fill('My custom voice and face');
 const savedCustom=page.waitForResponse(r=>r.url().endsWith('/api/settings')&&r.request().method()==='POST');await page.locator('#saveCustomPet').click();await savedCustom;
 assert.equal(state.customPet.background,0x123456);assert.equal(state.customPet.eyeWidth,125);assert.equal(state.customPet.base,7);
 await page.locator('[name=petPersonality][value="0"]').check();await page.waitForFunction(()=>!document.querySelector('#applyPetPreset').disabled);
 await page.reload();await page.locator('[name=petPersonality][value="8"]').check();await page.waitForFunction(()=>!document.querySelector('#applyPetPreset').disabled);
 assert.equal(await page.locator('#customPetForm [name=prompt]').inputValue(),'My custom voice and face');assert.equal(await page.locator('#customPetForm [name=background]').inputValue(),'#123456');
 const beforeInvalid=saved.length;await page.locator('#customPetForm [name=prompt]').fill('ก'.repeat(180));await page.locator('#saveCustomPet').click();assert.equal(saved.length,beforeInvalid);assert.ok((await page.locator('#notice').textContent()).includes('511'));
 await page.locator('#customPetForm [name=prompt]').fill('My custom voice and face');await page.locator('#applyPetPreset').click();assert.equal(await page.locator('#aiForm [name=voice]').inputValue(),'Puck');assert.equal(await page.locator('#aiForm [name=systemInstruction]').inputValue(),'My custom voice and face');
 await page.screenshot({path:'../docs/build/web-preview/personalities.png',fullPage:true});
 await page.setViewportSize({width:320,height:568});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);await page.screenshot({path:'../docs/build/web-preview/personalities-mobile.png',fullPage:true});
 assert.ok(saved.filter(([kind])=>kind==='ai').length===9);assert.deepEqual(errors,[]);
 console.log('PASS: nine previews, appearance saves, custom prompt preservation, nine voice/prompt presets, reload and mobile fit');
}finally{await browser.close();server.close();}
