"""
Space Sonar 209 (NINE HANDS | DSP) - apercu de la facade, d'apres la Roland RE-201 Space Echo.
Repere de travail 1600 x 800 unites (fenetre a 100 %), rendu a AS px par unite.
Lumiere : en haut a gauche a 45 deg, ombres vers le bas a droite.
"""
import os, sys, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import photo_controls as pc
import toggle_photo as tp

AS = 2
W, H = 1600, 1400
PW, PH = W * AS, H * AS
FONT = '/usr/share/fonts/truetype/google-fonts/Poppins-%s.ttf'
rng = np.random.default_rng(7)

yy, xx = np.mgrid[0:PH, 0:PW].astype(np.float32)
U = lambda v: v * AS


def font(style, size_u):
    return ImageFont.truetype(FONT % style, int(round(size_u * AS)))


def noise(scale_px, amp):
    n = rng.standard_normal((PH, PW)).astype(np.float32)
    if scale_px > 0:
        n = cv2.GaussianBlur(n, (0, 0), scale_px)
        n /= n.std() + 1e-6
    return n * amp


def rrect_mask(x0, y0, x1, y1, r, soft=1.0):
    x0, y0, x1, y1, r = U(x0), U(y0), U(x1), U(y1), U(r)
    cx = np.clip(xx, x0 + r, x1 - r); cy = np.clip(yy, y0 + r, y1 - r)
    d = np.hypot(xx - cx, yy - cy) - r
    return np.clip(0.5 - d / soft, 0, 1)


def blend(img, col, m):
    m = m[..., None]
    return img * (1 - m) + np.asarray(col, np.float32) * m


def paste(img, spr, cx, cy):
    """spr RGBA float/uint8 centre en (cx, cy) unites"""
    spr = spr.astype(np.float32)
    h, w = spr.shape[:2]
    x0 = int(round(U(cx) - w / 2)); y0 = int(round(U(cy) - h / 2))
    xa, ya = max(0, x0), max(0, y0); xb, yb = min(PW, x0 + w), min(PH, y0 + h)
    s = spr[ya - y0:yb - y0, xa - x0:xb - x0]
    a = s[..., 3:4] / 255.0
    img[ya:yb, xa:xb] = img[ya:yb, xa:xb] * (1 - a) + s[..., :3] * a


def soft_shadow(img, mask, dx, dy, blur, amt):
    m = cv2.GaussianBlur(np.roll(np.roll(mask, int(U(dy)), 0), int(U(dx)), 1), (0, 0), U(blur))
    return img * (1 - amt * m[..., None])



def rect_text_layer():
    t = Image.new('RGBA', (PW, PH), (0, 0, 0, 0))
    return t, ImageDraw.Draw(t)


def comp(img, layer):
    ta = np.asarray(layer, np.float32)
    a = ta[..., 3:4] / 255.0
    return img * (1 - a) + ta[..., :3] * a


def text_on(td, x, y, s, size, style='Medium', fill=(235, 238, 232, 255), anchor='mm', track=0.0):
    f = font(style, size)
    if track == 0:
        td.text((U(x), U(y)), s, font=f, fill=fill, anchor=anchor); return
    widths = [td.textlength(c, font=f) for c in s]
    tot = sum(widths) + U(track) * (len(s) - 1)
    cx = U(x) - (tot / 2 if anchor[0] == 'm' else 0)
    for c, w_ in zip(s, widths):
        td.text((cx, U(y)), c, font=f, fill=fill, anchor='lm'); cx += w_ + U(track)


def fill(img, m, col):
    col = np.asarray(col, np.float32)
    if col.ndim == 1: col = col[None, None, :]
    return img * (1 - m[..., None]) + col * m[..., None]


def brushed(base, amp=6):
    b = cv2.GaussianBlur(rng.standard_normal((PH, PW)).astype(np.float32), (0, 0), sigmaX=U(30), sigmaY=0.6)
    b /= b.std()
    return (base + amp * b + noise(0.4, 1.2))[..., None] * np.array([0.985, 0.995, 1.0])



BLACKTXT = (20, 22, 22, 255)
WHITETXT = (236, 238, 234, 255)
G2 = (196, 202, 198, 255)
GREEN = np.array([52, 132, 58], np.float32)
DIM = (120, 124, 124, 255)


