"""
Space Sonar 209 (NINE HANDS | DSP) - images du plugin, d'apres l'apercu valide (design/preview.png, v3).

Repere de l'interface : 1600 x 1400 unites (fenetre a 100 %). Fond rendu a AS = 2 px par unite (3200 x 2800).
Le fond ne contient que la matiere fixe ; l'editeur dessine par-dessus, en vectoriel et a la resolution de l'ecran :
textes (Poppins), logo NINE HANDS | DSP (SVG officiel), aiguille du VU, anneaux des prises actives, schema bloc,
boutons ; et pose les commandes animees (potards nh::Knob en calques, inverseur, LED, glissieres).

Sorties :
  assets/background.jpg                 fond 3200 x 2800
  assets/knob_chrome_*.png              potard chrome molete (calques 1024 px : shadow, crown, cap, marker, highlight)
  assets/knob_black_*.png               potard noir du panneau etendu (shadow, cap, marker, highlight)
  assets/toggle_strip.png               inverseur POWER : haut / milieu / bas empiles (photos du kit)
  assets/led_red_off.png, led_red_on.png
  assets/slide_knob.png                 bouton des glissieres
  Layout.h                              textes (position, taille, graisse, couleur) lus par l'editeur
Lumiere : en haut a gauche a 45 deg, ombres vers le bas a droite.
"""
import os, sys, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import photo_controls as pc
import toggle_photo as tp

AS = 2
W, H = 1600, 1400
PW, PH = W * AS, H * AS
rng = np.random.default_rng(7)
yy, xx = np.mgrid[0:PH, 0:PW].astype(np.float32)
U = lambda v: v * AS
OUT = os.path.join(ROOT, 'assets')


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


def soft_shadow(img, mask, dx, dy, blur, amt):
    m = cv2.GaussianBlur(np.roll(np.roll(mask, int(U(dy)), 0), int(U(dx)), 1), (0, 0), U(blur))
    return img * (1 - amt * m[..., None])


def fill(img, m, col):
    col = np.asarray(col, np.float32)
    if col.ndim == 1: col = col[None, None, :]
    return img * (1 - m[..., None]) + col * m[..., None]


def brushed(base, amp=6):
    b = cv2.GaussianBlur(rng.standard_normal((PH, PW)).astype(np.float32), (0, 0), sigmaX=U(30), sigmaY=0.6)
    b /= b.std()
    return (base + amp * b + noise(0.4, 1.2))[..., None] * np.array([0.985, 0.995, 1.0])


def disk(cx, cy, r):
    return np.clip(U(r) - np.hypot(xx - U(cx), yy - U(cy)), 0, 1)


def ring(cx, cy, r0, r1):
    d = np.hypot(xx - U(cx), yy - U(cy))
    return np.clip(U(r1) - d, 0, 1) * np.clip(d - U(r0), 0, 1)


def line_layer():
    t = Image.new('RGBA', (PW, PH), (0, 0, 0, 0))
    return t, ImageDraw.Draw(t)


def comp(img, layer):
    ta = np.asarray(layer, np.float32)
    a = ta[..., 3:4] / 255.0
    return img * (1 - a) + ta[..., :3] * a


GREEN = np.array([52, 132, 58], np.float32)
WHITE = (236, 238, 234, 255)
BLACK = (20, 22, 22, 255)
G2 = (196, 202, 198, 255)

# ====================================================================== textes (dessines par l'editeur)
TEXTS = []          # (x, y, texte, taille, graisse, argb, interlettrage, ancre)
def T(x, y, s, size, weight='Medium', col=WHITE, track=0.0, anchor='mm', rot=0.0):
    TEXTS.append((x, y, s, size, weight, col, track, anchor, rot))


_FONTS = {}
def advance(ch, size, weight='Medium'):
    if weight not in _FONTS:
        _FONTS[weight] = ImageFont.truetype(os.path.join(ROOT, 'fonts', 'Poppins-%s.ttf' % weight), 400)
    return _FONTS[weight].getlength(ch) * size / 400.0


