from pathlib import Path
import subprocess
from concurrent.futures import ThreadPoolExecutor
import imageio_ffmpeg
from PIL import Image, ImageDraw

ROOT = Path(__file__).parent
VIDEO = r'C:\Users\MinJun\Videos\Captures\Dungeon Survivor 2026-09-07 16-37-02.mp4'
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()

def extract(t):
    target = ROOT / f'preview-{t:03}.jpg'
    subprocess.run([FFMPEG, '-v', 'error', '-ss', str(t), '-i', VIDEO, '-frames:v', '1', '-vf', 'scale=480:270', '-y', str(target)], check=True)
    return target

if __name__ == '__main__':
    times = list(range(2, 339, 8))
    with ThreadPoolExecutor(max_workers=4) as pool:
        frames = list(pool.map(extract, times))
    for batch in range(3):
        group = list(zip(times, frames))[batch*16:(batch+1)*16]
        canvas = Image.new('RGB', (1920, 300*((len(group)+3)//4)), '#202020')
        draw = ImageDraw.Draw(canvas)
        for i, (t, frame) in enumerate(group):
            x,y=(i%4)*480,(i//4)*300
            canvas.paste(Image.open(frame), (x,y))
            draw.text((x+10,y+274), f'{t//60:02}:{t%60:02} ({t}s)', fill='white')
        canvas.save(ROOT / f'contact-{batch+1}.jpg')
    print('Contact sheets ready')
