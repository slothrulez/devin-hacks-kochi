"""Procedurally generate the game's sound effects and music loop (CC0, no samples). Writes data/sfx/*.wav."""
import numpy as np, wave, os
SR = 22050
OUT = os.path.join(os.path.dirname(__file__), '..', 'data', 'sfx')
rng = np.random.default_rng(7)

def env(n, a=0.002, d=0.1, curve=4.0):
    t = np.linspace(0, 1, n)
    att = np.clip(t / max(a, 1e-6), 0, 1)
    return att * np.exp(-t * curve / max(d, 1e-6))

def tone(freq, dur, kind='sine', sweep=1.0):
    n = int(SR * dur); t = np.arange(n) / SR
    f = freq * np.linspace(1, sweep, n)
    ph = 2 * np.pi * np.cumsum(f) / SR
    if kind == 'square': return np.sign(np.sin(ph))
    if kind == 'saw': return 2 * ((ph / (2 * np.pi)) % 1) - 1
    return np.sin(ph)

def noise(dur, lowpass=0.0):
    n = int(SR * dur); x = rng.uniform(-1, 1, n)
    if lowpass > 0:
        k = int(lowpass); x = np.convolve(x, np.ones(k) / k, mode='same')
    return x

def retro(x, bits=8):
    q = 2 ** (bits - 1); return np.round(np.clip(x, -1, 1) * q) / q

def save(name, x, gain=0.8):
    x = retro(x / (np.max(np.abs(x)) + 1e-9) * gain)
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype('<i2').tobytes())

def mix(*xs):
    n = max(len(x) for x in xs); out = np.zeros(n)
    for x in xs: out[:len(x)] += x
    return out

def hit(dur, f0, thump, crunch):
    n = int(SR * dur)
    body = tone(f0, dur, 'sine', sweep=0.4) * env(n, d=0.25) * thump
    nz = noise(dur, lowpass=3) * env(n, d=0.12) * crunch
    return body + nz

os.makedirs(OUT, exist_ok=True)
save('hit_light', hit(0.14, 180, 0.9, 0.7))
save('hit_medium', hit(0.20, 130, 1.0, 0.9))
save('hit_heavy', hit(0.30, 90, 1.2, 1.0))
save('knockdown', mix(hit(0.45, 70, 1.3, 1.1), tone(55, 0.45, 'square', 0.5) * env(int(SR * .45), d=0.5) * 0.3))
save('whiff', noise(0.16, lowpass=6) * env(int(SR * .16), a=0.03, d=0.12) * 0.7)
save('jump', tone(300, 0.18, 'square', sweep=2.2) * env(int(SR * .18), d=0.3) * 0.4)
save('fireball', mix(tone(700, 0.35, 'saw', sweep=0.35) * env(int(SR * .35), d=0.4) * 0.5, noise(0.35, lowpass=8) * env(int(SR * .35), d=0.3) * 0.5))
save('block', mix(tone(900, 0.08, 'square', sweep=0.8) * env(int(SR * .08), d=0.15) * 0.5, noise(0.08, lowpass=2) * env(int(SR * .08), d=0.1) * 0.4))

# Announcer-style stingers: short arpeggios on a square wave.
def stinger(notes, step=0.09, kind='square'):
    parts = [tone(f, step * 1.6, kind) * env(int(SR * step * 1.6), d=0.5) for f in notes]
    out = np.zeros(int(SR * (step * len(notes) + step)))
    for i, p in enumerate(parts):
        s = int(i * step * SR); out[s:s + len(p)] += p
    return out
save('round', stinger([392, 523, 659, 784]))
save('fight', mix(stinger([659, 784, 1046], step=0.08), hit(0.35, 80, 1.0, 0.8) * 0.8))
save('ko', mix(stinger([784, 659, 523, 392, 261], step=0.14, kind='saw'), hit(0.6, 60, 1.4, 1.0)))

# Music: chenda-melam inspired percussion loop (4 bars, 150 bpm, 2 beats of 4 subdivisions each bar => 8 steps/bar).
bpm = 150; step = 60 / bpm / 2; bars = 4; steps = bars * 8
loop = np.zeros(int(SR * step * steps) + SR)
def place(i, x): s = int(i * step * SR); loop[s:s + len(x)] += x
chenda_low = lambda: hit(0.22, 95, 1.3, 0.5)
chenda_high = lambda: hit(0.10, 420, 0.5, 1.2)
elathalam = lambda: mix(tone(2400, 0.25, 'sine') * env(int(SR * .25), d=0.6) * 0.25, tone(3600, 0.25, 'sine') * env(int(SR * .25), d=0.4) * 0.15)
pattern_low = [1, 0, 0, 1, 0, 1, 0, 0]
pattern_high = [0, 1, 1, 0, 1, 0, 1, 1]
for b in range(bars):
    for i in range(8):
        k = b * 8 + i
        if pattern_low[i]: place(k, chenda_low())
        if pattern_high[i] and not (b == 3 and i >= 6): place(k, chenda_high() * 0.8)
        if i in (0, 4): place(k, elathalam())
    if b == 3:  # fill
        for i in (6, 6.5, 7, 7.5): place(b * 8 + i, chenda_high() * 0.9)
# simple drone underneath (tanpura-like)
n = len(loop); t = np.arange(n) / SR
drone = (np.sin(2 * np.pi * 110 * t) + 0.5 * np.sin(2 * np.pi * 220 * t) + 0.25 * np.sin(2 * np.pi * 165 * t)) * 0.12
loop += drone
loop = loop[:int(SR * step * steps)]
save('music_loop', loop, gain=0.7)
print('wrote', sorted(os.listdir(OUT)))