def arc_text(cx, cy, r, center_deg, text, size, weight='Medium', col=WHITE, track=0.0):
    """texte courbe, sommets des lettres vers l'exterieur, lu dans le sens horaire (0 deg = en haut)"""
    adv = [advance(c, size, weight) + track for c in text]
    total = sum(adv) - track
    a = math.radians(center_deg) - total / r / 2.0
    for c, w_ in zip(text, adv):
        mid = a + (w_ - track) / 2.0 / r
        if c != ' ':
            T(cx + r * math.sin(mid), cy - r * math.cos(mid), c, size, weight, col, 0.0, 'mm', math.degrees(mid))
        a += w_ / r

# ====================================================================== fond
img = np.zeros((PH, PW, 3), np.float32)
allm = rrect_mask(0, 0, W, H, 18)
img = fill(img, allm, (30 + noise(0.8, 1.0))[..., None] * np.array([0.97, 0.99, 1.02]))
hdr = rrect_mask(0, 0, W, 96, 18) * (yy < U(96))
img = fill(img, hdr, np.array([22, 23, 24]))
img *= (1 - 0.5 * np.clip(1 - np.abs(yy - U(96)) / U(1.5), 0, 1))[..., None]
stage = (yy >= U(96)) * (yy < U(912)) * allm
img = fill(img, stage, (40 + 10 * np.clip(1 - (yy - U(96)) / U(800), 0, 1) + noise(0.8, 1.0))[..., None] * np.ones(3))

# appareil (tolex)
TX0, TY0, TX1, TY1 = 40, 140, 1560, 900
tolex_m = rrect_mask(TX0, TY0, TX1, TY1, 40)
img = soft_shadow(img, tolex_m, 6, 8, 12, 0.6)
tol = 26 + noise(2.0, 5.0) + noise(0.7, 3.0) + noise(0.0, 1.5)
img = fill(img, tolex_m, tol[..., None] * np.array([1.0, 1.0, 1.03]))
rim = np.clip(tolex_m - rrect_mask(TX0 + 6, TY0 + 6, TX1 - 6, TY1 - 6, 34), 0, 1)
img += (rim * 14)[..., None]
for (x0, x1) in ((20, 46), (1554, 1580)):
    for yc in (300, 700):
        hm = rrect_mask(x0, yc - 34, x1, yc + 34, 8)
        img = soft_shadow(img, hm, 3, 3, 4, 0.5)
        img = fill(img, hm, (60 + 50 * np.clip(1 - ((xx - U(x0)) + (yy - U(yc - 34))) / U(90), 0, 1))[..., None] * np.ones(3))
foot = rrect_mask(560, 900, 1040, 914, 4)
img = fill(img, foot, np.array([18, 18, 18]))

FX0, FX1, FY0, FY1 = 116, 1484, 280, 790
fr_m = rrect_mask(FX0 - 9, FY0 - 9, FX1 + 9, FY1 + 9, 8)
img = soft_shadow(img, fr_m, 5, 5, 7, 0.6)
img = fill(img, fr_m, (140 + 70 * np.clip(1 - (yy - U(FY0)) / U(FY1 - FY0), 0, 1))[..., None] * np.ones(3))
img = fill(img, rrect_mask(FX0, FY0, FX1, 372, 3), brushed(184))
img = fill(img, rrect_mask(FX0, 374, FX1, 684, 2), (15 + noise(0.6, 1.2))[..., None] * np.ones(3))
img = fill(img, rrect_mask(578, 382, 800, 676, 6), GREEN * (1 + noise(0.6, 0.02))[..., None])
img = fill(img, rrect_mask(808, 382, 1232, 676, 6), GREEN * (1 + noise(0.6, 0.02))[..., None])
img = fill(img, rrect_mask(820, 394, 1220, 664, 4), (14 + noise(0.6, 1.0))[..., None] * np.ones(3))
img = fill(img, rrect_mask(FX0, 686, FX1, FY1, 3), brushed(156, 4))
BOX = [(128, 268, 'MIC'), (276, 430, 'FROM P.A.'), (438, 712, 'INSTRUMENT'), (1000, 1244, 'OUTPUT'), (1252, 1472, 'ECHO CANCEL')]
for x0, x1, _ in BOX:
    bx = np.clip(rrect_mask(x0, 696, x1, 780, 5) - rrect_mask(x0 + 2, 698, x1 - 2, 778, 4), 0, 1)
    img = fill(img, bx, np.array([40, 42, 42]))
for y in (373, 685):
    ln = np.clip(1 - np.abs(yy - U(y)) / U(1.6), 0, 1) * (xx > U(FX0)) * (xx < U(FX1))
    img *= (1 - 0.8 * ln)[..., None]

