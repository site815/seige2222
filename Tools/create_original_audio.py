"""Original offline synthesized game effects. No recordings or third-party samples."""
from pathlib import Path
import array, hashlib, json, math, random, wave

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Art/Audio'; OUT.mkdir(parents=True,exist_ok=True)
RATE=48000
SPECS={'select':(.09,1250,1900),'confirm':(.20,680,1350),'cancel':(.15,950,500),
       'error':(.26,230,170),'build':(.35,140,440),'complete':(.65,520,1040),
       'alert':(.65,660,880),'laser':(.25,2700,260),'kinetic':(.35,90,30),
       'missile':(.65,160,520),'plasma':(.65,480,72),'rex':(.42,190,120)}
records={}
for name,(duration,start,end) in SPECS.items():
    rng=random.Random('seige2222:'+name);count=int(RATE*duration);pcm=array.array('h');phase=0.;filtered=0.
    for index in range(count):
        t=index/RATE;p=t/duration;frequency=start+(end-start)*p
        phase+=2*math.pi*frequency/RATE;noise=rng.uniform(-1,1);filtered=.89*filtered+.11*noise
        envelope=min(1.,t/.008)*min(1.,(duration-t)/.04)*math.exp(-p*2.6)
        if name=='kinetic':value=.60*filtered+.30*math.sin(phase)*math.exp(-p*4)+.25*noise*math.exp(-p*25)
        elif name=='missile':value=.85*filtered+.08*math.sin(phase+math.sin(phase*.21)*2)
        elif name=='plasma':value=.25*math.sin(phase+math.sin(phase*.31)*3)+.32*filtered
        elif name=='laser':value=.27*math.sin(phase+math.sin(phase*.47)*1.3)+.04*noise
        elif name=='rex':
            # Stylized two-formant woof, not a recording of the real Rex.
            value=(.24*math.sin(phase)+.13*math.sin(phase*3.1)+.08*noise)*max(0,math.sin(math.pi*p))**.6
        elif name=='alert':value=.23*math.sin(2*math.pi*(660 if int(t*8)%2 else 880)*t)
        elif name=='complete':value=.15*(math.sin(phase)+.55*math.sin(phase*1.25)+.38*math.sin(phase*1.5))
        else:value=.24*math.sin(phase)+.045*math.sin(phase*2)
        pcm.append(round(max(-.85,min(.85,value*envelope))*32767))
    path=OUT/(name+'.wav')
    with wave.open(str(path),'wb') as sound:
        sound.setnchannels(1);sound.setsampwidth(2);sound.setframerate(RATE);sound.writeframes(pcm.tobytes())
    records[name]={'file':path.name,'seconds':duration,'sample_rate':RATE,'channels':1,'peak':max(abs(x) for x in pcm)/32767,
                   'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'source':'Original mathematical synthesis','license':'CC0-1.0'}
(OUT/'manifest.json').write_text(json.dumps(records,indent=2)+'\n')
config={'version':1,'master_volume':.65,'maximum_voices_per_frame':8,
        'cues':{name:f'/Game/Art/Audio/S_{name}.S_{name}' for name in records}}
config['cues']['energy']=config['cues']['laser']
(ROOT/'Graphics/audio.json').write_text(json.dumps(config,indent=2)+'\n')
print('ORIGINAL_AUDIO_READY',len(records),'clips; maximum peak',max(v['peak'] for v in records.values()))