def disk(cx, cy, r):
    return np.clip(U(r) - np.hypot(xx - U(cx), yy - U(cy)), 0, 1)


def ring(cx, cy, r0, r1):
    d = np.hypot(xx - U(cx), yy - U(cy))
    return np.clip(U(r1) - d, 0, 1) * np.clip(d - U(r0), 0, 1)


img = np.zeros((PH, PW, 3), np.float32)
# ================================================================== fond general (graphite)
allm = rrect_mask(0, 0, W, H, 18)
img = fill(img, allm, (30 + noise(0.8, 1.0))[..., None] * np.array([0.97, 0.99, 1.02]))
# barre du haut
hdr = rrect_mask(0, 0, W, 96, 18) * (yy < U(96))
img = fill(img, hdr, np.array([22, 23, 24]))
img *= (1 - 0.5 * np.clip(1 - np.abs(yy - U(96)) / U(1.5), 0, 1))[..., None]
# fond de la zone appareil (gris sombre, comme la photo)
stage = (yy >= U(96)) * (yy < U(912)) * allm
img = fill(img, stage, (40 + 10 * np.clip(1 - (yy - U(96)) / U(800), 0, 1) + noise(0.8, 1.0))[..., None] * np.ones(3))

# ================================================================== appareil (tolex)
TX0, TY0, TX1, TY1 = 40, 140, 1560, 900
tolex_m = rrect_mask(TX0, TY0, TX1, TY1, 40)
img = soft_shadow(img, tolex_m, 6, 8, 12, 0.6)
tol = 26 + noise(2.0, 5.0) + noise(0.7, 3.0) + noise(0.0, 1.5)
img = fill(img, tolex_m, tol[..., None] * np.array([1.0, 1.0, 1.03]))
rim = np.clip(tolex_m - rrect_mask(TX0 + 6, TY0 + 6, TX1 - 6, TY1 - 6, 34), 0, 1)
img += (rim * 14)[..., None]
# poignees laterales (embouts metal)
for (x0, x1) in ((20, 46), (1554, 1580)):
    for yc in (300, 700):
        hm = rrect_mask(x0, yc - 34, x1, yc + 34, 8)
        img = soft_shadow(img, hm, 3, 3, 4, 0.5)
        img = fill(img, hm, (60 + 50 * np.clip(1 - ((xx - U(x0)) + (yy - U(yc - 34))) / U(90), 0, 1))[..., None] * np.ones(3))
# pied central
foot = rrect_mask(560, 900, 1040, 914, 4)
img = fill(img, foot, np.array([18, 18, 18]))

# cadre chrome de la facade
FX0, FX1, FY0, FY1 = 116, 1484, 280, 790
fr_m = rrect_mask(FX0 - 9, FY0 - 9, FX1 + 9, FY1 + 9, 8)
img = soft_shadow(img, fr_m, 5, 5, 7, 0.6)
img = fill(img, fr_m, (140 + 70 * np.clip(1 - (yy - U(FY0)) / U(FY1 - FY0), 0, 1))[..., None] * np.ones(3))
band = rrect_mask(FX0, FY0, FX1, 372, 3)
img = fill(img, band, brushed(184))
panel = rrect_mask(FX0, 374, FX1, 684, 2)
img = fill(img, panel, (15 + noise(0.6, 1.2))[..., None] * np.ones(3))
# MODE : vert plein
mg = rrect_mask(578, 382, 800, 676, 6)
img = fill(img, mg, GREEN * (1 + noise(0.6, 0.02))[..., None])
# section droite : cadre vert, interieur noir
eg = rrect_mask(808, 382, 1232, 676, 6)
img = fill(img, eg, GREEN * (1 + noise(0.6, 0.02))[..., None])
ei = rrect_mask(820, 394, 1220, 664, 4)
img = fill(img, ei, (14 + noise(0.6, 1.0))[..., None] * np.ones(3))
# bandeau des prises
bs = rrect_mask(FX0, 686, FX1, FY1, 3)
img = fill(img, bs, brushed(156, 4))
BOX = [(128, 268, 'MIC'), (276, 430, 'FROM P.A.'), (438, 712, 'INSTRUMENT'), (1000, 1244, 'OUTPUT'), (1252, 1472, 'ECHO CANCEL')]
for x0, x1, _ in BOX:
    bx = np.clip(rrect_mask(x0, 696, x1, 780, 5) - rrect_mask(x0 + 2, 698, x1 - 2, 778, 4), 0, 1)
    img = fill(img, bx, np.array([40, 42, 42]))
