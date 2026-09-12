import {chromium} from '@playwright/test';
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {geminiVoices,voiceSelect} from '../gemini-voices.mjs';
assert.equal(geminiVoices.length,30);assert.equal(new Set(geminiVoices.map(v=>v[0])).size,30);
const server=createServer(async(req,res)=>{try{const path=req.url==='/'?'test/foods.html':req.url.slice(1);res.setHeader('Content-Type',path.endsWith('.mjs')?'text/javascript':path.endsWith('.css')?'text/css':'text/html');res.end(await readFile(new URL('../'+path,import.meta.url)));}catch{res.writeHead(404);res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
const page=await browser.newPage({viewport:{width:1100,height:900}});const errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('dialog',async d=>{errors.push('native dialog');await d.dismiss();});
try{
 await page.goto('http://127.0.0.1:'+server.address().port);
 await page.locator('#foodForm').waitFor();
 await page.locator('#categoryForm [name=name]').fill('Dessert');await page.locator('#categoryForm .primary').click();await page.waitForFunction(()=>window.catalog.categories.length===2);
 await page.locator('#foodForm [name=name]').fill('<img src=x>');await page.locator('#foodForm [name=category]').selectOption('2',{force:true});await page.locator('#foodForm .primary').click();await page.waitForFunction(()=>window.catalog.items.length===1);assert.equal(await page.locator('.food-list img').count(),0);
 await page.locator('[data-edit]').click();await page.locator('#foodForm [name=name]').fill('Ice cream');await page.locator('#foodForm .primary').click();await page.waitForFunction(()=>window.catalog.items[0].name==='Ice cream');
 await page.locator('#foodForm [name=name]').fill('Cake');await page.locator('#foodForm [name=category]').selectOption('2',{force:true});await page.locator('#foodForm .primary').click();await page.waitForFunction(()=>window.catalog.items.length===2);
 await page.locator('#foodFilter').selectOption('2',{force:true});await page.locator('#foodDraw').click();assert.equal(await page.locator('#foodDraw').isDisabled(),true);await page.waitForFunction(()=>document.querySelector('#foodResult').textContent==='Cake');await page.waitForFunction(()=>!document.querySelector('#foodDraw').disabled);await page.waitForFunction(()=>document.querySelector('#foodResult').textContent==='Ice cream');
 await page.screenshot({path:'../docs/build/web-preview/foods.png',fullPage:true});
 await page.locator('[data-delete]').first().click();await page.locator('dialog').waitFor();await page.keyboard.press('Escape');assert.equal(await page.locator('[data-edit]').count(),2);
 await page.locator('[data-delete]').first().click();await page.locator('dialog .primary').click();await page.waitForFunction(()=>window.catalog.items.length===1);
 await page.setViewportSize({width:320,height:568});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
 assert.equal(await page.locator('#voice select option').count(),30);assert.equal(await page.locator('#voice select').inputValue(),'Sulafat');await page.locator('#voice .select-control').click();assert.equal(await page.locator('dialog [role=option]').count(),30);await page.locator('dialog [role=option]').filter({hasText:'Kore'}).click();assert.equal(await page.locator('#voice select').inputValue(),'Kore');
 assert.deepEqual(errors,[]);console.log('Food browser: CRUD, category filtering, draw, escaping, modal cancellation, mobile fit; all 30 voice options passed.');
}finally{await browser.close();server.close();}
