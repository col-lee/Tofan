export function mobileMenu(){
 const button=document.querySelector('#mobileMenu');
 button.onclick=()=>{
  const dialog=document.createElement('dialog');dialog.className='mobile-menu';dialog.setAttribute('aria-label','เมนูหลัก');
  const header=document.createElement('div');header.className='mobile-menu-head';const title=document.createElement('h2');title.textContent='พื้นที่ของคุณ';const close=document.createElement('button');close.textContent='×';close.setAttribute('aria-label','ปิดเมนู');close.onclick=()=>dialog.close();header.append(title,close);dialog.append(header);
  const links=document.createElement('div');links.className='mobile-menu-links';
  document.querySelectorAll('aside nav a').forEach(link=>{const item=link.cloneNode(true);item.onclick=()=>dialog.close();links.append(item);});dialog.append(links);
  const status=document.createElement('p');status.className='hint';status.textContent=document.querySelector('#connection').textContent;dialog.append(status);
  dialog.onclose=()=>{button.setAttribute('aria-expanded','false');dialog.remove();button.focus();};
  document.body.append(dialog);button.setAttribute('aria-expanded','true');dialog.showModal();close.focus();
 };
}
