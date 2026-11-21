"""Render Turnwave Liquid Library native assets and glyph atlases."""
from pathlib import Path
import os,sys
import numpy as np
from PIL import Image,ImageDraw,ImageFilter,ImageFont

WIDTH,HEIGHT=1280,720
OUT=Path(sys.argv[1] if len(sys.argv)>1 else 'build/ui')
OUT.mkdir(parents=True,exist_ok=True)
FONTS=Path(os.environ.get('TURNWAVE_FONTS','/usr/share/fonts/truetype/open-sans'))
y,x=np.mgrid[:HEIGHT,:WIDTH].astype(np.float32)
base=np.broadcast_to(np.array([10,15,24],dtype=np.float32),(HEIGHT,WIDTH,3)).copy()
for cx,cy,sx,sy,color in [
    (1010,370,415,325,(42,25,78)),(1110,90,270,220,(28,52,88)),
    (730,660,365,175,(13,43,59)),(1180,650,225,280,(35,16,66)),
    (220,180,280,325,(10,12,32))]:
    base+=np.exp(-((x-cx)/sx)**2-((y-cy)/sy)**2)[...,None]*color

dx=x-958;dy=(y-365)*1.1;theta=np.arctan2(dy,dx)
r=np.hypot(dx,dy)+10*np.cos(3*theta+.5)+6*np.sin(2*theta)
shine=np.maximum(0,.55+.45*np.sin(2*theta+.8));cyan=.5+.5*np.cos(theta+1.8);rose=.5+.5*np.sin(theta+.25)
surf=np.exp(-((r-178)/50)**4)
base+=(surf*(.12+.44*shine))[...,None]*np.stack([80+110*rose,98+85*cyan,np.full_like(x,180)],axis=-1)
for ring,t,p in [(133,3.7,.85),(145,2.4,.28),(170,1.8,.15),(196,3.2,.36),(215,3.3,.93),(224,1.2,.33)]:
    reflection=np.exp(-((r-ring)/t)**2)*p*(.18+.82*shine)
    base+=reflection[...,None]*np.stack([110+100*rose,88+144*cyan,np.full_like(x,225)],axis=-1)
for ring in (244,267,291):
    base+=np.exp(-((r-ring)/.85)**2)[...,None]*(.2+.8*shine)[...,None]*(23,30,49)
art=Image.fromarray(np.clip(base,0,255).astype('uint8')).convert('RGBA')

def glass(rect,corner,tint,edge,shin=55):
    global art
    m=Image.new('L',(WIDTH,HEIGHT))
    ImageDraw.Draw(m).rounded_rectangle(rect,corner,fill=255)
    blurred=art.filter(ImageFilter.GaussianBlur(17))
    art.paste(Image.blend(blurred,Image.new('RGBA',(WIDTH,HEIGHT),(*tint,255)),.36),(0,0),m)
    overlay=np.zeros((HEIGHT,WIDTH,4),dtype=np.uint8)
    overlay[:,:,:3]=[228,235,255]
    fade=np.clip(1-(y-rect[1])/(rect[3]-rect[1]),0,1)
    overlay[:,:,3]=(np.asarray(m)/255*shin*fade**2).astype('uint8')
    art.alpha_composite(Image.fromarray(overlay))
    ImageDraw.Draw(art).rounded_rectangle(rect,corner,outline=edge,width=1)

def label(at,text,size=19,bold=False,color='#fff',anchor='lt'):
    font=ImageFont.truetype(str(FONTS / ('OpenSans-Bold.ttf' if bold else 'OpenSans-Regular.ttf')),size)
    ImageDraw.Draw(art).text(at,text,font=font,fill=color,anchor=anchor)

glass((64,48,247,96),24,(29,34,60),'#5b5c82',37)
draw=ImageDraw.Draw(art)
draw.ellipse((84,63,100,79),outline='#dbd7ff',width=2)
draw.ellipse((89,68,95,74),fill='#b6a7ff')
label((112,71),'Turnwave NX2',16,True,anchor='lm')
glass((64,140,664,609),35,(19,22,41),'#68698f',34)
glass((685,140,1217,609),35,(21,25,46),'#7776a0',38)
label((100,178),'Your music',37,True)
label((100,223),'Your library, on Nintendo Switch 2.',16,color='#c7d0ec')
label((724,181),'YOUR LIBRARY',15,True,color='#c6bdec')
label((724,217),'APPLE MUSIC · LIQUID',14,color='#d7c4fb')
for i in range(7):
    y0=265+i*44
    glass((94,y0,633,y0+40),16,(29,33,55),'#464d6e',17)
    draw=ImageDraw.Draw(art)
    draw.ellipse((114,y0+11,132,y0+29),outline='#a6b1dd',width=2)
    draw.ellipse((120,y0+17,126,y0+23),fill='#c3b5ee')
# Reserved right-panel details end above the separator.
# The button label and status are drawn dynamically by native/source/main.c.
glass((730,468,1170,471),2,(113,94,174),'#6e5d98',18)
glass((754,484,1148,543),25,(97,74,159),'#b2a6dd',42)
# Global footer contains one controls hint and one separate dynamic status.
glass((64,621,1217,708),24,(28,42,64),'#555d78',24)
label((96,633),'L/R  Sections     Up/Down  Browse     X  Connect / Sync     Y  Reload     B/+  Exit',15,color='#d6def4')
(OUT/'home.rgba').write_bytes(art.tobytes())
art.save(OUT/'home.png',optimize=True)
# ASCII + Latin-1 font sheets; glyphs include each 32..255 code point,
# with known fixed cell size and a measured logical advance width.
for sz,weight,cellw,cellh in [(16,'Regular',27,30),(22,'Semibold',34,38),(32,'Bold',50,52)]:
    f=ImageFont.truetype(str(FONTS/f'OpenSans-{weight}.ttf'),sz)
    glyphs=bytearray();widths=bytearray()
    for codepoint in range(32,256):
        c=chr(codepoint)
        m=Image.new('L',(cellw,cellh))
        ImageDraw.Draw(m).text((1,1),c,font=f,fill=255,anchor='lt')
        widths.append(min(cellw-1,max(3,round(f.getlength(c)))))
        glyphs.extend(m.tobytes())
    (OUT/f'font{sz}.alpha').write_bytes(glyphs)
    (OUT/f'font{sz}.widths').write_bytes(widths)
print('Liquid UI and font assets generated at',OUT)
