"""Generate our own quiet stereo demonstration audio; no third-party assets."""
import math,struct,wave
from pathlib import Path
def generate(path,seconds=12,rate=48000):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(path),'wb') as out:
        out.setnchannels(2);out.setsampwidth(2);out.setframerate(rate)
        data=bytearray()
        for n in range(int(seconds*rate)):
            t=n/rate;fade=min(1,t/.1,(seconds-t)/.2)
            pulse=.3+.7*math.exp(-8*(t%1))
            a=int(32767*.012*fade*pulse*(math.sin(2*math.pi*220*t)+.25*math.sin(2*math.pi*330*t)))
            data.extend(struct.pack('<hh',a,a))
        out.writeframes(data)
    return path
if __name__=='__main__':
    import sys
    print(generate(sys.argv[1] if len(sys.argv)>1 else Path(__file__).parent/'runtime/show-control-assets/Demonstration.wav'))
