"""Генерация звуков игры в WAV (обычный python, без зависимостей).

Настоящих записей у нас нет, поэтому синтезируем: шумовые всплески с
затуханием для выстрелов, щелчки для перезарядки, глухие удары для шагов,
низкий бум для взрыва, простые петли для музыки. Результат — в
ImportSource/_Generated/GeneratedAudio, оттуда его забирает import_sounds.py.

Запуск:  python Scripts/make_sounds.py
"""

import math
import os
import random
import struct
import wave

OUT = r"C:\Dev\RageStrike\ImportSource\_Generated\GeneratedAudio"
RATE = 44100

os.makedirs(OUT, exist_ok=True)
random.seed(1234)


def write_wav(name, samples):
    """samples — список float в диапазоне -1..1"""
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for s in samples:
            v = int(max(-1.0, min(1.0, s)) * 32767)
            frames += struct.pack("<h", v)
        w.writeframes(bytes(frames))
    print("{}  {:.2f} c".format(name, len(samples) / RATE))


def lowpass(samples, cutoff):
    """однополюсный фильтр: убирает песок из белого шума"""
    a = math.exp(-2.0 * math.pi * cutoff / RATE)
    out = []
    prev = 0.0
    for s in samples:
        prev = (1.0 - a) * s + a * prev
        out.append(prev)
    return out


def highpass(samples, cutoff):
    lp = lowpass(samples, cutoff)
    return [s - l for s, l in zip(samples, lp)]


def noise(seconds):
    return [random.uniform(-1.0, 1.0) for _ in range(int(RATE * seconds))]


def env_decay(samples, tail, attack=0.002):
    """резкая атака и экспоненциальный спад"""
    n = len(samples)
    out = []
    atk = max(1, int(RATE * attack))
    for i, s in enumerate(samples):
        a = min(1.0, i / atk)
        d = math.exp(-i / (RATE * tail))
        out.append(s * a * d)
    return out


def sine(freq, seconds, amp=1.0, sweep=0.0):
    n = int(RATE * seconds)
    out = []
    phase = 0.0
    for i in range(n):
        f = freq + sweep * (i / n)
        phase += 2.0 * math.pi * f / RATE
        out.append(math.sin(phase) * amp)
    return out


def mix(*tracks):
    length = max(len(t) for t in tracks)
    out = [0.0] * length
    for t in tracks:
        for i, s in enumerate(t):
            out[i] += s
    peak = max(1e-6, max(abs(s) for s in out))
    return [s / peak * 0.95 for s in out]


def silence(seconds):
    return [0.0] * int(RATE * seconds)


# ---------------------------------------------------------------- выстрелы
def gunshot(body_cut, tail, thump_freq, thump_amp=0.8, length=0.35):
    """Выстрел: щелчок сверху, шумовое тело и низкий удар пороха."""
    crack = env_decay(highpass(noise(length), 2000.0), tail * 0.35, 0.0005)
    body = env_decay(lowpass(noise(length), body_cut), tail)
    thump = env_decay(sine(thump_freq, length, thump_amp, -thump_freq * 0.6), tail * 1.4)
    return mix(crack, body, thump)


write_wav("Fire_Rifle", gunshot(1800.0, 0.055, 120.0, 0.9))
write_wav("Fire_SMG", gunshot(2600.0, 0.035, 150.0, 0.6, 0.25))
write_wav("Fire_Pistol", gunshot(2200.0, 0.045, 160.0, 0.7, 0.3))
write_wav("Fire_Sniper", gunshot(1200.0, 0.12, 80.0, 1.0, 0.6))
write_wav("Fire_Shotgun", gunshot(900.0, 0.10, 70.0, 1.0, 0.5))

# ------------------------------------------------------------------- нож
swish = env_decay(highpass(noise(0.22), 900.0), 0.05, 0.02)
write_wav("Knife_Slash", mix(swish))
hit = mix(env_decay(highpass(noise(0.18), 1500.0), 0.03, 0.0005),
          env_decay(sine(220.0, 0.18, 0.5, -120.0), 0.05))
write_wav("Knife_Hit", hit)

