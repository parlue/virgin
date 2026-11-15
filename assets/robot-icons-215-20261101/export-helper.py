from pathlib import Path
import json,sys,hashlib,html
sys.path.insert(0,str(Path(__file__).parent/'tmp/core2-pillow'))
from PIL import Image,ImageOps
r=Path(r'C:\Users\DirkSommerfeld\Documents\projects\virgin\assets\robot-icons-215-20261101')
manifest=r/'manifest.json'
doc=json.loads(manifest.read_text(encoding='utf8'))
jobs=json.loads(Path(sys.argv[1]).read_text(encoding='utf8'))
if isinstance(jobs,dict):jobs=[jobs]
for job in jobs:
 row=next(x for x in doc['items'] if x['elo']==job['elo'] and x['style']==job['style'])
 src=Path(job['source']);dest=r/row['file']
 im=Image.open(src).convert('RGBA');assert im.getchannel('A').getextrema()==(0,255),str(src)
 # Export format only: preserve generated artwork/alpha and fit a uniform square.
 scaled=ImageOps.contain(im,(1024,1024),Image.Resampling.LANCZOS)
 canvas=Image.new('RGBA',(1024,1024));canvas.alpha_composite(scaled,((1024-scaled.width)//2,(1024-scaled.height)//2))
 canvas.save(dest,optimize=True)
 row.update(status='complete',width=1024,height=1024,source=str(src),sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
 row['prompt']=job['prompt']
ready=sum(x['status']=='complete' for x in doc['items'])
doc['complete_count']=ready
manifest.write_text(json.dumps(doc,indent=2,ensure_ascii=False),encoding='utf8')
blocks=[]
for x in doc['items']:
 label=f"{x['elo']} · {x['name']} · {x['style'].title()}"
 inside=f'<a href="{x["file"]}"><img loading="lazy" src="{x["file"]}"></a>' if x['status']=='complete' else '<div class="pending">Wird erstellt</div>'
 blocks.append(f'<article data-style="{x["style"]}">{inside}<p>{html.escape(label)}</p></article>')
page='''<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Roboter-Icons</title><style>body{background:#08111f;color:#e4efff;font:16px system-ui;margin:30px}h1{margin-bottom:8px}header{position:sticky;top:0;background:#08111f;padding:10px 0;z-index:1}main{display:grid;grid-template-columns:repeat(5,minmax(100px,1fr));gap:14px;max-width:1300px}article{background:#111f34;border:1px solid #254563;border-radius:16px;overflow:hidden}img{width:100%;display:block}p{font-size:12px;text-align:center}select{background:#192d48;color:white;border:1px solid #4c7caa;padding:10px;border-radius:9px}.pending{aspect-ratio:1;display:grid;place-items:center;color:#7992b2}@media(max-width:650px){main{grid-template-columns:repeat(2,1fr)}}</style><header><h1>Roboter-Icons</h1><p style="text-align:left">COMPLETE von 215 fertig · 700–2800 Elo · 1024×1024 PNG · transparente Außenecken</p><select onchange="document.querySelectorAll('article').forEach(e=>e.hidden=this.value!=='all'&&e.dataset.style!==this.value)"><option value="all">Alle Varianten</option><option>fortress</option><option>stone</option><option>normal</option><option>fire</option><option>thunder</option></select></header><main>BLOCKS</main><footer><p>Zum Aktualisieren die Datei erneut öffnen oder neu laden. Jedes Bild lässt sich einzeln öffnen.</p></footer></html>'''.replace('COMPLETE',str(ready)).replace('BLOCKS',''.join(blocks))
(r/'index.html').write_text(page,encoding='utf8')
print(json.dumps({'complete':ready,'target':215,'folder':str(r)}))
