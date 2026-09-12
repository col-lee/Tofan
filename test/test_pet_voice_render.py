"""Check production-rendered mouth/wave geometry after frame settling."""
from pathlib import Path
import json
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'test/render_ui_preview.py')],check=True)
exe=root/'.pio/ui-preview.exe'
subprocess.run(['g++','-std=c++17','-I'+str(root/'test/support'),str(root/'.pio/ui-preview.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],cwd=root,check=True)
frames={name:[json.loads(line) for line in (root/'docs/build/ui-preview'/('12-pet-'+name+'.jsonl')).read_text().splitlines()]
        for name in ['speaking','pause','listening']}
def mouth(frame):
    return [op['a'][3] for op in frame if op['op'] in ['ellipse','round']
            and 120<op['a'][0]<190 and 155<op['a'][1]<210]
assert max(mouth(frames['speaking']))>=24
assert max(mouth(frames['pause']))<=5
assert any(op['op']=='ellipse-outline' for op in frames['listening'])
print('PASS: production renderer opens mouth for speech, closes during pause, and draws listening waves')
