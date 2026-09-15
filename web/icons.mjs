// Font Awesome Free, served from the device. Labels remain text, never HTML.
const pages={espnow:'tower-broadcast',foods:'utensils',dashboard:'gauge-high',files:'images',wifi:'wifi',settings:'sliders',ai:'robot',ota:'microchip',account:'user-gear'};
const widgets={network:'wifi',storage:'hard-drive',music:'music',volume:'volume-high',voice:'microphone',system:'memory'};
const buttons={foodDraw:'shuffle',mobileMenu:'bars',logout:'arrow-right-from-bracket',accountLogout:'arrow-right-from-bracket',addBlock:'plus',resetLayout:'rotate-left',refreshFiles:'rotate-right',screenSize:'display',resetCrop:'crop-simple',process:'wand-magic-sparkles',processVideo:'film',cancelProcess:'xmark',cancelVideoProcess:'xmark',cancelUpload:'xmark',uploadPrepared:'file-arrow-up',uploadOriginal:'upload',downloadPrepared:'download',saveColors:'palette'};
const actions={up:'arrow-up',down:'arrow-down',remove:'xmark',preview:'eye',download:'download',rename:'pen-to-square',delete:'trash'};
function decorate(element,name,iconOnly=false,end=false){
 if(!name||element.querySelector(':scope > .fa-solid'))return;
 const label=element.textContent.replace(/^[\s＋+↻↗↑↓×●○✓⌟]+/u,'').replace(/[\s⌄↗]+$/u,'');
 const icon=document.createElement('i');icon.className=`fa-solid fa-${name}`;icon.setAttribute('aria-hidden','true');
 element.replaceChildren();if(end){element.append(document.createTextNode(label),icon);}else{element.append(icon);if(!iconOnly&&label)element.append(document.createTextNode(' '+label));}
}
export function startIcons(){
 let scheduled=false;
 const refresh=()=>{
  scheduled=false;
  document.querySelectorAll('aside nav a,.mobile-menu-links a').forEach(a=>decorate(a,pages[a.hash.slice(1)]));
  document.querySelectorAll('.widget').forEach(card=>decorate(card.querySelector('h3'),widgets[card.dataset.id]));
  document.querySelectorAll('button').forEach(button=>{
   let name=buttons[button.id]||actions[button.dataset.action]||actions[button.dataset.op],only=false,end=false;
   if(button.classList.contains('resize')){name='up-right-and-down-left-from-center';only=true;}
   else if(button.classList.contains('select-control')){name='chevron-down';end=true;}
   else if(button.classList.contains('file-pick-button'))name='folder-open';
   else if(button.closest('#notice,.mobile-menu-head')){name='xmark';only=true;}
   else if(button.closest('.modal-actions'))name=button.classList.contains('primary')?'check':'xmark';
   else if(button.closest('form')&&button.classList.contains('primary'))name=button.closest('#loginForm')?'right-to-bracket':button.closest('#otaFile')?'microchip':'floppy-disk';
   else if(button.closest('#otaUrl'))name='cloud-arrow-down';
   if(button.id==='mobileMenu'||button.dataset.action)only=true;
   decorate(button,name,only,end);
  });
  document.querySelectorAll('.action-modal h2').forEach(h=>decorate(h,'circle-question'));
  document.querySelectorAll('.select-modal h2').forEach(h=>decorate(h,'list-ul'));
  const connection=document.querySelector('#connection');if(connection)decorate(connection,'wifi');
  document.querySelectorAll('.state-pill').forEach(p=>{if(p.textContent.trim().startsWith('●'))decorate(p,'radio');});
 };
 const observer=new MutationObserver(()=>{if(!scheduled){scheduled=true;queueMicrotask(refresh);}});
 observer.observe(document.body,{childList:true,subtree:true});refresh();
}