# ------------------------------------------------------------ перезарядка
def click(freq, tail=0.02, length=0.09):
    return mix(env_decay(highpass(noise(length), 2500.0), tail, 0.0004),
               env_decay(sine(freq, length, 0.6, -freq * 0.3), tail))


reload_seq = []
reload_seq += click(700.0)                 # магазин вышел
reload_seq += silence(0.28)
reload_seq += click(500.0, 0.03, 0.12)     # новый вставлен
reload_seq += silence(0.30)
reload_seq += click(900.0, 0.025)          # затвор
write_wav("Reload", reload_seq)

# ------------------------------------------------------------------ шаги
def step(cut, tail, thump, amp):
    return mix(env_decay(lowpass(noise(0.14), cut), tail, 0.001),
               env_decay(sine(thump, 0.14, amp, -thump * 0.5), tail * 1.2))


write_wav("Step_Walk", step(700.0, 0.035, 90.0, 0.5))
write_wav("Step_Run", step(1100.0, 0.045, 110.0, 0.8))
write_wav("Land", step(500.0, 0.07, 70.0, 1.0))

# --------------------------------------------------------------- гранаты
write_wav("Nade_Throw", mix(env_decay(highpass(noise(0.25), 700.0), 0.06, 0.03)))
write_wav("Nade_Bounce", click(400.0, 0.015, 0.07))

boom = mix(env_decay(lowpass(noise(1.2), 400.0), 0.28),
           env_decay(sine(60.0, 1.2, 1.0, -40.0), 0.35),
           env_decay(highpass(noise(1.2), 3000.0), 0.05, 0.0005))
write_wav("Explode", boom)

# флешка: щелчок и долгий звон в ушах
ring = mix(env_decay(sine(4200.0, 2.2, 0.55), 0.9, 0.001),
           env_decay(sine(5300.0, 2.2, 0.35), 0.8, 0.001),
           env_decay(highpass(noise(0.2), 4000.0), 0.03, 0.0004))
write_wav("Flash", ring)

# дым: шипение
write_wav("Smoke", env_decay(lowpass(noise(2.5), 3000.0), 1.6, 0.15))

# огонь: потрескивание, петля
burn = lowpass(noise(3.0), 1600.0)
for i in range(len(burn)):
    burn[i] *= 0.35 + 0.65 * abs(math.sin(i / RATE * 7.0)) * random.uniform(0.4, 1.0)
write_wav("Burn", mix(burn))

# --------------------------------------------------------------- интерфейс
write_wav("Buy", click(1200.0, 0.03, 0.1))
write_wav("HitMarker", mix(env_decay(sine(1800.0, 0.09, 0.8, -600.0), 0.03, 0.0005)))


# ----------------------------------------------------------------- музыка
def music_loop(name, root, pattern, seconds, pulse_hz, bright):
    """Простая петля: басовый пульс и аккорд из синусов сверху."""
    n = int(RATE * seconds)
    out = [0.0] * n
    beat = RATE / pulse_hz

    # бас на каждый удар
    for b in range(int(seconds * pulse_hz)):
        start = int(b * beat)
        hit = env_decay(sine(root, 0.35, 0.9, -root * 0.25), 0.09)
        for i, s in enumerate(hit):
            if start + i < n:
                out[start + i] += s

    # аккорд-подложка меняет ноты по кругу
    step_len = seconds / len(pattern)
    for idx, semis in enumerate(pattern):
        freq = root * 4.0 * (2.0 ** (semis / 12.0))
        start = int(idx * step_len * RATE)
        pad = sine(freq, step_len, 0.16)
        pad2 = sine(freq * 1.5, step_len, 0.09 if bright else 0.04)
        for i in range(len(pad)):
            if start + i < n:
                # плавные края, чтобы петля не щёлкала на стыке
                fade = min(1.0, i / (RATE * 0.15), (len(pad) - i) / (RATE * 0.15))
                out[start + i] += (pad[i] + pad2[i]) * fade
    write_wav(name, mix(out))


# меню: спокойная петля
music_loop("Music_Menu", 55.0, [0, 3, 7, 3], 16.0, 1.0, False)
# закупка: тревожнее и быстрее
music_loop("Music_Buy", 65.4, [0, 1, 5, 3], 12.0, 2.0, True)

print("готово, файлы в", OUT)
