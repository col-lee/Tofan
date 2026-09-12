// Google Gemini native audio / TTS voice catalog, checked 2026-09-12.
// https://ai.google.dev/gemini-api/docs/speech-generation#voice-options
export const geminiVoices=[
 ['Zephyr','สดใส'],['Puck','ร่าเริง'],['Charon','อธิบายชัดเจน'],['Kore','หนักแน่น'],
 ['Fenrir','ตื่นเต้น'],['Leda','อ่อนเยาว์'],['Orus','หนักแน่น'],['Aoede','โปร่งเบาสบาย'],
 ['Callirrhoe','ผ่อนคลาย'],['Autonoe','สดใส'],['Enceladus','นุ่มกระซิบ'],['Iapetus','ชัดใส'],
 ['Umbriel','สบาย ๆ'],['Algieba','นุ่มลื่น'],['Despina','นุ่มลื่น'],['Erinome','ชัดใส'],
 ['Algenib','แหบมีเอกลักษณ์'],['Rasalgethi','เชิงบรรยาย'],['Laomedeia','ร่าเริง'],
 ['Achernar','นุ่มนวล'],['Alnilam','หนักแน่น'],['Schedar','เรียบสม่ำเสมอ'],['Gacrux','สุขุม'],
 ['Pulcherrima','มั่นใจตรงไปตรงมา'],['Achird','เป็นมิตร'],['Zubenelgenubi','เป็นกันเอง'],
 ['Vindemiatrix','อ่อนโยน'],['Sadachbia','มีชีวิตชีวา'],['Sadaltager','สุขุมแบบผู้รู้'],['Sulafat','อบอุ่น']
];
export function voiceSelect(current,escape){
 const selected=current||'Kore',known=geminiVoices.some(([name])=>name===selected);
 return `<select name="voice">${known?'':`<option value="${escape(selected)}" selected>${escape(selected)} · ค่าที่บันทึกไว้</option>`}${geminiVoices.map(([name,tone])=>`<option value="${name}" ${name===selected?'selected':''}>${name} · ${tone}</option>`).join('')}</select>`;
}