# separation + panneau etendu + ecran du schema
img = fill(img, (yy >= U(912)) * (yy < U(946)) * allm, np.array([24, 25, 26]))
img = fill(img, rrect_mask(700, 918, 900, 940, 6), np.array([46, 48, 50]))
img = fill(img, (yy >= U(946)) * allm, (33 + noise(0.8, 1.0))[..., None] * np.array([0.97, 0.99, 1.02]))
SX0, SY0, SX1, SY1 = 50, 990, 800, 1380
scr = rrect_mask(SX0, SY0, SX1, SY1, 8)
img = fill(img, scr, (16 + noise(1.0, 0.8))[..., None] * np.ones(3))
inn = cv2.GaussianBlur(1 - scr, (0, 0), U(4))
img *= (1 - 0.5 * scr * np.clip(np.roll(np.roll(inn, int(U(2)), 0), int(U(2)), 1), 0, 1))[..., None]

# eclairage 45 deg (appareil seulement : facade, tolex)
t = ((xx / PW) + (yy / U(912))) / 2.0
img = img * (1 + tolex_m * (0.08 - 0.20 * t))[..., None]

# VU : lunette, face noire retroeclairee, graduations (aiguille et lueur : editeur)
vx0, vy0, vx1, vy1 = 252, 394, 440, 494
bez = rrect_mask(vx0 - 7, vy0 - 7, vx1 + 7, vy1 + 7, 4)
img = soft_shadow(img, bez, 3, 3, 4, 0.5)
img = fill(img, bez, (110 + 90 * np.clip(1 - ((xx - U(vx0)) + (yy - U(vy0))) / U(230), 0, 1))[..., None] * np.ones(3))
face = rrect_mask(vx0, vy0, vx1, vy1, 3)
img = fill(img, face, np.array([10, 13, 11]))
L, td = line_layer()
vc = ((vx0 + vx1) / 2, vy1 + 26); vr = 96
GLOWG = (160, 238, 176, 255); RED = (236, 76, 60, 255)
for i in range(11):
    a = math.radians(-46 + 92 * i / 10 - 90)
    r0 = vr - 7 - (4 if i % 2 == 0 else 0)
    td.line([U(vc[0] + r0 * math.cos(a)), U(vc[1] + r0 * math.sin(a)), U(vc[0] + vr * math.cos(a)), U(vc[1] + vr * math.sin(a))], fill=RED if i >= 7 else GLOWG, width=int(U(1.5)))
td.arc([U(vc[0] - vr), U(vc[1] - vr), U(vc[0] + vr), U(vc[1] + vr)], -136, -81.4, fill=GLOWG, width=int(U(1.4)))
td.arc([U(vc[0] - vr), U(vc[1] - vr), U(vc[0] + vr), U(vc[1] + vr)], -81.4, -44, fill=RED, width=int(U(3)))
img = comp(img, L)
T(vc[0], vy1 - 30, 'VU', 13, 'Medium', GLOWG)

# ====================================================================== serigraphies (textes) + graduations
T(150, 328, 'SPACE SONAR', 36, 'Bold', BLACK, 2.5, 'lm')
T(482, 330, '209', 36, 'Light', BLACK, 1.5, 'lm')
T(200, 466, 'PEAK', 10, 'Medium', WHITE, 1.2); T(200, 479, 'LEVEL', 10, 'Medium', WHITE, 1.2)
LK = [(190, 'MIC', 'VOLUME'), (345, 'FROM P.A.', 'VOLUME'), (500, 'INSTRUMENT', 'VOLUME')]
for x, a1, a2 in LK:
    T(x, 540, a1, 11, 'Medium', WHITE, 1.1); T(x, 554, a2, 11, 'Medium', WHITE, 1.1)
