"""Quiet original offline effects; no recordings or third-party samples.

The mix is authored independently in Graphics/audio.json. Regenerating these
waveforms must not overwrite a designer's volume or cooldown adjustments.
"""
from pathlib import Path
import array, hashlib, json, math, random, wave

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Art/Audio'; OUT.mkdir(parents=True,exist_ok=True)
RATE=48000
SPECS={'select':(.055,160,140),'confirm':(.12,210,200),'cancel':(.08,150,130),
       'error':(.18,150,110),'build':(.24,120,90),'complete':(.40,220,220),
       'alert':(.48,210,210),'laser':(.20,540,180),'kinetic':(.34,95,38),
       'missile':(.52,145,110),'plasma':(.44,220,85),'rex':(.42,190,120)}
records={}
for name,(duration,start,end) in SPECS.items():
    rng=random.Random('seige2222:soft:'+name);count=int(RATE*duration);pcm=array.array('h');phase=0.;filtered=0.;filtered2=0.
    for index in range(count):
        t=index/RATE;p=t/duration;frequency=start+(end-start)*p
        phase+=2*math.pi*frequency/RATE;noise=rng.uniform(-1,1);filtered=.94*filtered+.06*noise;filtered2=.94*filtered2+.06*filtered
        envelope=math.sin(min(1.,t/.018)*math.pi/2)**2*math.sin(min(1.,(duration-t)/.055)*math.pi/2)**2*math.exp(-p*3.2)
        if name=='kinetic':value=.50*filtered2+.21*math.sin(phase)*math.exp(-p*4)
        elif name=='missile':value=1.25*filtered2+.055*math.sin(phase)
        elif name=='plasma':value=.12*math.sin(phase+.35*math.sin(phase*.31))+.40*filtered2
        elif name=='laser':value=.13*math.sin(phase)+.25*filtered2
        elif name=='rex':
            # Stylized two-formant woof, not a recording of the real Rex.
            # Disabled in the shipping mix until a convincing canine source exists.
            value=(.14*math.sin(phase)+.05*math.sin(phase*3.1)+.15*filtered2)*max(0,math.sin(math.pi*p))**.6
        elif name=='alert':value=.13*math.sin(phase)+.03*math.sin(phase*1.5)
        elif name=='complete':value=.07*(math.sin(phase)+.50*math.sin(phase*1.2599)+.25*math.sin(phase*1.5))
        else:value=.09*math.sin(phase)+.25*filtered2
        pcm.append(round(max(-.85,min(.85,value*envelope))*32767))
    path=OUT/(name+'.wav')
    with wave.open(str(path),'wb') as sound:
        sound.setnchannels(1);sound.setsampwidth(2);sound.setframerate(RATE);sound.writeframes(pcm.tobytes())
    records[name]={'file':path.name,'seconds':duration,'sample_rate':RATE,'channels':1,'peak':max(abs(x) for x in pcm)/32767,
                   'rms':math.sqrt(sum(x*x for x in pcm)/len(pcm))/32767,
                   'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'source':'Original mathematical synthesis','license':'CC0-1.0'}
(OUT/'manifest.json').write_text(json.dumps(records,indent=2)+'\n')
print('ORIGINAL_AUDIO_READY',len(records),'clips; maximum peak',max(v['peak'] for v in records.values()))