for y in (373, 685):
    ln = np.clip(1 - np.abs(yy - U(y)) / U(1.6), 0, 1) * (xx > U(FX0)) * (xx < U(FX1))
    img *= (1 - 0.8 * ln)[..., None]

# ================================================================== separation + panneau etendu
sep = (yy >= U(912)) * (yy < U(946)) * allm
img = fill(img, sep, np.array([24, 25, 26]))
tab = rrect_mask(700, 918, 900, 940, 6)
img = fill(img, tab, np.array([46, 48, 50]))
lower = (yy >= U(946)) * allm
img = fill(img, lower, (33 + noise(0.8, 1.0))[..., None] * np.array([0.97, 0.99, 1.02]))
SX0, SY0, SX1, SY1 = 50, 990, 800, 1380
scr = rrect_mask(SX0, SY0, SX1, SY1, 8)
img = fill(img, scr, (16 + noise(1.0, 0.8))[..., None] * np.ones(3))
inn = cv2.GaussianBlur(1 - scr, (0, 0), U(4))
img *= (1 - 0.5 * scr * np.clip(np.roll(np.roll(inn, int(U(2)), 0), int(U(2)), 1), 0, 1))[..., None]

# ================================================================== eclairage 45 deg sur l'appareil seulement
t = ((xx / PW) + (yy / 912)) / 2.0
lit = tolex_m
img = img * (1 + lit * (0.08 - 0.20 * t))[..., None]

# ================================================================== VU (face noire)
vx0, vy0, vx1, vy1 = 252, 394, 440, 494
bez = rrect_mask(vx0 - 7, vy0 - 7, vx1 + 7, vy1 + 7, 4)
img = soft_shadow(img, bez, 3, 3, 4, 0.5)
img = fill(img, bez, (110 + 90 * np.clip(1 - ((xx - U(vx0)) + (yy - U(vy0))) / U(230), 0, 1))[..., None] * np.ones(3))
face = rrect_mask(vx0, vy0, vx1, vy1, 3)
glow = np.exp(-(((xx - U((vx0 + vx1) / 2)) / U(110)) ** 2 + ((yy - U(vy1)) / U(70)) ** 2))
img = fill(img, face, np.array([12, 16, 14]) + glow[..., None] * np.array([12, 30, 16]))
L, td = rect_text_layer()
vc = ((vx0 + vx1) / 2, vy1 + 26); vr = 96
GLOWG = (160, 238, 176, 255); RED = (236, 76, 60, 255)
for i in range(11):
    a = math.radians(-46 + 92 * i / 10 - 90)
    r0 = vr - 7 - (4 if i % 2 == 0 else 0)
    td.line([U(vc[0] + r0 * math.cos(a)), U(vc[1] + r0 * math.sin(a)), U(vc[0] + vr * math.cos(a)), U(vc[1] + vr * math.sin(a))], fill=RED if i >= 7 else GLOWG, width=int(U(1.5)))
td.arc([U(vc[0] - vr), U(vc[1] - vr), U(vc[0] + vr), U(vc[1] + vr)], -136, -81.4, fill=GLOWG, width=int(U(1.4)))
td.arc([U(vc[0] - vr), U(vc[1] - vr), U(vc[0] + vr), U(vc[1] + vr)], -81.4, -44, fill=RED, width=int(U(3)))
text_on(td, vc[0], vy1 - 30, 'VU', 13, 'Medium', GLOWG)
na = math.radians(-22 - 90); rs = (vc[1] - vy1 + 2) / -math.sin(na)
td.line([U(vc[0] + rs * math.cos(na)), U(vc[1] + rs * math.sin(na)), U(vc[0] + (vr + 3) * math.cos(na)), U(vc[1] + (vr + 3) * math.sin(na))], fill=(225, 230, 222, 255), width=int(U(1.4)))
img = comp(img, L)
img += (face * np.exp(-(((xx - U(vx0)) - (yy - U(vy0)) * 1.3 - U(40)) / U(22)) ** 2) * 0.10 * 255)[..., None]