T(689, 402, 'MODE SELECTOR', 12.5, 'Bold', WHITE, 1.6)
MX, MY, MR = 689, 528, 44
MANG = [-150 + 30 * i for i in range(11)] + [180]
# bande de graduation blanche (1 a 11), separations noires, ouverte en bas sur REVERB ONLY
RB0, RB1 = 53.0, 70.0
dd = np.hypot(xx - U(MX), yy - U(MY))
th = np.degrees(np.arctan2(xx - U(MX), -(yy - U(MY))))          # 0 = en haut, sens horaire
band = np.clip(dd - U(RB0), 0, 1) * np.clip(U(RB1) - dd, 0, 1) * np.clip((165.0 - np.abs(th)) * U(RB1) * math.pi / 180.0, 0, 1)
img = soft_shadow(img, band, 1.2, 1.2, 1.5, 0.45)
shade = 226 + 16 * np.clip(1 - ((xx - U(MX - RB1)) + (yy - U(MY - RB1))) / U(4 * RB1), 0, 1)
img = fill(img, band, shade[..., None] * np.array([1.0, 1.0, 0.975]))
L, td = line_layer()
for kk in range(12):
    a = math.radians(-165 + 30 * kk)
    td.line([U(MX + RB0 * math.sin(a)), U(MY - RB0 * math.cos(a)), U(MX + RB1 * math.sin(a)), U(MY - RB1 * math.cos(a))], fill=(18, 20, 18, 255), width=int(U(1.6)))
a = math.radians(180)
td.line([U(MX + (RB0 + 2) * math.sin(a)), U(MY - (RB0 + 2) * math.cos(a)), U(MX + (RB1 - 1) * math.sin(a)), U(MY - (RB1 - 1) * math.cos(a))], fill=WHITE, width=int(U(2.2)))
img = comp(img, L)
for i in range(11):
    a = math.radians(MANG[i])
    T(MX + 61.8 * math.sin(a), MY - 61.8 * math.cos(a), str(i + 1), 10.5, 'Bold', (16, 18, 16, 255), 0.0, 'mm', MANG[i])
arc_text(MX, MY, 79.0, -105.0, 'REPEAT', 9.0, 'Medium', WHITE, 1.0)
arc_text(MX, MY, 79.0, 72.0, 'REVERB ECHO', 9.0, 'Medium', WHITE, 1.0)
T(MX, MY + 84, 'REVERB', 10, 'Medium', WHITE, 1.2); T(MX, MY + 97, 'ONLY', 10, 'Medium', WHITE, 1.2)
L, td = line_layer()
RK = [(882, 478, 'BASS'), (1020, 478, 'TREBLE'), (1158, 478, 'REVERB VOLUME'), (882, 612, 'REPEAT RATE'), (1020, 612, 'INTENSITY'), (1158, 612, 'ECHO VOLUME')]
for x, y, lab in RK:
    T(x, y - 60, lab, 11, 'Medium', WHITE, 1.1)
T(1362, 548, 'POWER', 11, 'Medium', WHITE, 1.2)
T(1412, 584, 'ON', 9.5, 'Medium', WHITE, 1); T(1412, 660, 'OFF', 9.5, 'Medium', WHITE, 1)


def ticks(cx, cy, r0, r1, col, n=11):
    for i in range(n):
        a = math.radians(-135 + 270 * i / (n - 1) - 90)
        td.line([U(cx + r0 * math.cos(a)), U(cy + r0 * math.sin(a)), U(cx + r1 * math.cos(a)), U(cy + r1 * math.sin(a))], fill=col, width=int(U(1.4)))


for x, *_ in LK: ticks(x, 619, 38, 44, WHITE)
for x, y, _ in RK: ticks(x, y, 38, 44, WHITE)
img = comp(img, L)

# bandeau des prises
for x0, x1, lab in BOX:
    if lab in ('MIC', 'FROM P.A.'): T((x0 + x1) / 2, 712, lab, 10.5, 'Medium', BLACK, 1.1)
    else: T(x0 + 14, 712, lab, 10.5, 'Medium', BLACK, 1.1, 'lm')
T(600, 764, 'ECHO', 8.5, 'Medium', BLACK, 0.8); T(672, 764, 'NORMAL', 8.5, 'Medium', BLACK, 0.8)
T(774, 712, 'MIC', 10.5, 'Medium', BLACK, 0.8); T(821, 712, 'P.A.', 10.5, 'Medium', BLACK, 0.8); T(868, 712, 'INST', 10.5, 'Medium', BLACK, 0.8)
for i, s_ in enumerate('HML'):
    T(1040 + i * 26, 730, s_, 9.5, 'Medium', BLACK)
T(1380, 764, 'ON   ·   OFF', 8.5, 'Medium', BLACK, 0.8)


