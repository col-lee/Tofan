export function historyWarnings(s){
 const warnings=[];
 if(s.error)warnings.push(s.error);
 if(s.storage==='ram')warnings.push('ไม่มี SD card: เก็บเฉพาะข้อความล่าสุดใน RAM และจะหายเมื่อปิดเครื่อง ใส่ SD แล้วเปิดเครื่องใหม่เพื่อบันทึกถาวร');
 if(s.full)warnings.push('ประวัติแชทเต็มแล้ว หยุดบันทึกข้อความใหม่ลง SD กรุณาอ่านและรีเซ็ตประวัติ');
 else if(s.long)warnings.push('ประวัติแชทยาวแล้ว เมื่อเชื่อมต่อใหม่ AI จะใช้เฉพาะข้อความล่าสุด ไม่เกิน 8 ข้อความ / 8 KiB ส่วนข้อความเก่ายังเปิดอ่านจาก SD ได้');
 if(s.truncated)warnings.push('มีข้อความยาวเกินขนาดที่บันทึกได้ จึงเก็บไว้บางส่วน');
 if(s.dropped)warnings.push(`มีข้อความ ${s.dropped} รายการที่บันทึกไม่ทัน`);
 return warnings;
}
export function mountHistory({root,api,ask,notice}){
 let current=null,dialog=null,cursor=0,loading=false,resetting=false,stack=[],pageRequest=0;
 root.innerHTML=`<section class="panel history-panel"><span class="badge">CONVERSATION MEMORY</span><h2>ประวัติและความจำ</h2><p class="hint">บันทึกข้อความถอดเสียง Gemini Live ลง SD และนำบทสนทนาล่าสุดกลับไปใช้เมื่อเชื่อมต่อใหม่</p><p id="historyStats" role="status">กำลังอ่านสถานะ…</p><progress id="historyCapacity" max="1048576" value="0" aria-label="พื้นที่ประวัติแชท"></progress><div id="historyWarnings" class="history-warning" role="status" hidden></div><div class="row"><button id="readHistory"><i class="fa-solid fa-comments" aria-hidden="true"></i> อ่านประวัติ / จัดการ</button><button id="resetHistory"><i class="fa-solid fa-trash" aria-hidden="true"></i> รีเซ็ตประวัติ</button></div></section>`;
 const $=s=>root.querySelector(s);
 const status=s=>{
  current=s;$('#historyStats').textContent=s.resetting?'กำลังหยุดบทสนทนาและล้างประวัติ…':`${s.count||0} ข้อความ · ${s.storage==='sd'?'SD card':'RAM ชั่วคราว'} · ${Math.ceil((s.bytes||0)/1024)} / ${Math.ceil((s.maxBytes||1048576)/1024)} KiB${s.pending?` · รอบันทึก ${s.pending} ข้อความ`:''}`;
  $('#historyCapacity').value=s.bytes||0;$('#historyCapacity').max=s.maxBytes||1048576;
  const warnings=historyWarnings(s),box=$('#historyWarnings');box.replaceChildren();box.hidden=!warnings.length;
  warnings.forEach(w=>{const p=document.createElement('p');p.textContent=w;box.append(p);});
  $('#resetHistory').disabled=resetting||!!s.resetting;
  dialog?.querySelector('[data-reset]')?.toggleAttribute('disabled',resetting||!!s.resetting);
 };
 const refresh=async()=>{if(loading||!root.isConnected)return;loading=true;try{status(await api('history?summary=1'));}catch(error){$('#historyStats').textContent=error.message;}finally{loading=false;}};
 const reset=async()=>{
  if(resetting||current?.resetting)return;
  if(!await ask('รีเซ็ตประวัติแชทและความจำทั้งหมดใช่ไหม?\nระบบจะหยุดบทสนทนาปัจจุบันและลบข้อความที่บันทึกไว้ การลบนี้ย้อนกลับไม่ได้'))return;
  resetting=true;if(current)status(current);
  try{await api('history/reset',{confirm:'reset'});let result;
   for(let i=0;i<20;i++){await new Promise(r=>setTimeout(r,250));result=await api('history?summary=1');status(result);if(!result.resetting)break;}
   if(result?.resetting)notice('กำลังล้างประวัติ รอสถานะจากเครื่อง');
   else if(result?.error)notice(result.error,true);
   else {notice('ล้างประวัติและความจำแล้ว เริ่มคุยใหม่ได้');stack=[];cursor=0;if(dialog?.open)await showPage(0);}
  }catch(error){notice(error.message,true);}finally{resetting=false;if(current)status(current);}
 };
 const showPage=async before=>{
  const target=dialog,request=++pageRequest;if(!target)return;target.querySelector('[data-messages]').textContent='กำลังโหลด…';
  try{const s=await api(`history?before=${before}`);if(dialog!==target||!target.open||request!==pageRequest)return;status(s);cursor=before;
   const list=target.querySelector('[data-messages]');list.replaceChildren();
   if(s.error){const p=document.createElement('p');p.textContent=s.error;list.append(p);}
   for(const m of s.messages||[]){const item=document.createElement('article');item.className='chat-message '+(m.role==='model'?'from-ai':'from-user');
    const label=document.createElement('strong');label.textContent=m.role==='model'?'AI Pet':'คุณ';
    const time=document.createElement('small');time.textContent=m.time?new Date(m.time*1000).toLocaleString('th-TH'):`หลังเปิดเครื่อง ${m.uptime||0} วินาที (ยังไม่ซิงก์เวลา)`;
    const text=document.createElement('p');text.textContent=m.text;
    item.append(label,time,text);if(m.partial||m.truncated){const note=document.createElement('small');note.textContent=m.truncated?'ข้อความยาว เก็บไว้บางส่วน':'บทสนทนาถูกขัดจังหวะหรือการเชื่อมต่อสิ้นสุด';item.append(note);}list.append(item);
   }
   if(!s.messages?.length&&!s.error)list.textContent='ยังไม่มีข้อความที่บันทึกไว้ เริ่มคุยกับ Gemini Live ได้เลย';
   target.querySelector('[data-older]').disabled=!s.hasOlder;target.querySelector('[data-older]').onclick=()=>{stack.push(cursor);showPage(s.before);};
   target.querySelector('[data-newer]').disabled=!stack.length;
  }catch(error){if(target.open)target.querySelector('[data-messages]').textContent=error.message;}
 };
 $('#readHistory').onclick=()=>{
  if(dialog?.open)return;dialog=document.createElement('dialog');dialog.className='history-modal';dialog.setAttribute('aria-label','ประวัติสนทนา');
  dialog.innerHTML='<div class="toolbar"><h2>ประวัติสนทนา</h2><button data-close aria-label="ปิดประวัติ">ปิด</button></div><div data-messages class="chat-messages" aria-live="polite"></div><div class="row"><button data-older>เก่ากว่านี้</button><button data-newer>ใหม่กว่านี้</button><button data-latest>ล่าสุด / รีเฟรช</button><button data-reset>รีเซ็ตประวัติ</button></div>';
  const target=dialog;target.querySelector('[data-close]').onclick=()=>target.close();target.querySelector('[data-newer]').onclick=()=>showPage(stack.pop()||0);
  target.querySelector('[data-latest]').onclick=()=>{stack=[];showPage(0);};target.querySelector('[data-reset]').onclick=reset;
  target.onclose=()=>{target.remove();if(dialog===target)dialog=null;$('#readHistory').focus();};document.body.append(target);target.showModal();stack=[];showPage(0);
 };
 $('#resetHistory').onclick=reset;
 refresh();const timer=setInterval(()=>{if(!root.isConnected){clearInterval(timer);dialog?.close();return;}refresh();},2500);
}