# ================================================================== serigraphies
L, td = rect_text_layer()
T = lambda *a, **k: text_on(td, *a, **k)
T(150, 328, 'SPACE SONAR', 36, 'Bold', BLACKTXT, anchor='lm', track=2.5)
T(482, 330, '209', 36, 'Light', BLACKTXT, anchor='lm', track=1.5)
T(200, 466, 'PEAK', 10, 'Medium', WHITETXT, track=1.2); T(200, 479, 'LEVEL', 10, 'Medium', WHITETXT, track=1.2)
LK = [(190, 'MIC', 'VOLUME'), (345, 'FROM P.A.', 'VOLUME'), (500, 'INSTRUMENT', 'VOLUME')]
for x, a1, a2 in LK:
    T(x, 540, a1, 11, 'Medium', WHITETXT, track=1.1); T(x, 554, a2, 11, 'Medium', WHITETXT, track=1.1)
T(689, 404, 'MODE SELECTOR', 12.5, 'SemiBold' if os.path.exists(FONT % 'SemiBold') else 'Medium', WHITETXT, track=1.4)
MX, MY, MR = 689, 528, 46
MANG = [-150 + 30 * i for i in range(11)] + [180]
for i in range(12):
    a = math.radians(MANG[i] - 90)
    td.line([U(MX + (MR + 7) * math.cos(a)), U(MY + (MR + 7) * math.sin(a)), U(MX + (MR + 14) * math.cos(a)), U(MY + (MR + 14) * math.sin(a))], fill=WHITETXT, width=int(U(1.8)))
    if i < 11:
        T(MX + (MR + 25) * math.cos(a), MY + (MR + 25) * math.sin(a), str(i + 1), 11.5, 'Medium', WHITETXT)
T(MX, MY + 80, 'REVERB', 10, 'Medium', WHITETXT, track=1.2); T(MX, MY + 93, 'ONLY', 10, 'Medium', WHITETXT, track=1.2)
RK = [(882, 478, 'BASS'), (1020, 478, 'TREBLE'), (1158, 478, 'REVERB VOLUME'), (882, 612, 'REPEAT RATE'), (1020, 612, 'INTENSITY'), (1158, 612, 'ECHO VOLUME')]
for x, y, lab in RK:
    T(x, y - 60, lab, 11, 'Medium', WHITETXT, track=1.1)
T(1362, 548, 'POWER', 11, 'Medium', WHITETXT, track=1.2)
T(1412, 584, 'ON', 9.5, 'Medium', WHITETXT, track=1); T(1412, 660, 'OFF', 9.5, 'Medium', WHITETXT, track=1)
# bandeau des prises
for x0, x1, lab in BOX:
    T((x0 + x1) / 2 if lab not in ('INSTRUMENT', 'OUTPUT', 'ECHO CANCEL') else x0 + 14, 712, lab, 10.5, 'Medium', BLACKTXT, track=1.1, anchor='mm' if lab in ('MIC', 'FROM P.A.') else 'lm')
T(600, 764, 'ECHO', 8.5, 'Medium', BLACKTXT, track=0.8); T(672, 764, 'NORMAL', 8.5, 'Medium', BLACKTXT, track=0.8)
T(820, 712, 'MIC   P.A.   INST', 10.5, 'Medium', BLACKTXT, track=0.8)
for i, s_ in enumerate('HML'):
    T(1040 + i * 26, 730, s_, 9.5, 'Medium', BLACKTXT)
T(1380, 764, 'ON   ·   OFF', 8.5, 'Medium', BLACKTXT, track=0.8)
# panneau etendu
T(425, 975, 'BLOCK DIAGRAM', 11, 'Medium', G2, track=1.3)
T(905, 1110, 'STEREO MODE', 10.5, 'Medium', G2, track=1.0)
T(1120, 1010, 'RATE RANGE', 10.5, 'Medium', G2, track=1.0)
T(1120, 1210, 'SPEED', 10, 'Medium', G2, track=1.0)
T(1410, 1010, 'TAPE SPEED LFO', 10.5, 'Medium', G2, track=1.0)
for x, lab in ((1310, 'RATE'), (1410, 'WAVEFORM'), (1510, 'DEPTH')):
    T(x, 1105, lab, 10, 'Medium', G2, track=1.0)
EK = [(905, 'CONDITION'), (1010, 'W/F'), (1120, 'MOTOR TORQUE'), (1265, 'LOW CUT'), (1385, 'HIGH CUT'), (1510, 'MASTER')]
for x, lab in EK:
    T(x, 1268, lab, 10, 'Medium', G2, track=1.0)