def jack(img, x, y):
    nut = disk(x, y, 17)
    img = soft_shadow(img, nut, 2.5, 2.5, 3, 0.5)
    ang = np.arctan2(yy - U(y), xx - U(x))
    hexr = U(17) * np.cos(math.pi / 6) / np.cos(((ang + math.pi / 6) % (math.pi / 3)) - math.pi / 6)
    hexm = np.clip(hexr - np.hypot(xx - U(x), yy - U(y)), 0, 1)
    sh = 120 + 90 * np.clip(1 - ((xx - U(x - 17)) + (yy - U(y - 17))) / U(68), 0, 1)
    img = fill(img, hexm, sh[..., None] * np.ones(3))
    img = fill(img, ring(x, y, 8, 11.5), (190 - 70 * np.clip(((xx - U(x)) + (yy - U(y))) / U(24) + 0.5, 0, 1))[..., None] * np.ones(3))
    return fill(img, disk(x, y, 7.5), np.array([8, 8, 8]))


JACKS = [(198, 748), (353, 748), (478, 748), (1200, 748), (1296, 748)]
for (x, y) in JACKS:
    img = jack(img, x, y)
SLIDES = [(590, 684, 744), (764, 878, 744), (1028, 1104, 750)]
for (x0, x1, y) in SLIDES:
    img = fill(img, rrect_mask(x0, y - 4.5, x1, y + 4.5, 4.5), np.array([12, 12, 12]))

# panneau etendu : titres
T(425, 975, 'BLOCK DIAGRAM', 11, 'Medium', G2, 1.3)
T(905, 1110, 'STEREO MODE', 10.5, 'Medium', G2, 1.0)
T(1120, 1010, 'RATE RANGE', 10.5, 'Medium', G2, 1.0)
T(1120, 1210, 'SPEED', 10, 'Medium', G2, 1.0)
T(1410, 1010, 'TAPE SPEED LFO', 10.5, 'Medium', G2, 1.0)
for x, lab in ((1310, 'RATE'), (1410, 'WAVEFORM'), (1510, 'DEPTH')):
    T(x, 1105, lab, 10, 'Medium', G2, 1.0)
EK = [(905, 'CONDITION'), (1010, 'W/F'), (1120, 'MOTOR TORQUE'), (1265, 'LOW CUT'), (1385, 'HIGH CUT'), (1510, 'MASTER')]
for x, lab in EK:
    T(x, 1268, lab, 10, 'Medium', G2, 1.0)

img = np.clip(img, 0, 255)
os.makedirs(OUT, exist_ok=True)
Image.fromarray(img.astype(np.uint8), 'RGB').save(os.path.join(OUT, 'background.jpg'), quality=93, subsampling=0)
print('fond ok')

# ====================================================================== potards (calques 1024 px)
N = 1024
PAD = 1.30
RPX = N / 2 / PAD
ky, kx = np.mgrid[0:N, 0:N].astype(np.float32)
kc = N / 2
kr = np.hypot(kx - kc, ky - kc) / RPX
kth = np.arctan2(ky - kc, kx - kc)


def kit_knob_full():
    rgb, a = pc._place(pc.knob_clean(), pc.KNOB_C, RPX / pc.KNOB_RPX, N)
    return rgb.astype(np.float32), a.astype(np.float32)


def radial_avg(v, a):
    """profil radial (moyenne sur l'angle) : la partie de l'image invariante par rotation"""
    bins = np.clip((kr * 400).astype(int), 0, 799)
    s = np.bincount(bins.ravel(), (v * a).ravel(), 800); w = np.bincount(bins.ravel(), a.ravel(), 800)
    prof = s / np.maximum(w, 1e-6)
    return prof[bins]


def save_layer(name, rgb, a):
    out = np.dstack([np.clip(rgb, 0, 255), np.clip(a, 0, 1) * 255]).astype(np.uint8)
    Image.fromarray(out, 'RGBA').save(os.path.join(OUT, name + '.png'), optimize=True)


def highlight_from(target, base, a):
    """calque fixe qui, pose sur la base, redonne la cible : blanc ou noir avec l'alpha juste"""
    up = target > base
    al = np.where(up, (target - base) / np.maximum(255 - base, 1), (base - target) / np.maximum(base, 1))
    col = np.where(up, 255.0, 0.0)
    return np.dstack([col] * 3), np.clip(al, 0, 1) * a


