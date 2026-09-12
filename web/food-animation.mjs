// One final result comes from the device; intermediate names are visual only.
export const spinDelays=Array.from({length:16},(_,step)=>40+step*14);
export async function animateFood(element,names,active=()=>true) {
 if(names.length<2||matchMedia('(prefers-reduced-motion: reduce)').matches)return;
 element.setAttribute('aria-live','off');element.classList.add('spinning');
 try {
  for(let step=0;step<spinDelays.length;step++){
   if(!element.isConnected||!active())return;
   element.textContent=names[step%names.length];
   await new Promise(resolve=>setTimeout(resolve,spinDelays[step]));
  }
 }finally{element.classList.remove('spinning');element.setAttribute('aria-live','polite');}
}