img = comp(img, L)

# logo noir sur l'alu
logo = Image.open('/tmp/logo_Black.png').convert('RGBA')
lw = int(U(220)); lh = int(logo.height * lw / logo.width)
la = np.asarray(logo.resize((lw, lh), Image.LANCZOS), np.float32)
lx, ly = int(U(1456) - lw), int(U(327) - lh / 2)
a = la[..., 3:4] / 255.0 * 0.92
img[ly:ly + lh, lx:lx + lw] = img[ly:ly + lh, lx:lx + lw] * (1 - a) + la[..., :3] * a

# ================================================================== potards
def chrome_knob(img, x, y, r, ang, marker=(18, 18, 18)):
    spr, S_ = pc.knob_sprite(r, AS)
    spr = spr.astype(np.float32)
    n = spr.shape[0]; c = n / 2
    yy_, xx_ = np.mgrid[0:n, 0:n].astype(np.float32)
    rr = np.hypot(xx_ - c, yy_ - c) / U(r)
    body = (rr <= 1.0).astype(np.float32)
    v = spr[..., :3].mean(2, keepdims=True)
    chrome = np.clip(62 + 1.75 * v, 0, 255) * np.array([0.98, 0.99, 1.0])
    ang_ = np.arctan2(yy_ - c, xx_ - c)
    knurl = (0.5 + 0.5 * np.sign(np.sin(ang_ * 60))) * ((rr > 0.80) & (rr <= 1.0))
    chrome = chrome * (1 - 0.22 * knurl[..., None])
    spr[..., :3] = spr[..., :3] * (1 - body[..., None]) + chrome * body[..., None]
    m = Image.new('L', (n, n), 0); dm_ = ImageDraw.Draw(m)
    a_ = math.radians(ang - 90)
    dm_.line([c + U(r * 0.10) * math.cos(a_), c + U(r * 0.10) * math.sin(a_), c + U(r * 0.74) * math.cos(a_), c + U(r * 0.74) * math.sin(a_)], fill=255, width=max(2, int(U(r * 0.085))))
    mm = cv2.GaussianBlur(np.asarray(m, np.float32) / 255.0, (0, 0), 0.6)
    spr[..., :3] = spr[..., :3] * (1 - mm[..., None]) + np.array(marker) * mm[..., None]
    paste(img, spr, x, y)


def ticks(td, cx, cy, r0, r1, col, n=11):
    for i in range(n):
        a = math.radians(-135 + 270 * i / (n - 1) - 90)
        td.line([U(cx + r0 * math.cos(a)), U(cy + r0 * math.sin(a)), U(cx + r1 * math.cos(a)), U(cy + r1 * math.sin(a))], fill=col, width=int(U(1.4)))


def small_knob(img, x, y, v, r=24):
    """potard noir du panneau etendu + arc vert de la valeur (v 0..1)"""
    L2, t2 = rect_text_layer()
    box = [U(x - r - 7), U(y - r - 7), U(x + r + 7), U(y + r + 7)]
    t2.arc(box, 135, 405, fill=(64, 68, 68, 255), width=int(U(3)))
    t2.arc(box, 135, 135 + 270 * v, fill=(70, 200, 96, 255), width=int(U(3)))
    img[:] = comp(img, L2)
    spr, _s = pc.knob_sprite(r, AS)
    spr = spr.astype(np.float32); n = spr.shape[0]; c = n / 2
    m = Image.new('L', (n, n), 0); dm_ = ImageDraw.Draw(m); a_ = math.radians(-135 + 270 * v - 90)
    dm_.line([c + U(r * 0.40) * math.cos(a_), c + U(r * 0.40) * math.sin(a_), c + U(r * 0.86) * math.cos(a_), c + U(r * 0.86) * math.sin(a_)], fill=255, width=int(U(2.2)))
    mm = cv2.GaussianBlur(np.asarray(m, np.float32) / 255.0, (0, 0), 0.6)
    spr[..., :3] = spr[..., :3] * (1 - mm[..., None]) + 236 * mm[..., None]
    paste(img, spr, x, y)


L, td = rect_text_layer()
for x, *_ in LK: ticks(td, x, 619, 38, 44, WHITETXT)
for x, y, _ in RK: ticks(td, x, y, 38, 44, WHITETXT)
img = comp(img, L)
for (x, *_), ang in zip(LK, (-30, 0, 20)):
    chrome_knob(img, x, 619, 31, ang)