def shadow_layer(a):
    s = 2 * RPX
    cast = cv2.GaussianBlur(a, (0, 0), 0.07 * s) * 0.62
    contact = cv2.GaussianBlur(a, (0, 0), 0.018 * s) * 0.5
    return np.zeros((N, N, 3)), np.clip(cast + contact, 0, 0.88)


rgb, a = kit_knob_full()
body = (kr <= 1.0).astype(np.float32) * a
v = rgb.mean(2)
# --- chrome molete (facade)
chrome = np.clip(62 + 1.75 * v, 0, 255)
knurl_zone = ((kr > 0.80) & (kr <= 1.0)).astype(np.float32)
knurl = 0.5 + 0.5 * np.sign(np.sin(kth * 60))
base = radial_avg(chrome, body)
crown_rgb = base * (1 - 0.22 * knurl * knurl_zone)
save_layer('knob_chrome_shadow', *shadow_layer(a))
save_layer('knob_chrome_crown', np.dstack([crown_rgb] * 3) * np.array([0.98, 0.99, 1.0]), a * knurl_zone)
save_layer('knob_chrome_cap', np.dstack([base] * 3) * np.array([0.98, 0.99, 1.0]), a * (kr <= 0.80))
m = Image.new('L', (N, N), 0); dm = ImageDraw.Draw(m)
dm.line([kc, kc - RPX * 0.10, kc, kc - RPX * 0.74], fill=255, width=int(RPX * 0.085))
mm = cv2.GaussianBlur(np.asarray(m, np.float32) / 255.0, (0, 0), 1.0)
save_layer('knob_chrome_marker', np.full((N, N, 3), 18.0), mm)
hl_rgb, hl_a = highlight_from(chrome, base, body)
save_layer('knob_chrome_highlight', hl_rgb, hl_a)
# --- noir (panneau etendu)
baseb = radial_avg(v, body)
save_layer('knob_black_shadow', *shadow_layer(a))
save_layer('knob_black_cap', np.dstack([baseb] * 3), a)
m = Image.new('L', (N, N), 0); dm = ImageDraw.Draw(m)
dm.line([kc, kc - RPX * 0.40, kc, kc - RPX * 0.86], fill=255, width=int(RPX * 0.085))
mm = cv2.GaussianBlur(np.asarray(m, np.float32) / 255.0, (0, 0), 1.0)
save_layer('knob_black_marker', np.full((N, N, 3), 236.0), mm)
hl_rgb, hl_a = highlight_from(v, baseb, body)
save_layer('knob_black_highlight', hl_rgb, hl_a)
# --- selecteur de mode : chapeau chrome (r <= 0,70) sur jupe noire molete
RC = 0.70
mapx = (kc + (kx - kc) * (1.0 / RC)).astype(np.float32); mapy = (kc + (ky - kc) * (1.0 / RC)).astype(np.float32)
chrome_in = cv2.remap(chrome.astype(np.float32), mapx, mapy, cv2.INTER_LINEAR)
base_in = cv2.remap(base.astype(np.float32), mapx, mapy, cv2.INTER_LINEAR)
capz = (kr <= RC).astype(np.float32)
skirtz = ((kr > RC) & (kr <= 1.0)).astype(np.float32)
knurl2 = 0.5 + 0.5 * np.sign(np.sin(kth * 72))
skirt = baseb * (0.80 + 0.30 * knurl2)                       # moletage de la jupe noire
target = np.where(kr <= RC, chrome_in, v)
basem = np.where(kr <= RC, base_in, skirt)
lip = np.clip(1 - np.abs(kr - RC) / 0.025, 0, 1)            # liseré sombre entre chapeau et jupe
save_layer('knob_mode_crown', np.dstack([skirt * (1 - 0.6 * lip)] * 3), a * skirtz)
save_layer('knob_mode_cap', np.dstack([base_in * (1 - 0.5 * lip)] * 3) * np.array([0.98, 0.99, 1.0]), a * capz)
m = Image.new('L', (N, N), 0); dm = ImageDraw.Draw(m)
dm.line([kc, kc - RPX * 0.06, kc, kc - RPX * 0.64], fill=255, width=int(RPX * 0.07))
mk = cv2.GaussianBlur(np.asarray(m, np.float32) / 255.0, (0, 0), 1.0)
m = Image.new('L', (N, N), 0); dm = ImageDraw.Draw(m)
dm.line([kc, kc - RPX * 0.76, kc, kc - RPX * 0.97], fill=255, width=int(RPX * 0.075))
mw = cv2.GaussianBlur(np.asarray(m, np.float32) / 255.0, (0, 0), 1.0)
mrgb = np.dstack([18 * mk + 238 * mw] * 3) / np.maximum(np.dstack([mk + mw] * 3), 1e-4)
save_layer('knob_mode_marker', mrgb, np.clip(mk + mw, 0, 1))
hl_rgb, hl_a = highlight_from(np.where(kr <= RC, chrome_in, skirt * (v / np.maximum(baseb, 1))), basem, body)
save_layer('knob_mode_highlight', hl_rgb, hl_a)
print('potards ok')

