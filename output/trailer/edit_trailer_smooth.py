from pathlib import Path
import subprocess, json, shutil, math, wave
from concurrent.futures import ThreadPoolExecutor
import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parent
WORK=ROOT/'work-smooth'; WORK.mkdir(exist_ok=True)
VIDEO=Path(r'C:\Users\MinJun\Videos\Captures\Dungeon Survivor 2026-09-07 16-37-02.mp4')
FF=imageio_ffmpeg.get_ffmpeg_exe()
# source start, source duration, speed, final duration, heading, eyebrow, zoom
SHOTS=[
 (0.4,4,1,4,'','',1),
 (13.0,2.5,1,2.5,'FIREBALL','01 / COLLECT YOUR ARSENAL',1.25),
 (61.5,2.2,1,2.2,'LIGHTNING','02 / COLLECT YOUR ARSENAL',1.25),
 (87.3,.75,1,.75,'','',1.15),
 (88.98,1.5,1,1.5,'ORBIT BLADE','03 / COLLECT YOUR ARSENAL',1.25),
 (134.6,2,1,2,'BOOMERANG','04 / COLLECT YOUR ARSENAL',1.25),
 (158.95,.75,1,.75,'RICOCHET','05 / COLLECT YOUR ARSENAL',1.25),
 (212.45,2,1,2,'MAGIC BULLET','06 / COLLECT YOUR ARSENAL',1.25),
 (249.6,3,1,3,'SIX WEAPONS. ONE ARSENAL.','COMBINE YOUR FIREPOWER',1),
 (260,2.5,1,2.5,'','',1),
 (129.3,1.5,1,1.5,'CHOOSE YOUR UPGRADE','BUILD YOUR OWN RUN',1.08),
 (130.8,1.7,1,1.7,'','',1),
 (175.5,5.5,1,5.5,'FIGHT THE HORDE','KEEP MOVING. KEEP FIRING.',1),
 (194.8,5.5,1,5.5,'','',1),
 (273.2,5.8,1,5.8,'','',1),
 (203.9,.9,1, .9,'STOP TIME','TURN THE FIGHT AROUND',1),
 (204.8,1.9,1,1.9,'EMPOWER YOUR DASH','TIME SLASH',1),
 (206.7,1.5,1,1.5,'','',1),
 (64.0,3.5,1,3.5,'FACE THE BOSSES','FOUR ROUNDS. RISING THREATS.',1),
 (73.1,5,1,5,'GIANT SLIME','BOSS / 01',1),
 (137.5,2.7,1,2.7,'','',1),
 (143.0,5,1,5,'CORRUPTED KNIGHT','BOSS / 02',1),
 (215.5,2.7,1,2.7,'','',1),
 (224.7,5,1,5,'CORRUPTED MAGE','BOSS / 03',1),
 (282.0,4,1,4,'ABYSS CTHULHU','THE FINAL ENCOUNTER',1),
 (291.0,2.8,1,2.8,'','',1),
 (293.8,2.5,1,2.5,'','',1),
 (308.0,3.8,1,3.8,'','',1),
 (319.5,2.6,1,2.6,'','',1),
 (323.5,2.5,1,2.5,'','',1),
 (1.0,3.2,1,3.2,'HOW LONG WILL YOU SURVIVE?','DUNGEON SURVIVOR',1),
]
TRANS=.1

NEW_VIDEO=Path(r'C:\Users\MinJun\Videos\Captures\Dungeon Survivor 2026-09-07 18-11-22.mp4')
# Give each pickup breathing room at original speed and dissolve between clips.
# Eight distinct weapons, omitting repeated magic bullet and magic blade pickups.
POPUPS=[4.4666667,7.6,10.9666667,14.1333333,18.1,25.5333333,28.8,32.3]
ORIGINAL_SHOTS=SHOTS
SHOTS=[SHOTS[0]]+[(t-.6,1.6,1,1.6,'','',1.5) for t in POPUPS]+SHOTS[8:-1]
SHOTS=[(*s[:4],'','',s[6]) for s in SHOTS]

