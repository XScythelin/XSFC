"""Summarize already-decoded configurator parser output (no board access)."""
import json
from pathlib import Path
import sys
from collections import Counter
import math
import statistics

p = Path(sys.argv[1])
raw = p.read_bytes()
s = raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8')
d = json.loads(s[s.index('{"config"'):])
fields = d['fields']
frames = d['frames']
print('FIELDS', fields)
print('FRAMES', len(frames), 'INVALID', d['invalid'])
print('SPAN_S', (frames[-1][1]-frames[0][1])/1e6)
print('ITERATION_HZ', (frames[-1][0]-frames[0][0])*1e6/(frames[-1][1]-frames[0][1]))
print('TIME_DELTAS', Counter(b[1]-a[1] for a,b in zip(frames,frames[1:])).most_common(12))
print('EVENTS', d['events'][-8:])
print('CONFIG', {k:v for k,v in d['config'].items() if any(t in k.lower() for t in ('acc','align','trim','gyro','looptime','debug','pid','rate'))})

def stats(values):
    v = sorted(values)
    return dict(min=v[0], median=statistics.median(v), mean=statistics.mean(v), p95=v[int((len(v)-1)*.95)], max=v[-1])

idx = {name:i for i,name in enumerate(fields)}
acc = [idx[f'accSmooth[{i}]'] for i in range(3)]
one_g = d['config'].get('acc_1G', 2048)
for begin in range(0, math.ceil((frames[-1][1]-frames[0][1])/1e6), 2):
    window = [f for f in frames if begin <= (f[1]-frames[0][1])/1e6 < begin+2]
    if not window: continue
    out = {'t':begin, 'acc_g':stats([math.sqrt(sum(f[i]**2 for i in acc))/one_g for f in window])}
    for prefix in ('rcCommand', 'setpoint', 'gyroADC', 'accSmooth', 'motor'):
        for axis in range(4 if prefix in ('rcCommand','motor') else 3):
            name = f'{prefix}[{axis}]'
            if name in idx: out[name] = round(statistics.mean(f[idx[name]] for f in window),3)
    print('WINDOW',json.dumps(out))