# ====================================================================== inverseur, LED, glissiere (4 px par unite)
AS4 = 4
fr = tp.aligned_frames(washer_r_units=14.0, size_units=80.0, AS=AS4)
strip = np.concatenate([f.astype(np.float32) for f in fr], 0)
Image.fromarray(np.clip(strip, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'toggle_strip.png'), optimize=True)
off, on, side = pc.led_sprites(6.5, AS4, colour='red')
Image.fromarray(np.clip(off, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'led_red_off.png'))
Image.fromarray(np.clip(on, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'led_red_on.png'))
print('inverseur + LED ok, LED', side, 'unites')

# bouton de glissiere : 22 x 20 unites, sprite 40 x 40 unites
n = 40 * AS4
sy, sx = np.mgrid[0:n, 0:n].astype(np.float32)
def rr_(x0, y0, x1, y1, r):
    x0, y0, x1, y1, r = [v * AS4 for v in (x0, y0, x1, y1, r)]
    cx = np.clip(sx, x0 + r, x1 - r); cy = np.clip(sy, y0 + r, y1 - r)
    return np.clip(0.5 - (np.hypot(sx - cx, sy - cy) - r), 0, 1)
km = rr_(9, 10, 31, 30, 3)
sh = cv2.GaussianBlur(np.roll(np.roll(km, int(2.5 * AS4), 0), int(2.5 * AS4), 1), (0, 0), 2.5 * AS4) * 0.55
g = 52 + 40 * np.clip(1 - ((sx - 9 * AS4) + (sy - 10 * AS4)) / (42 * AS4), 0, 1)
ridges = 0.5 + 0.5 * np.sin((sx - 20 * AS4) / (2.0 * AS4) * math.pi)
colk = g * (0.85 + 0.15 * ridges)
alpha = np.clip(km + sh * (1 - km), 0, 1)
colv = np.where(alpha > 1e-4, colk * km / np.maximum(alpha, 1e-4), 0)
Image.fromarray(np.dstack([np.clip(np.dstack([colv] * 3), 0, 255), alpha * 255]).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'slide_knob.png'))
print('glissiere ok')

# ====================================================================== Layout.h
def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('·', '\\xc2\\xb7') + '"'

W_ = {'Regular': 0, 'Medium': 1, 'Bold': 2, 'Light': 3}
lines = ['#pragma once', '// genere par tools/make_art.py : ne pas modifier a la main', '#include <cstdint>', '',
         'namespace ssl', '{', '    constexpr float kW = %d.0f, kH = %d.0f;   // repere de l\'interface (100 %%)' % (W, H),
         '    struct Txt { float x, y; const char* s; float size; int weight; uint32_t argb; float track; bool left; float rot; };',
         '    inline const Txt* texts (int& n)', '    {', '        static const Txt t[] = {']
for (x, y, s, size, w, col, tr, anc, rot) in TEXTS:
    argb = (col[3] << 24) | (col[0] << 16) | (col[1] << 8) | col[2]
    lines.append('            { %.2ff, %.2ff, %s, %.2ff, %d, 0x%08xu, %.2ff, %s, %.2ff },' % (x, y, cstr(s), size, W_[w], argb, tr, 'true' if anc == 'lm' else 'false', rot))
lines += ['        };', '        n = (int) (sizeof (t) / sizeof (t[0]));', '        return t;', '    }', '}', '']
open(os.path.join(ROOT, 'Layout.h'), 'w').write('\n'.join(lines))
print('Layout.h ok,', len(TEXTS), 'textes')