def overlap(i):
    return .3 if 1 <= i <= 9 else TRANS

def shot_source(i):
    return NEW_VIDEO if 1 <= i <= 8 else VIDEO

# The clean cut already has caption-free original-speed clips.
for i,s in enumerate(SHOTS):
    if 1 <= i <= 8: continue
    cached=ROOT/'work-clean'/f'shot-{i:02}.mp4'
    target=WORK/f'shot-{i:02}.mp4'
    if cached.exists() and not target.exists():shutil.copyfile(cached,target)
for i in range(2,6):
    target=WORK/f'group-{i}.mp4'
    if not target.exists():shutil.copyfile(ROOT/'work-clean'/f'group-{i}.mp4',target)

def run(args, log):
    with open(WORK/(log+'.log'),'w',encoding='utf8') as f:
        p=subprocess.run([FF,'-hide_banner','-y',*args],stdout=f,stderr=f)
    if p.returncode: raise RuntimeError((WORK/(log+'.log')).read_text()[-4000:])

def label(index,title,kicker):
    im=Image.new('RGBA',(1920,1080))
    d=ImageDraw.Draw(im)
    for x in range(1000):
        d.line((x,817,x,945),fill=(8,10,20,int(195*max(0,1-x/1000)**.4)))
    d.rectangle((48,837,53,920),fill='#f5c96a')
    d.text((75,836),kicker,font=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',19),fill='#f5c96a')
    font=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',40)
    d.text((74,867),title,font=font,fill='white',stroke_width=1,stroke_fill='#16131d')
    p=WORK/f'label-{index:02}.png'; im.save(p); return p

def render(item):
    i,s=item; start,length,speed,duration,title,kicker,zoom=s
    dest=WORK/f'shot-{i:02}.mp4'
    if dest.exists(): return dest,duration
    args=['-threads','2','-ss',str(start),'-t',str(length),'-i',str(shot_source(i))]
    if title: args+=['-loop','1','-i',str(label(i,title,kicker))]
    assert speed == 1 and length == duration, 'Only original-speed footage is allowed'
    vf='[0:v]setpts=PTS-STARTPTS,fps=30,setsar=1'
    if zoom!=1:
        w=int(1920/zoom)//2*2;h=int(1080/zoom)//2*2
        vf+=f',crop={w}:{h},scale=1920:1080:flags=lanczos'
    vf+=f',trim=duration={duration},settb=1/30'
    if i==0: vf+=',fade=t=in:d=0.8'
    vf+='[base]'
    if title:
        vf+=f';[1:v]format=rgba,fade=t=in:st=0.1:d=0.25:alpha=1,fade=t=out:st={max(.4,duration-.4)}:d=0.25:alpha=1[title];[base][title]overlay=0:0:shortest=1[v]'
    else: vf+=';[base]null[v]'
    # Preserve source timing for both video and audio. No slow motion or freeze holds.
    vf+=f';[0:a]asetpts=PTS-STARTPTS,aresample=48000,atrim=duration={duration},afade=t=in:d=0.04,afade=t=out:st={duration-.14}:d=0.14[a]'
    run(args+['-filter_complex_threads','2','-filter_complex',vf,'-map','[v]','-map','[a]','-t',str(duration),'-c:v','libx264','-preset','veryfast','-crf','18','-threads','2','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k',str(dest)],f'shot-{i:02}')
    print(f'Shot {i+1}/{len(SHOTS)} ready',flush=True)
    return dest,duration

def join(parts,name,first_shot=0,group_starts=None):
    dest=WORK/(name+'.mp4')
    transitions=[overlap((group_starts[i] if group_starts else first_shot+i)) for i in range(1,len(parts))]
    duration=sum(t for p,t in parts)-sum(transitions)
    if dest.exists() and dest.stat().st_size>10000:return dest,duration
    args=[]
    for p,t in parts:args+=['-threads','1','-i',str(p)]
    filters=[]
    for i,(p,t) in enumerate(parts):filters.append(f'[{i}:v]setpts=PTS-STARTPTS,fps=30,setsar=1,settb=AVTB[v{i}];[{i}:a]asetpts=PTS-STARTPTS[a{i}]')
    v='v0';a='a0';elapsed=parts[0][1]
    for i in range(1,len(parts)):
        cross=transitions[i-1]
        transition='fade'
        if cross:
            filters.append(f'[{v}][v{i}]xfade=transition={transition}:duration={cross}:offset={elapsed-cross},fps=30,settb=AVTB[x{i}]')
            filters.append(f'[{a}][a{i}]acrossfade=d={cross}:c1=tri:c2=tri[y{i}]')
        else:
            filters.append(f'[{v}][{a}][v{i}][a{i}]concat=n=2:v=1:a=1[cx{i}][y{i}]')
            filters.append(f'[cx{i}]fps=30,settb=AVTB[x{i}]')
        v=f'x{i}';a=f'y{i}';elapsed+=parts[i][1]-cross
    run(args+['-filter_complex_threads','2','-filter_complex',';'.join(filters),'-map',f'[{v}]','-map',f'[{a}]','-t',str(duration),'-r','30','-c:v','libx264','-preset','veryfast','-crf','18','-threads','3','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k',str(dest)],name)
    print(name+' ready',flush=True)
    return dest,duration

def accents(duration,starts):
    sr=48000; data=np.zeros(int((duration+1)*sr),dtype=np.float32);rng=np.random.default_rng(42)
    for pos in starts:
        length=.35;n=int(sr*length);t=np.arange(n)/sr
        env=np.sin(np.pi*t/length)**2
        noise=rng.normal(0,1,n);noise=np.convolve(noise,np.ones(7)/7,'same')
        sweep=np.sin(2*np.pi*(170*t-130*t*t))
        hit=(noise*.15+sweep*.12)*env
        idx=max(0,int((pos-.12)*sr));data[idx:idx+n]+=hit
    out=WORK/'transition-accents.wav'
    with wave.open(str(out),'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(sr);w.writeframes((np.clip(data,-1,1)*32767).astype('<i2').tobytes())
    return out

if __name__=='__main__':
    timeline=[];cursor=0
    for i,s in enumerate(SHOTS):
        timeline.append(dict(shot=i,start=round(cursor,6),duration=s[3],source_start=s[0],source_duration=s[1],source=str(shot_source(i)),speed=s[2],heading=s[4]))
        cursor+=s[3]-(overlap(i+1) if i<len(SHOTS)-1 else 0)
    assert cursor<=90,cursor
    (ROOT/'timeline-smooth.json').write_text(json.dumps(timeline,indent=2),encoding='utf8')
    print(f'Target duration: {cursor:.2f}s',flush=True)
    with ThreadPoolExecutor(max_workers=2) as pool:parts=list(pool.map(render,enumerate(SHOTS)))
    groups=[join(parts[i:i+5],f'group-{i//5}',first_shot=i) for i in range(0,len(parts),5)]
    video,duration=join(groups,'assembled',group_starts=list(range(0,len(parts),5)))
    music=Path(r'C:\ShootingGame2D_DX11\HAL_Study_WIN_260527\asset\sound\bgm\stage2_dungeon2.wav')
    final=ROOT/'DungeonSurvivor_Trailer_Smooth.mp4'
    filt=f'[0:v]fade=t=out:st={duration-1.1}:d=1.1[v];[0:a]volume=0.55[game];[1:a]volume=0.23,afade=t=in:d=1.2,afade=t=out:st={duration-2}:d=2[music];[game][music]amix=inputs=2:normalize=0,alimiter=limit=0.92,afade=t=out:st={duration-1.1}:d=1.1[a]'
    run(['-i',str(video),'-stream_loop','-1','-i',str(music),'-filter_complex_threads','2','-filter_complex',filt,'-map','[v]','-map','[a]','-t',str(duration),'-c:v','libx264','-preset','veryfast','-crf','18','-threads','4','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k','-movflags','+faststart',str(final)],'final')
    shutil.copyfile('ATTRIBUTION_BGM.txt',ROOT/'Music_Credits.txt')
    print(f'FINISHED: {final} / {duration:.2f}s',flush=True)
