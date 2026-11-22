#!/usr/bin/env python3
"""Generate native 720p Liquid assets and Latin-1 glyph atlas."""
from pathlib import Path
import os,sys
import numpy as np
from PIL import Image,ImageDraw,ImageFilter,ImageFont

W,H=1280,720
out=Path(sys.argv[1] if len(sys.argv)>1 else 'build/ui')
out.mkdir(parents=True,exist_ok=True)
fonts=Path(os.environ.get('TURNWAVE_FONTS','/usr/share/fonts/truetype/open-sans'))
def font(name,size):
    p=fonts/name
    return ImageFont.truetype(str(p if p.is_file() else '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),size)
y,x=np.mgrid[:H,:W].astype(np.float32)
base=np.broadcast_to(np.array([11,15,28],dtype=np.float32),(H,W,3)).copy()
for cx,cy,sx,sy,color in [
    (1100,320,440,300,(43,25,83)),(730,90,440,250,(20,48,73)),
    (180,620,420,280,(24,26,55)),(1010,680,390,260,(33,24,65))]:
    base+=np.exp(-((x-cx)/sx)**2-((y-cy)/sy)**2)[...,None]*color
# Abstract illuminated audio waveform, muted behind the panels.
t=np.linspace(0,1,W,dtype=np.float32)
wave=np.sin(t*28)*np.cos(t*9)*68
band=np.exp(-((y-(382+wave[None,:]))/3.5)**2)
base+=(band*.37)[...,None]*np.array([151,109,206])
img=Image.fromarray(np.clip(base,0,255).astype('uint8'),'RGB').convert('RGBA')

def glass(rect,radius,tint,edge):
    global img
    blur=img.filter(ImageFilter.GaussianBlur(15))
    tint_layer=Image.new('RGBA',(W,H),(*tint,255))
    blur=Image.blend(blur,tint_layer,.34)
    m=Image.new('L',(W,H))
    ImageDraw.Draw(m).rounded_rectangle(rect,radius,fill=238)
    img.paste(blur,(0,0),m)
    ImageDraw.Draw(img).rounded_rectangle(rect,radius,outline=edge,width=2)

def text(x,y,title,size,color='#ffffff',bold=False):
    ImageDraw.Draw(img).text((x,y),title,font=font('OpenSans-Bold.ttf' if bold else 'OpenSans-Regular.ttf',size),fill=color)

glass((64,42,1216,109),25,(24,29,49),'#686d96')
glass((64,130,739,620),30,(21,24,44),'#656a93')
glass((759,130,1216,620),30,(27,27,48),'#7771a0')
glass((64,636,1216,706),23,(25,31,53),'#5d6588')
text(108,57,'TURNWAVE',22,'#f8f4ff',True)
text(274,65,'NX2  /  NATIVE ENGINE',14,'#c4b9e5')
text(1047,65,'BUILD 0.7',15,'#f2defe',True)
text(102,646,'L/R  VIEW     UP/DOWN  SELECT     A  PLAY/PAUSE     X  NEXT/SEARCH     Y  REFRESH     B/+  EXIT',13,'#e4dcf6')
# UI separators and playback-progress track.
ImageDraw.Draw(img).rounded_rectangle((796,456,1140,477),8,fill=(51,47,77,225))
ImageDraw.Draw(img).rounded_rectangle((796,550,1185,552),1,fill=(100,87,137,255))
for i in range(7):
    y0=249+i*47
    glass((93,y0,709,y0+41),14,(32,34,59),'#4a4e70')
    d=ImageDraw.Draw(img)
    d.ellipse((111,y0+11,129,y0+29),outline='#a3a7cf',width=2)
    d.ellipse((117,y0+17,123,y0+23),fill='#ded2fc')
# Dimensions exactly match source/main.c and assets in ui.S.
(out/'home.rgba').write_bytes(img.tobytes())
img.save(out/'home.png',optimize=True)
for pts,weight,cw,ch in [(16,'Regular',27,30),(22,'Semibold',34,38),(32,'Bold',50,52)]:
    f=font(f'OpenSans-{weight}.ttf',pts)
    glyphs=bytearray();widths=bytearray()
    for i in range(32,256):
        m=Image.new('L',(cw,ch))
        ImageDraw.Draw(m).text((1,1),chr(i),font=f,fill=255)
        widths.append(min(cw-1,max(3,round(f.getlength(chr(i))))))
        glyphs.extend(m.tobytes())
    (out/f'font{pts}.alpha').write_bytes(glyphs)
    (out/f'font{pts}.widths').write_bytes(widths)
print('Native UI generated:',out)
