from pathlib import Path
import subprocess, re, json, sys
from concurrent.futures import ThreadPoolExecutor
import imageio_ffmpeg
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parent
VIDEO=ROOT/(sys.argv[1] if len(sys.argv)>1 else 'DungeonSurvivor_Trailer.mp4')
FF=imageio_ffmpeg.get_ffmpeg_exe()
QA=ROOT/'work'/('final-qa-'+VIDEO.stem.split('_')[-1].lower());QA.mkdir(exist_ok=True)
meta=subprocess.run([FF,'-hide_banner','-i',str(VIDEO)],capture_output=True,text=True).stderr
match=re.search(r'Duration: (\d+):(\d+):([\d.]+)',meta)
assert match,meta
duration=int(match[1])*3600+int(match[2])*60+float(match[3])
assert 0<duration<=90,duration
assert '1920x1080' in meta and '30 fps' in meta,meta
assert 'Audio: aac' in meta,meta
p=subprocess.run([FF,'-hide_banner','-i',str(VIDEO),'-vf','blackdetect=d=0.35:pix_th=0.05','-af','volumedetect','-f','null','-'],capture_output=True,text=True)
assert p.returncode==0,p.stderr[-2000:]
decoded_frames=int(re.findall(r'frame=\s*(\d+)',p.stderr)[-1])
assert abs(decoded_frames-duration*30)<=2,(decoded_frames,duration)
(QA/'decode-check.log').write_text(p.stderr,encoding='utf8')
print('\n'.join(s for s in p.stderr.splitlines() if any(k in s for k in ['black_start','mean_volume','max_volume'])))
ts=[2,5.2,8,10.3,13,16,19,22,26,31,36,39,42,46,51,56,61,65,69,74,78,81.5]
if '_Dash' in VIDEO.stem and '_DashShort' not in VIDEO.stem:
    timeline=json.loads((ROOT/'timeline-dash.json').read_text())
    assert all(r['speed']==1 and not r['heading'] for r in timeline)
    assert all(r['source_start']+r['source_duration']<=326 for r in timeline)
    assert timeline[0]['source_start']==8.5 and timeline[0]['duration']==1.1
    assert abs(sum(timeline[i]['duration'] for i in [13,14,15])-8.3)<.001
    checks=[(0,.6)]+[(i,.8) for i in range(1,9)]+[(11,.5)]+[(i,.8) for i in range(16,22)]+[(23,2),(25,2),(27,2),(28,1.5),(30,1.2),(33,1.2)]
    ts=[timeline[i]['start']+offset for i,offset in checks]
if '_Smooth' in VIDEO.stem:
    timeline=json.loads((ROOT/'timeline-smooth.json').read_text())
    assert all(r['speed']==1 and not r['heading'] for r in timeline)
    assert timeline[-1]['source_start']==323.5
    checks=[(0,2)]+[(i,.8) for i in range(1,9)]+[(9,1.3),(11,.5),(13,2),(14,2),(15,2),(17,1),(20,2),(22,2),(24,2),(25,1.5),(27,1.2),(29,1),(30,1.0)]
    ts=[timeline[i]['start']+offset for i,offset in checks]
if '_Clean' in VIDEO.stem:
    timeline=json.loads((ROOT/'timeline-clean.json').read_text())
    assert all(r['speed']==1 and not r['heading'] for r in timeline)
    assert all(abs(timeline[i+1]['start']-timeline[i]['start']-.8)<.0001 for i in range(1,8))
    checks=[(0,2)]+[(i,.25) for i in range(1,9)]+[(9,1.3),(11,.5),(13,2),(14,2),(15,2),(17,1),(20,2),(22,2),(24,2),(25,1.5),(27,1.2),(29,1),(30,1.8),(31,1.7)]
    ts=[timeline[i]['start']+offset for i,offset in checks]
if '_1x' in VIDEO.stem:
    timeline=json.loads((ROOT/'timeline-1x.json').read_text())
    checks=[(0,2),(1,1.2),(2,.8),(4,.2),(5,.65),(6,.57),(7,.95),
            (8,1.3),(10,.5),(12,2),(13,2),(14,2),(16,1),(19,2),
            (21,2),(23,2),(24,1.5),(26,1.2),(27,2),(28,1),(29,1.8),(30,1.7)]
    ts=[timeline[i]['start']+offset for i,offset in checks]
if '_Tight' in VIDEO.stem or '_DashShort' in VIDEO.stem:
    timeline=json.loads((ROOT/('timeline-tight.json' if '_Tight' in VIDEO.stem else 'timeline-dash-short.json')).read_text())
    assert all(r['speed']==1 and not r['heading'] for r in timeline)
    assert abs(sum(r['duration'] for r in timeline[16:20])-5)<.001
    if '_Tight' in VIDEO.stem:assert abs(sum(r['duration'] for r in timeline[26:])-9.1)<.001
    assert timeline[-1]['source_start']+timeline[-1]['source_duration']<=326
    ts=[timeline[i]['start']+min(.7,timeline[i]['duration']/2) for i in [0,1,2,3,4,5,6,7,8,11,13,14,15,16,17,18,19,21,23,25,26,27,29,31]]

def frame(t):
    target=QA/f'frame-{t:05.1f}.jpg'
    subprocess.run([FF,'-v','error','-ss',str(t),'-i',str(VIDEO),'-frames:v','1','-vf','scale=480:270,format=yuvj420p','-y',str(target)],check=True)
    return target
with ThreadPoolExecutor(4) as pool:frames=list(pool.map(frame,ts))
for batch in range(2):
    group=list(zip(ts,frames))[batch*12:(batch+1)*12]
    canvas=Image.new('RGB',(1920,900),'#15151b');draw=ImageDraw.Draw(canvas)
    for i,(t,p) in enumerate(group):
        x,y=(i%4)*480,(i//4)*300;canvas.paste(Image.open(p),(x,y));draw.text((x+8,y+277),f'{t:.1f}s',fill='white')
    canvas.save(QA/f'sheet-{batch}.jpg')
print(json.dumps(dict(duration=duration,resolution='1920x1080',fps=30,size_mb=round(VIDEO.stat().st_size/1e6,1),decode='passed')))
