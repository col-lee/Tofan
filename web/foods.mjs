import {animateFood} from './food-animation.mjs';
export async function mountFoods({root,api,ask,notice,enhanceControls,escape:e}) {
 const host=document.createElement('div');root.replaceChildren(host);
 let state,filter=0,busy=false;
 const run=fn=>async event=>{event?.preventDefault();if(busy)return;busy=true;try{await fn(event);}catch(error){notice(error.message,true);}finally{busy=false;}};
 const change=async payload=>{state=await api('foods',{values:JSON.stringify(payload)});if(host.isConnected)render();};
 function render(){
  if(!state.categories.some(c=>c.id===filter))filter=0;
  const options=selected=>state.categories.map(c=>`<option value="${c.id}" ${c.id===selected?'selected':''}>${e(c.name)}</option>`).join('');
  const rows=state.items.filter(f=>!filter||f.category===filter);
  host.innerHTML=`<div class="two"><section class="panel"><span class="badge">RANDOM FOODS</span><h2>วันนี้กินอะไรดี</h2><label>หมวดหมู่ที่อยากสุ่ม<select id="foodFilter"><option value="0">ทุกหมวดหมู่</option>${options(filter)}</select></label><div class="food-result" id="foodResult" aria-live="polite">${e(state.result||'ให้เราช่วยเลือกมื้อนี้')}</div><p class="hint">${rows.length} ตัวเลือก · สุ่มไม่ซ้ำเมนูก่อนหน้าเมื่อมีหลายเมนู</p><button id="foodDraw" class="primary" ${rows.length?'':'disabled'}>สุ่มเมนู</button></section><form id="foodForm" class="panel"><h2 id="foodFormTitle">เพิ่มเมนูอาหาร / เครื่องดื่ม</h2><input type="hidden" name="id" value="0"><label>ชื่อเมนู<input name="name" required maxlength="32" placeholder="เช่น ข้าวกะเพรา"></label><label>หมวดหมู่<select name="category" required>${options(filter)}</select></label><div class="row"><button class="primary" ${state.categories.length?'':'disabled'}>บันทึกเมนู</button><button type="button" id="foodCancel">เริ่มรายการใหม่</button></div><p class="hint">บันทึกในเครื่อง ใช้ต่อได้หลังเปิดใหม่ · สูงสุด 60 เมนู / 12 หมวดหมู่</p></form></div><section class="panel"><h2>รายการของกิน (${rows.length})</h2><div class="food-list">${rows.map(f=>`<div class="food-row"><div><strong>${e(f.name)}</strong><small>${e(state.categories.find(c=>c.id===f.category)?.name)}</small></div><div class="row"><button data-edit="${f.id}" data-op="rename" aria-label="แก้ไข ${e(f.name)}">แก้ไข</button><button data-delete="${f.id}" data-op="delete" aria-label="ลบ ${e(f.name)}">ลบ</button></div></div>`).join('')||'<p class="hint">ยังไม่มีเมนู เพิ่มเมนูแรกได้ด้านบน</p>'}</div></section><section class="panel"><h2>จัดการหมวดหมู่</h2><form id="categoryForm" class="row"><input type="hidden" name="id" value="0"><label>ชื่อหมวดหมู่<input name="name" required maxlength="20" placeholder="เช่น อาหารเช้า"></label><button class="primary">บันทึกหมวดหมู่</button><button type="button" id="categoryCancel">เริ่มรายการใหม่</button></form><div class="food-list">${state.categories.map(c=>`<div class="food-row"><strong>${e(c.name)}</strong><div class="row"><button data-cat-edit="${c.id}" data-op="rename">เปลี่ยนชื่อ</button><button data-cat-delete="${c.id}" data-op="delete">ลบ</button></div></div>`).join('')}</div></section>`;
  const $=s=>host.querySelector(s);
  $('#foodFilter').onchange=event=>{filter=+event.target.value;state.result='';render();};
  $('#foodDraw').onclick=run(async()=>{
   const result=$('#foodResult'),button=$('#foodDraw');
   // Lock this view while a draw is pending; prevent edits/category changes midway.
   const controls=[...host.querySelectorAll('button,input,select')];
   const disabled=controls.map(c=>c.disabled);controls.forEach(c=>c.disabled=true);
   button.textContent='กำลังสุ่ม…';result.setAttribute('aria-busy','true');
   try {
    const [response]=await Promise.allSettled([
     api('foods',{values:JSON.stringify({action:'draw',category:filter})}),
     animateFood(result,rows.map(f=>f.name),()=>host.isConnected)
    ]);
    if(response.status==='rejected')throw response.reason;
    state=response.value;if(host.isConnected)render();
   }finally {
    if(result.isConnected){result.textContent=state.result||'ลองสุ่มอีกครั้ง';result.removeAttribute('aria-busy');button.textContent='สุ่มเมนู';controls.forEach((c,i)=>c.disabled=disabled[i]);}
   }
  });
  $('#foodForm').onsubmit=run(event=>{const d=Object.fromEntries(new FormData(event.target));return change({action:'item',id:+d.id,name:d.name,category:+d.category});});
  $('#categoryForm').onsubmit=run(event=>{const d=Object.fromEntries(new FormData(event.target));return change({action:'category',id:+d.id,name:d.name});});
  $('#foodCancel').onclick=()=>render();$('#categoryCancel').onclick=()=>render();
  host.querySelectorAll('[data-edit]').forEach(b=>b.onclick=()=>{const f=state.items.find(f=>f.id===+b.dataset.edit),form=$('#foodForm');form.elements.id.value=f.id;form.elements.name.value=f.name;form.elements.category.value=f.category;$('#foodFormTitle').textContent='แก้ไขเมนู';form.elements.name.focus();});
  host.querySelectorAll('[data-cat-edit]').forEach(b=>b.onclick=()=>{const c=state.categories.find(c=>c.id===+b.dataset.catEdit),form=$('#categoryForm');form.elements.id.value=c.id;form.elements.name.value=c.name;form.elements.name.focus();});
  host.querySelectorAll('[data-delete]').forEach(b=>b.onclick=run(async()=>{if(await ask('ลบเมนูนี้ออกจากรายการ?'))await change({action:'deleteItem',id:+b.dataset.delete});}));
  host.querySelectorAll('[data-cat-delete]').forEach(b=>b.onclick=run(async()=>{if(await ask('ลบหมวดหมู่นี้? ต้องย้ายหรือลบเมนูในหมวดหมู่ก่อน'))await change({action:'deleteCategory',id:+b.dataset.catDelete});}));
  enhanceControls(host);
 }
 host.textContent='กำลังโหลดเมนู…';
 await run(async()=>{state=await api('foods');filter=state.category||0;if(!Array.isArray(state.categories)||!Array.isArray(state.items))throw Error('โหลดรายการไม่สำเร็จ กรุณาเปิดหน้านี้ใหม่');if(host.isConnected)render();})();
}