chrome_knob(img, MX, MY, MR, MANG[2])
for (x, y, _), ang in zip(RK, (0, 15, -30, -20, 40, 20)):
    chrome_knob(img, x, y, 30, ang)

# inverseur POWER + LED
fr = tp.aligned_frames(washer_r_units=14.0, size_units=80.0, AS=AS)
paste(img, fr[0], 1412, 622)
off, on, _ = pc.led_sprites(6.5, AS, colour='red')
paste(img, off, 200, 436)
paste(img, on, 1318, 622)
hal = np.exp(-(np.hypot(xx - U(1318), yy - U(622)) / U(18)) ** 2) * 0.35
img = img * (1 + hal[..., None] * np.array([0.9, 0.15, 0.1]))


# prises jack de face (ecrou + douille)
def jack(img, x, y, ring_col=None):
    nut = disk(x, y, 17)
    img = soft_shadow(img, nut, 2.5, 2.5, 3, 0.5)
    ang = np.arctan2(yy - U(y), xx - U(x))
    hexr = U(17) * np.cos(math.pi / 6) / np.cos(((ang + math.pi / 6) % (math.pi / 3)) - math.pi / 6)
    hexm = np.clip(hexr - np.hypot(xx - U(x), yy - U(y)), 0, 1)
    sh = 120 + 90 * np.clip(1 - ((xx - U(x - 17)) + (yy - U(y - 17))) / U(68), 0, 1)
    img = fill(img, hexm, sh[..., None] * np.ones(3))
    img = fill(img, ring(x, y, 8, 11.5), (190 - 70 * np.clip(((xx - U(x)) + (yy - U(y))) / U(24) + 0.5, 0, 1))[..., None] * np.ones(3))
    img = fill(img, disk(x, y, 7.5), np.array([8, 8, 8]))
    if ring_col is not None:
        img = fill(img, ring(x, y, 12.5, 15), np.array(ring_col))
    return img

img = jack(img, 198, 748)
img = jack(img, 353, 748)
img = jack(img, 478, 748, (70, 170, 230))
img = jack(img, 1200, 748, (230, 120, 50))
img = jack(img, 1296, 748)


def slide(img, x0, x1, y, pos_x):
    slot = rrect_mask(x0, y - 4.5, x1, y + 4.5, 4.5)
    img = fill(img, slot, np.array([12, 12, 12]))
    km = rrect_mask(pos_x - 11, y - 10, pos_x + 11, y + 10, 3)
    img = soft_shadow(img, km, 2.5, 2.5, 2.5, 0.55)
    g = 52 + 40 * np.clip(1 - ((xx - U(pos_x - 11)) + (yy - U(y - 10))) / U(42), 0, 1)
    ridges = 0.5 + 0.5 * np.sin((xx - U(pos_x)) / U(2.0) * math.pi)
    return fill(img, km, (g * (0.85 + 0.15 * ridges))[..., None] * np.ones(3))

img = slide(img, 590, 684, 744, 604)       # ECHO / NORMAL
img = slide(img, 764, 878, 744, 864)       # MIC / P.A. / INST
img = slide(img, 1030, 1094, 750, 1040)    # H / M / L

# ================================================================== barre du haut
pb = rrect_mask(20, 29, 420, 67, 19)
img = fill(img, rrect_mask(19, 28, 421, 68, 20), np.array([70, 74, 74]))
img = fill(img, pb, np.array([10, 11, 11]))
# IN / OUT
for i, x in enumerate((1086, 1146)):
    for k in range(2):
        mt = rrect_mask(x - 10 + k * 11, 18, x - 3 + k * 11, 62, 1.5)
        img = fill(img, mt, np.array([8, 8, 8]))
        lv = rrect_mask(x - 10 + k * 11, 44 - 6 * i, x - 3 + k * 11, 62, 1.5)
        img = fill(img, lv, np.array([70, 200, 96]))
def hbtn(img, x0, x1, y0, y1, col=(46, 48, 50)):
    return fill(img, rrect_mask(x0, y0, x1, y1, 6), np.array(col))
img = hbtn(img, 1190, 1300, 26, 70)
img = hbtn(img, 1314, 1418, 26, 70, (36, 38, 40))
img = hbtn(img, 1432, 1490, 26, 70, (36, 38, 40))
img = hbtn(img, 1500, 1580, 26, 70, (36, 38, 40))

