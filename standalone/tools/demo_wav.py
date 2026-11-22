#!/usr/bin/env python3
"""Create an original, unencumbered PCM sine-wave hardware-test track."""
import math,struct,sys,wave
from pathlib import Path
out=Path(sys.argv[1] if len(sys.argv)>1 else 'test-tone.wav')
out.parent.mkdir(parents=True,exist_ok=True)
rate=48000;seconds=6
with wave.open(str(out),'wb') as w:
    w.setnchannels(2);w.setsampwidth(2);w.setframerate(rate)
    for part in range(seconds*20):
        data=bytearray()
        for n in range(rate//20):
            t=(part*(rate//20)+n)/rate
            chord=(220,277.18,329.63,440)[(int(t)//2)%4]
            fade=min(1.0,t*5,(seconds-t)*4)
            amp=0.16*max(0,fade)
            left=int(32767*amp*(math.sin(2*math.pi*chord*t)+0.23*math.sin(2*math.pi*chord*2*t)))
            right=int(32767*amp*(math.sin(2*math.pi*chord*t+0.12)+0.23*math.sin(2*math.pi*chord*2*t)))
            data+=struct.pack('<hh',left,right)
        w.writeframesraw(data)
print(f'Wrote {out} (6 seconds, 48000 Hz, 16-bit stereo)')
