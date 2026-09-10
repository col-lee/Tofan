import {snapWidth,snapHeight} from './model.mjs';

// Keep the shared snap grid, but never shrink below the actual rendered content.
export function contentHeight(card){
 const css=getComputedStyle(card);
 const needed=card.querySelector('.widget-head').getBoundingClientRect().height+
  card.querySelector('[data-value]').getBoundingClientRect().height+
  parseFloat(css.paddingTop)+parseFloat(css.paddingBottom)+12;
 return Math.max(220,220+Math.ceil((needed-220)/80)*80);
}
export function sizeCard(card,block,width=block.w,height=block.h){
 const grid=card.parentElement;
 let minWidth=3;
 if(window.innerWidth>1000)minWidth=Math.min(12,Math.ceil(280/(grid.clientWidth+20)*12/3)*3);
 block.w=Math.max(minWidth,snapWidth(width));
 card.style.setProperty('--span',block.w);
 block.h=Math.max(contentHeight(card),snapHeight(height));
 card.style.setProperty('--rows',(block.h+20)/80);
 card.style.setProperty('--height',block.h+'px');
 card.querySelector('.block-size').textContent=`${block.w}/12 | ${block.h} px`;
}
export function observeCards(grid,blocks,onChange){
 let scheduled=false;
 const fit=()=>{scheduled=false;if(!grid.isConnected)return;let changed=false;grid.querySelectorAll('.widget').forEach(card=>{const block=blocks.find(b=>b.id===card.dataset.id);if(!block)return;const w=block.w,h=block.h;sizeCard(card,block);changed=changed||w!==block.w||h!==block.h;});if(changed)onChange();};
 const observer=new ResizeObserver(()=>{if(!scheduled){scheduled=true;requestAnimationFrame(fit);}});
 observer.observe(grid);grid.querySelectorAll('[data-value]').forEach(el=>observer.observe(el));fit();
 return ()=>observer.disconnect();
}
