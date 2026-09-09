import {bytes,time} from './model.mjs';
const safe=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const percent=n=>Number.isFinite(n)?Math.max(0,Math.min(100,n)):0;
const metric=(value,sub)=>`<div class="metric">${safe(value)}</div><small>${safe(sub)}</small>`;
function bar(value,label){const n=Math.round(percent(value));return `<div class="status-caption"><span>${safe(label)}</span><b>${n}%</b></div><div class="status-track" role="progressbar" aria-label="${safe(label)}" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${n}"><span style="width:${n}%"></span></div>`;}
const history=[];
export function recordStatus(s){if(Number.isFinite(s.heap)){history.push(s.heap);if(history.length>45)history.shift();}}
function graph(){if(history.length<2)return '<div class="graph-empty">กำลังเก็บข้อมูล RAM…</div>';const min=Math.min(...history),max=Math.max(...history),range=Math.max(max-min,1024);const points=history.map((n,i)=>`${(i/(history.length-1)*300).toFixed(1)},${(45-(n-min)/range*38).toFixed(1)}`).join(' ');return `<svg class="sparkline" viewBox="0 0 300 52" preserveAspectRatio="none" role="img" aria-label="กราฟ RAM ว่าง ${history.length} จุดล่าสุด ช่วง ${safe(bytes(min))} ถึง ${safe(bytes(max))}"><path d="M0 48 H300 M0 25 H300" class="graph-grid"/><polyline points="${points}"/></svg><small>RAM ว่าง · ${bytes(min)}–${bytes(max)} · ${history.length} จุดล่าสุด</small>`;}
export function widgetContent(id,s){switch(id){
 case'network':return metric(s.connected?s.ssid:'Access Point',s.connected?`${s.ip} · ${s.rssi} dBm`:`AP · ${s.apIP||'192.168.4.1'}`)+bar(s.connected?(s.rssi+100)*2:0,'ความแรงสัญญาณ WiFi');
 case'storage':return metric(s.sd?bytes((s.storageTotal||0)-(s.storageUsed||0)):'ไม่มี SD',s.sd?`ว่างจากทั้งหมด ${bytes(s.storageTotal)}`:'ใส่ SD card เพื่อจัดเก็บไฟล์')+bar(s.storageTotal?s.storageUsed/s.storageTotal*100:0,'พื้นที่ใช้งาน');
 case'music':return metric(s.title||'พักสักครู่',`${time(s.current)} / ${time(s.duration)} · ${s.playing?'กำลังเล่น':'หยุดอยู่'}`)+(s.duration?bar(s.current/s.duration*100,'ความคืบหน้าเพลง'):'<div class="state-pill">● '+(s.playing?'สตรีมสด · ไม่ระบุความยาว':'ยังไม่มีเพลง')+'</div>');
 case'volume':return metric(`${s.settings?.volume??'—'}%`,'ระดับเสียงของอุปกรณ์')+bar(s.settings?.volume,'ระดับเสียง');
 case'voice':return metric(s.settings?.voice?'เปิดอยู่':'ปิดอยู่','Voice assistant · inference')+`<div class="state-pill ${s.settings?.voice?'on':''}"><span class="state-dot"></span>${s.settings?.voice?'เปิดการรับคำสั่งเสียง':'พักการรับคำสั่งเสียง'}</div>`;
 case'system':return metric(`${Math.floor((s.uptime||0)/60)} นาที`,`RAM ว่าง ${bytes(s.heap)}`)+graph();
 default:return '';
}}