# ================================================================== schema bloc
def bbox(img, td, x0, y0, x1, y1, lab, on=True, sub=None):
    img = fill(img, np.clip(rrect_mask(x0, y0, x1, y1, 3) - rrect_mask(x0 + 1.6, y0 + 1.6, x1 - 1.6, y1 - 1.6, 2), 0, 1), np.array([210, 214, 210]) if on else np.array([78, 80, 80]))
    img = fill(img, rrect_mask(x0 + 1.6, y0 + 1.6, x1 - 1.6, y1 - 1.6, 2), np.array([86, 90, 90]) if on else np.array([30, 31, 32]))
    text_on(td, (x0 + x1) / 2, (y0 + y1) / 2 + (-6 if sub else 0.5), lab, 10, 'Medium', (244, 246, 244, 255) if on else DIM)
    if sub: text_on(td, (x0 + x1) / 2, (y0 + y1) / 2 + 8, sub, 8, 'Medium', (200, 204, 200, 255) if on else DIM)
    return img

L, td = rect_text_layer()
lines = []
ON = (214, 218, 214, 255); OFF = (80, 82, 82, 255)
def wire(pts, on=True): td.line([(U(a), U(b)) for a, b in pts], fill=ON if on else OFF, width=int(U(1.6)), joint='curve')
# entrees
INS = [('MIC L', 1012, False), ('MIC R', 1040, False), ('P.A. L', 1120, False), ('P.A. R', 1148, False), ('INST L', 1330, True), ('INST R', 1358, False)]
# fils (dessines avant les cases)
wire([(126, 1012), (150, 1012)], False); wire([(126, 1040), (150, 1040)], False)
wire([(240, 1012), (270, 1012), (270, 1090), (300, 1090)], False)
wire([(126, 1330), (270, 1330), (270, 1090), (300, 1090)], True)      # INST L -> echo
wire([(270, 1150), (300, 1150)], True); wire([(270, 1090), (270, 1150)], True)
wire([(270, 1240), (300, 1240)], True); wire([(270, 1150), (270, 1240)], True)
wire([(370, 1090), (420, 1090), (420, 1120), (450, 1120)], True)
wire([(370, 1150), (420, 1150), (420, 1120)], True)
wire([(530, 1120), (580, 1120)], True)
wire([(370, 1240), (440, 1240), (440, 1282), (500, 1282)], True)
wire([(660, 1120), (690, 1120), (690, 1060), (710, 1060)], True)
wire([(580, 1282), (620, 1282), (620, 1120)], True)
wire([(126, 1358), (240, 1358), (240, 1300), (300, 1300)], False)
wire([(370, 1300), (420, 1300), (420, 1330), (500, 1330)], False)
wire([(580, 1330), (620, 1330), (620, 1180), (580, 1180)], False)
img = comp(img, L)
L, td = rect_text_layer()
for lab, y, on in INS:
    img = bbox(img, td, 70, y - 11, 126, y + 11, lab, on)
img = bbox(img, td, 150, 1000, 240, 1024, 'MIC AMP', False)
img = bbox(img, td, 150, 1028, 240, 1052, 'MIC AMP', False)
img = bbox(img, td, 300, 1076, 370, 1104, 'VU', True)
img = bbox(img, td, 300, 1134, 370, 1166, 'ECHO', True, 'TAPE')
img = bbox(img, td, 300, 1226, 370, 1254, 'REVERB', True)
img = bbox(img, td, 300, 1286, 370, 1314, 'REVERB', False)
img = bbox(img, td, 450, 1106, 530, 1134, 'TONE', True)
img = bbox(img, td, 450, 1166, 530, 1194, 'TONE', False)
text_on(td, 490, 1150, 'BASS/TREBLE', 7.5, 'Medium', DIM)
img = bbox(img, td, 580, 1104, 660, 1136, 'AMP', True, 'H/M/L')
img = bbox(img, td, 580, 1164, 660, 1196, 'AMP', False)
img = bbox(img, td, 500, 1268, 580, 1296, 'MIC AMP', True)
img = bbox(img, td, 500, 1316, 580, 1344, 'MIC AMP', False)
img = bbox(img, td, 710, 1046, 780, 1074, 'OUT L', True)
img = bbox(img, td, 710, 1166, 780, 1194, 'OUT R', False)

