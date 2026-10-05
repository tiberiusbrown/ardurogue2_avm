"""Create an exact terrain gallery from the native generator's ASCII output.

SVG/HTML need only Python's standard library. If Pillow is installed, also
write contact-sheet PNGs for visual review. Nothing enters the AVM build.
"""

import argparse
import html
from pathlib import Path
import re
import subprocess

COLORS = {'#': '#17212b', '.': '#d6d1bf', '+': '#c58b36', '<': '#47c5dc',
          '>': '#c78dee', '@': '#ffe55a', 'M': '#e45a53', '!': '#6fb478'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--count', type=int, default=40)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.count <= 1000:
        parser.error('count must be 1..1000')
    args.output.mkdir(parents=True, exist_ok=True)
    text = subprocess.check_output([str(args.native.resolve()), '--floor-batch', str(args.count)], text=True)
    (args.output / 'maps.txt').write_text(text)
    maps = []
    for block in text.strip().split('\n\n'):
        lines = block.splitlines()
        caption, rows = lines[0], lines[1:]
        if len(rows) != 32 or any(len(row) != 64 for row in rows):
            raise RuntimeError('unexpected native map output')
        maps.append((caption, rows))
    cards = []
    for index, (caption, rows) in enumerate(maps):
        rects = []
        for y, row in enumerate(rows):
            for x, cell in enumerate(row):
                if cell != '#':
                    rects.append(f'<rect x="{x}" y="{y}" width="1" height="1" fill="{COLORS[cell]}"/>')
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 32" shape-rendering="crispEdges">'
               f'<rect width="64" height="32" fill="{COLORS["#"]}"/>' + ''.join(rects) + '</svg>')
        name = f'map-{index:03}.svg'
        (args.output / name).write_text(svg)
        cards.append(f'<figure><img src="{name}" alt="{html.escape(caption)}"><figcaption>{html.escape(caption)}</figcaption></figure>')
    page = '''<!doctype html><meta charset="utf-8"><title>ArduRogue 2 floor gallery</title>
<style>body{background:#101820;color:#e7e4d9;font:14px system-ui;margin:24px}
main{display:grid;grid-template-columns:repeat(auto-fit,minmax(450px,1fr));gap:20px}
figure{margin:0;background:#202c38;padding:12px}img{width:100%;image-rendering:pixelated}
figcaption{padding-top:8px;font:12px monospace;line-height:1.6}h1{font-size:24px}</style>
<h1>ArduRogue 2: generated floors</h1><p>Gold: player · cyan: up · violet: down · amber: door · red: monster · green: item</p><main>'''
    (args.output / 'index.html').write_text(page + ''.join(cards) + '</main>')
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print(f'{len(maps)} SVG maps and gallery written to {args.output}; PNGs skipped (Pillow unavailable)')
        return
    font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 13) if Path('C:/Windows/Fonts/consola.ttf').exists() else ImageFont.load_default()
    for start in range(0, len(maps), 12):
        group = maps[start:start+12]
        sheet = Image.new('RGB', (1440, ((len(group)+2)//3)*290 + 55), '#101820')
        draw = ImageDraw.Draw(sheet)
        draw.text((16, 12), 'Gold player | cyan up | violet down | amber door | red monster | green item', font=font, fill='#e7e4d9')
        for i, (caption, rows) in enumerate(group):
            left, top = (i % 3)*480 + 16, (i // 3)*290 + 50
            words = re.split(r' coverage=', caption, maxsplit=1)
            draw.text((left, top), words[0], font=font, fill='#e7e4d9')
            draw.text((left, top+17), 'coverage=' + words[1].split(' attempts=')[0], font=font, fill='#b2bbc5')
            for y, row in enumerate(rows):
                for x, cell in enumerate(row):
                    draw.rectangle((left+x*7, top+40+y*7, left+x*7+6, top+40+y*7+6), fill=COLORS[cell])
        sheet.save(args.output / f'contact-{start//12+1}.png')
    print(f'{len(maps)} maps, HTML gallery, and {(len(maps)+11)//12} contact sheets written to {args.output}')


if __name__ == '__main__':
    main()
