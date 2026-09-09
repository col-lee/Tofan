// Local, accessible dialogs and controls; no browser alert/confirm/prompt.
export function modal(message,{title='ยืนยันการทำรายการ',value,confirm='ยืนยัน',cancel='ยกเลิก'}={}) {
 return new Promise(resolve=>{
  const previous=document.activeElement,d=document.createElement('dialog');d.className='action-modal';
  const heading=document.createElement('h2');heading.id='modal-title';heading.textContent=title;d.setAttribute('aria-labelledby',heading.id);
  const description=document.createElement('p');description.textContent=message;description.style.whiteSpace='pre-line';d.append(heading,description);
  let input;if(value!==undefined){input=document.createElement('input');input.value=value;input.maxLength=120;input.setAttribute('aria-label',message);d.append(input);}
  const actions=document.createElement('div');actions.className='modal-actions';const no=document.createElement('button'),yes=document.createElement('button');no.textContent=cancel;yes.textContent=confirm;yes.className='primary';actions.append(no,yes);d.append(actions);
  let result=null;no.onclick=()=>d.close();yes.onclick=()=>{if(input&&!input.value.trim()){input.focus();return;}result=input?input.value:true;d.close();};
  d.onkeydown=e=>{if(e.key==='Enter'&&e.target===input){e.preventDefault();yes.click();}};
  d.onclose=()=>{d.remove();if(previous?.isConnected)previous.focus();resolve(result);};document.body.append(d);d.showModal();(input||no).focus();input?.select();
 });
}
export const ask=message=>modal(message);
export const requestName=(message,value)=>modal(message,{title:'เปลี่ยนชื่อไฟล์',value,confirm:'บันทึกชื่อ'});

export function enhanceControls(root=document){
 root.querySelectorAll('select:not([data-enhanced])').forEach(select=>{
  select.dataset.enhanced='true';select.classList.add('native-control');select.tabIndex=-1;select.setAttribute('aria-hidden','true');
  const button=document.createElement('button');button.type='button';button.className='select-control';button.setAttribute('aria-haspopup','dialog');
  const label=select.getAttribute('aria-label')||select.closest('label')?.firstChild?.textContent.trim()||'เลือกตัวเลือก';
  const sync=()=>{button.textContent=(select.selectedOptions[0]?.textContent||'เลือก')+'  ⌄';button.disabled=select.disabled;button.setAttribute('aria-label',label+': '+(select.selectedOptions[0]?.textContent||''));};
  const descriptor=Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype,'value');Object.defineProperty(select,'value',{get(){return descriptor.get.call(this);},set(value){descriptor.set.call(this,value);sync();},configurable:true});
  select.after(button);select.addEventListener('change',sync);sync();
  button.onclick=e=>{e.preventDefault();const d=document.createElement('dialog');d.className='select-modal';d.setAttribute('aria-label',label);const h=document.createElement('h2');h.textContent=label;d.append(h);const list=document.createElement('div');list.setAttribute('role','listbox');list.setAttribute('aria-label',label);
   [...select.options].forEach(option=>{const item=document.createElement('button');item.type='button';item.className='select-option';item.textContent=option.textContent;item.disabled=option.disabled;item.setAttribute('role','option');item.setAttribute('aria-selected',String(option.selected));item.onclick=()=>{select.value=option.value;select.dispatchEvent(new Event('input',{bubbles:true}));select.dispatchEvent(new Event('change',{bubbles:true}));sync();d.close();};list.append(item);});d.append(list);
   const close=document.createElement('button');close.textContent='ปิด';close.className='quiet';close.onclick=()=>d.close();d.append(close);
   d.onkeydown=e=>{if(!['ArrowDown','ArrowUp','Home','End'].includes(e.key))return;e.preventDefault();const items=[...list.querySelectorAll('button:not(:disabled)')];let i=items.indexOf(document.activeElement);i=e.key==='Home'?0:e.key==='End'?items.length-1:(i+(e.key==='ArrowDown'?1:-1)+items.length)%items.length;items[i]?.focus();};
   d.onclose=()=>{d.remove();if(button.isConnected)button.focus();};document.body.append(d);d.showModal();(list.querySelector('[aria-selected=true]:not(:disabled)')||list.querySelector('button:not(:disabled)')||close).focus();
  };
 });
 root.querySelectorAll('input[type=file]:not([data-enhanced])').forEach(input=>{
  input.dataset.enhanced='true';input.classList.add('native-control');input.tabIndex=-1;
  const box=document.createElement('div');box.className='file-picker';const button=document.createElement('button');button.type='button';button.className='file-pick-button';button.textContent='＋ เลือกไฟล์จากเครื่อง';const name=document.createElement('span');name.className='picked-name';name.textContent='ยังไม่ได้เลือกไฟล์';name.setAttribute('aria-live','polite');box.append(button,name);input.after(box);
  button.onclick=e=>{e.preventDefault();input.click();};input.addEventListener('change',()=>{name.textContent=input.files[0]?.name||'ยังไม่ได้เลือกไฟล์';button.textContent=input.files.length?'↻ เปลี่ยนไฟล์':'＋ เลือกไฟล์จากเครื่อง';});
 });
}

let validationOpen=false;
document.addEventListener('invalid',async e=>{e.preventDefault();if(validationOpen)return;validationOpen=true;await modal(e.target.validationMessage,{title:'ตรวจสอบข้อมูลอีกนิด',confirm:'ตกลง',cancel:'ปิด'});validationOpen=false;if(e.target.isConnected){const focus=e.target.classList.contains('native-control')?e.target.nextElementSibling?.querySelector('button')||e.target.nextElementSibling:e.target;focus?.focus();}},true);