# barre du haut : textes
text_on(td, 220, 48.5, '43: Cosmic Strings', 21, 'Medium', (232, 236, 230, 255))
for sgn in (-1, 1):
    cx = 220 + sgn * (200 - 17)
    td.polygon([(U(cx + sgn * 5.5), U(48)), (U(cx - sgn * 4), U(42.5)), (U(cx - sgn * 4), U(53.5))], fill=(150, 150, 150, 255))
text_on(td, 1086, 80, 'IN', 9.5, 'Medium', G2); text_on(td, 1146, 80, 'OUT', 9.5, 'Medium', G2)
text_on(td, 1245, 48, 'BYPASS', 11, 'Medium', (230, 232, 230, 255), track=1)
text_on(td, 1350, 56, 'A', 12, 'Medium', (110, 210, 130, 255)); text_on(td, 1384, 56, 'B', 12, 'Medium', G2)
td.line([U(1340), U(38), U(1394), U(38)], fill=(110, 210, 130, 255), width=int(U(2)))
for sgn, cx in ((-1, 1450), (1, 1472)):
    td.arc([U(cx - 7), U(41), U(cx + 7), U(55)], 180, 360, fill=G2, width=int(U(1.8)))
    ax = cx - 7 if sgn < 0 else cx + 7
    td.polygon([(U(ax - 3.5), U(48)), (U(ax + 3.5), U(48)), (U(ax), U(53))], fill=G2)
for k in range(3):
    td.line([U(1526), U(38 + k * 9), U(1554), U(38 + k * 9)], fill=G2, width=int(U(2)))
img = comp(img, L)

# ================================================================== commandes du panneau etendu
def pill(img, td, x, y, w_, lab, on):
    img = fill(img, rrect_mask(x - w_ / 2, y - 13, x + w_ / 2, y + 13, 13), np.array([56, 160, 80]) if on else np.array([50, 53, 54]))
    text_on(td, x, y, lab, 9, 'Medium', (244, 246, 244, 255) if on else (200, 204, 200, 255))
    return img

L, td = rect_text_layer()
img = pill(img, td, 905, 1046, 110, 'TEMPO SYNC', False)
img = pill(img, td, 905, 1146, 80, 'SINGLE', False)
img = pill(img, td, 905, 1178, 80, 'DUAL', True)
img = pill(img, td, 1120, 1046, 80, 'NORMAL', False)
img = pill(img, td, 1080, 1078, 66, 'SLOW', True)
img = pill(img, td, 1160, 1078, 66, 'FAST', False)
img = comp(img, L)
# formes d'onde du LFO (6 boutons)
L, td = rect_text_layer()
for k in range(6):
    x = 1300 + k * 44
    img = fill(img, rrect_mask(x - 18, 1034, x + 18, 1058, 5), np.array([56, 160, 80]) if k == 0 else np.array([50, 53, 54]))
    pts = []
    for j in range(25):
        u = j / 24
        if k == 0: v = math.sin(u * 2 * math.pi)
        elif k == 1: v = 1 - 4 * abs(u - 0.5) if True else 0
        elif k == 2: v = 1 - 2 * u
        elif k == 3: v = 2 * u - 1
        elif k == 4: v = 1 if u < 0.5 else -1
        else: v = [0.2, -0.7, 0.9, -0.3, 0.5][min(4, int(u * 5))]
        pts.append((U(x - 12 + 24 * u), U(1046 - 6 * v)))
    td.line(pts, fill=(240, 244, 240, 255), width=int(U(1.5)))
img = comp(img, L)
small_knob(img, 1120, 1170, 0.5)
small_knob(img, 1310, 1170, 0.35)
small_knob(img, 1410, 1170, 0.0)
small_knob(img, 1510, 1170, 0.3)
for (x, _), v in zip(EK, (0.6, 0.4, 0.5, 0.0, 1.0, 0.5)):
    small_knob(img, x, 1330, v)

img = np.clip(img, 0, 255)
out = np.dstack([img, allm * 255]).astype(np.uint8)
Image.fromarray(out, 'RGBA').save(os.path.join(HERE, '..', 'design', 'preview_2x.png'))
Image.fromarray(out, 'RGBA').resize((W, H), Image.LANCZOS).save(os.path.join(HERE, '..', 'design', 'preview.png'))
print('ok')
