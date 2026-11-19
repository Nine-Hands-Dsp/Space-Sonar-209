"""
Space Sonar 209 (NINE HANDS | DSP) - images du plugin, d'apres l'apercu valide (design/preview.png, v3).

Repere de l'interface : 1600 x 1400 unites (fenetre a 100 %). Fond rendu a AS = 3 px par unite (4800 x 4200).
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

AS = 3                                   # 3 px par unite : net jusqu a 150 % sur ecran Retina / 4K a 200 %
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
# facade agrandie (demande de l'utilisateur, v1.0.2) : coordonnees d'origine -> X = FCX + (x - FCX) * FS
FS, FCX, FCY = 1.12, 800.0, 780.0
FAC = False
def fxy(x, y): return (FCX + (x - FCX) * FS, FCY + (y - FCY) * FS)


def T(x, y, s, size, weight='Medium', col=WHITE, track=0.0, anchor='mm', rot=0.0):
    if FAC:
        x, y = fxy(x, y); size *= FS; track *= FS
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
TX0, TY0, TX1, TY1 = 8, 118, 1592, 880
tolex_m = rrect_mask(TX0, TY0, TX1, TY1, 34)
img = soft_shadow(img, tolex_m, 6, 8, 12, 0.6)
tol = 26 + noise(2.0, 5.0) + noise(0.7, 3.0) + noise(0.0, 1.5)
img = fill(img, tolex_m, tol[..., None] * np.array([1.0, 1.0, 1.03]))
rim = np.clip(tolex_m - rrect_mask(TX0 + 6, TY0 + 6, TX1 - 6, TY1 - 6, 28), 0, 1)
img += (rim * 14)[..., None]
foot = rrect_mask(560, 880, 1040, 894, 4)
img = fill(img, foot, np.array([18, 18, 18]))

# ---- facade : dessinee dans ses coordonnees d'origine, evaluee directement aux pixels finaux (aucun agrandissement d'image)
xx_fin, yy_fin = xx, yy
xx = (FCX * AS + (xx_fin - FCX * AS) / FS).astype(np.float32)
yy = (FCY * AS + (yy_fin - FCY * AS) / FS).astype(np.float32)
FAC = True
_line_layer0 = line_layer
class _FacDraw:
    def __init__(self, d): self.d = d
    def _p(self, v, axis): return (FCX if axis == 0 else FCY) * AS + (v - (FCX if axis == 0 else FCY) * AS) * FS
    def line(self, pts, fill=None, width=1, **kw):
        q = [self._p(v, i % 2) for i, v in enumerate(pts)]
        self.d.line(q, fill=fill, width=max(1, int(round(width * FS))), **kw)
def line_layer():
    t, d = _line_layer0()
    return t, _FacDraw(d)
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

# VU : photo fournie par l'utilisateur (assets/photos/vu_meter_src.png) ; aiguille dessinee par l'editeur
PHOTOS = os.path.join(OUT, 'photos')
VU_SRC_BOX = (19, 303, 1236, 950)                    # lunette dans la photo (px)
VU_BOX = (239.0, 387.0, 453.0, 501.0)                # lunette sur la facade (unites d'origine)


def paste_photo(img, rgba, box_units, shadow=(3.0, 3.0, 4.0, 0.5)):
    """pose une photo RGBA (reduite, jamais agrandie) dans une boite en unites de facade, avec ombre portee a 45 deg"""
    (X0, Y0), (X1, Y1) = fxy(box_units[0], box_units[1]), fxy(box_units[2], box_units[3])
    w, h = int(round(U(X1 - X0))), int(round(U(Y1 - Y0)))
    assert w <= rgba.width and h <= rgba.height, 'agrandissement interdit'
    ph = np.asarray(rgba.resize((w, h), Image.LANCZOS), np.float32)
    x0, y0 = int(round(U(X0))), int(round(U(Y0)))
    al = ph[..., 3:4] / 255.0
    if shadow:
        m = np.zeros((PH, PW), np.float32); m[y0:y0 + h, x0:x0 + w] = al[..., 0]
        img = soft_shadow(img, m, shadow[0] * FS, shadow[1] * FS, shadow[2] * FS, shadow[3])
    img[y0:y0 + h, x0:x0 + w] = img[y0:y0 + h, x0:x0 + w] * (1 - al) + ph[..., :3] * al
    return img


vu_src = Image.open(os.path.join(PHOTOS, 'vu_meter_src.png')).convert('RGBA').crop(VU_SRC_BOX)
img = paste_photo(img, vu_src, VU_BOX)

# ====================================================================== serigraphies (textes) + graduations
T(150, 328, 'SPACE SONAR', 36, 'Bold', BLACK, 2.5, 'lm')
T(482, 330, '209', 36, 'Light', BLACK, 1.5, 'lm')
T(200, 466, 'PEAK', 10, 'Medium', WHITE, 1.2); T(200, 479, 'LEVEL', 10, 'Medium', WHITE, 1.2)
LK = [(190, 'MIC', 'VOLUME'), (345, 'FROM P.A.', 'VOLUME'), (500, 'INSTRUMENT', 'VOLUME')]
for x, a1, a2 in LK:
    T(x, 540, a1, 11, 'Medium', WHITE, 1.1); T(x, 554, a2, 11, 'Medium', WHITE, 1.1)
T(689, 400, 'MODE SELECTOR', 13, 'Bold', WHITE, 1.8)
MX, MY, MR = 689, 530, 56                    # potard (bague exterieure) : photo fournie, comme les autres potards
MANG = [-150 + 30 * i for i in range(11)] + [180]
PALE = np.array([214, 222, 226], np.float32)
# --- paves gris clair entre les numeros (anneau r 75..86)
RB0, RB1, RN = 74.5, 86.5, 80.5
dd = np.hypot(xx - U(MX), yy - U(MY))
th = np.degrees(np.arctan2(xx - U(MX), -(yy - U(MY))))          # 0 = en haut, sens horaire
ringm = np.clip(dd - U(RB0), 0, 1) * np.clip(U(RB1) - dd, 0, 1)
blocks = np.zeros_like(dd)
for i in range(10):                                              # entre 1-2, 2-3, ... 10-11
    c0 = MANG[i] + 15.0
    half = 10.5 if i < 8 else 9.0                                # numeros 10 et 11 plus larges
    dth = np.abs(((th - c0 + 180.0) % 360.0) - 180.0)
    blocks = np.maximum(blocks, np.clip((half - dth) * U(RN) * math.pi / 180.0, 0, 1))
blocks *= ringm
img = soft_shadow(img, blocks, 0.8, 0.8, 1.0, 0.35)
shade = 1.0 + 0.06 * np.clip(1 - ((xx - U(MX - RB1)) + (yy - U(MY - RB1))) / U(4 * RB1), 0, 1)
img = fill(img, blocks, PALE * shade[..., None])
L, td = line_layer()
WL = (238, 240, 236, 255)
def pol(r, a_deg): a_ = math.radians(a_deg); return (U(MX + r * math.sin(a_)), U(MY - r * math.cos(a_)))
def seg(r0, r1, a_deg, w):
    p0, p1 = pol(r0, a_deg), pol(r1, a_deg); td.line([p0[0], p0[1], p1[0], p1[1]], fill=WL, width=int(U(w)))
def arcl(r, a0, a1, w):
    n_ = max(2, int(abs(a1 - a0) / 1.5))
    pts = []
    for j in range(n_ + 1):
        q = pol(r, a0 + (a1 - a0) * j / n_); pts += [q[0], q[1]]
    td.line(pts, fill=WL, width=int(U(w)), joint='curve')
# --- traits blancs entre le potard et les numeros, long trait vers REVERB ONLY
for i in range(11):
    seg(61.5, 70.0, MANG[i], 1.7)
seg(61.5, 98.0, 180.0, 1.9)
# --- accolades : REPEAT (1 a 4) et REVERB ECHO (5 a 11), coupees par leur texte, crochets aux bouts
RA = 96.0
arcl(RA, -160.0, -133.0, 1.3); arcl(RA, -86.0, -50.0, 1.3)
seg(RA - 6.0, RA, -160.0, 1.3); seg(RA - 6.0, RA, -50.0, 1.3)
arcl(RA, -40.0, 30.0, 1.3); arcl(RA, 92.0, 160.0, 1.3)
seg(RA - 6.0, RA, -40.0, 1.3); seg(RA - 6.0, RA, 160.0, 1.3)
img = comp(img, L)
arc_text(MX, MY, RA, -109.5, 'REPEAT', 8.2, 'Bold', WHITE, 0.8)
arc_text(MX, MY, RA, 61.0, 'REVERB ECHO', 8.2, 'Bold', WHITE, 0.8)
# --- numeros blancs (graisses fortes), tournes le long du cercle
for i in range(11):
    a = math.radians(MANG[i])
    T(MX + RN * math.sin(a), MY - RN * math.cos(a), str(i + 1), 12.5, 'Bold', (246, 248, 244, 255), 0.0, 'mm', MANG[i])
T(MX, MY + 109, 'REVERB', 10.5, 'Bold', WHITE, 1.2); T(MX, MY + 123, 'ONLY', 10.5, 'Bold', WHITE, 1.2)
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
T(616, 768, 'ECHO', 8.5, 'Medium', BLACK, 0.8); T(658, 768, 'NORMAL', 8.5, 'Medium', BLACK, 0.8)
T(790.2, 718, 'MIC', 9.5, 'Medium', BLACK, 0.6); T(820.8, 718, 'P.A.', 9.5, 'Medium', BLACK, 0.6); T(851.4, 718, 'INST', 9.5, 'Medium', BLACK, 0.6)
for i, s_ in enumerate('HML'):
    T(1035.2 + i * 30.6, 728, s_, 9.5, 'Medium', BLACK)
T(1380, 764, 'ON   ·   OFF', 8.5, 'Medium', BLACK, 0.8)


# prise jack : photo fournie (hexagone pointe a gauche et a droite), detouree ici (le damier de la photo n'est pas transparent)
_js = np.asarray(Image.open(os.path.join(PHOTOS, 'jack_src.png')).convert('RGB'), np.float32)
_jy, _jx = np.mgrid[0:_js.shape[0], 0:_js.shape[1]].astype(np.float32)
JC, JR = (620.0, 627.0), 574.0                        # centre et rayon (sommet) de l'hexagone, px
_ang = np.arctan2(_jy - JC[1], _jx - JC[0])
_hexr = JR * np.cos(math.pi / 6) / np.cos(((_ang) % (math.pi / 3)) - math.pi / 6)
_jal = np.clip((_hexr - np.hypot(_jx - JC[0], _jy - JC[1])) / 1.5, 0, 1)
_jbox = (int(JC[0] - JR), int(JC[1] - JR), int(JC[0] + JR), int(JC[1] + JR))
JACK_RGBA = Image.fromarray(np.dstack([_js, _jal * 255]).astype(np.uint8), 'RGBA').crop(_jbox)
JACK_R = 19.0                                          # rayon (sommet) sur la facade, unites d'origine


def jack(img, x, y):
    return paste_photo(img, JACK_RGBA, (x - JACK_R, y - JACK_R, x + JACK_R, y + JACK_R), (2.5, 2.5, 3.0, 0.55))


JACKS = [(198, 748), (353, 748), (478, 748), (1200, 748), (1296, 748)]
for (x, y) in JACKS:
    img = jack(img, x, y)
# glissieres : bouton et cavite d'apres la photo fournie (tools/slide_photo.py) ; le bouton est pose par l'editeur
SL_H = 26.0                                            # hauteur de la cavite (unites d'origine) = 214 px de la photo
SL_SU = SL_H / 214.0
for name, cx, cy in (('slide_cavity2.png', 637.0, 744.0), ('slide_cavity3.png', 821.0, 744.0), ('slide_cavity3.png', 1066.0, 750.0)):
    cav = Image.open(os.path.join(OUT, name))
    wv = cav.width * SL_SU
    img = paste_photo(img, cav, (cx - wv / 2, cy - SL_H / 2, cx + wv / 2, cy + SL_H / 2), (1.0, 1.0, 1.5, 0.25))
FAC = False
xx, yy = xx_fin, yy_fin
line_layer = _line_layer0

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
# --- potard de facade : photo fournie (assets/photos/knob_src.png) : bague exterieure brossee, couronne crantee noire,
#     chapeau brosse avec fente noire. Couronne, fente et matiere tournent ; reflets coniques fixes (calque highlight).
KC, KR = (612.9, 631.5), 497.0                         # centre et rayon de la photo (px)
ksrc = np.asarray(Image.open(os.path.join(OUT, 'photos', 'knob_src.png')).convert('RGBA'), np.float32)
sc = KR / RPX
mapx = (KC[0] + (kx - kc) * sc).astype(np.float32); mapy = (KC[1] + (ky - kc) * sc).astype(np.float32)
kp = cv2.remap(ksrc, mapx, mapy, cv2.INTER_AREA if sc > 1 else cv2.INTER_LINEAR, borderValue=(255, 255, 255, 0))
kv = kp[..., :3].mean(2)
ka = np.clip(kp[..., 3] / 255.0, 0, 1) * (kr <= 1.0)
R_CAP, R_RING = 0.615, 0.795                            # chapeau / couronne crantee / bague exterieure
# fente du chapeau (photo decalee de ~7 px a droite du centre) : extraite puis recentree
slot = np.zeros((N, N), np.float32)
SX = (620.0 - KC[0]) / sc
slotm = (np.abs(kx - (kc + SX)) < 0.042 * RPX) & (ky < kc - 0.15 * RPX) & (kr < R_CAP + 0.04) & (kv < 95)
slot[slotm] = 1.0
slot = cv2.GaussianBlur(slot, (0, 0), 1.2)
shiftM = np.float32([[1, 0, -SX], [0, 1, 0]])
slot_c = cv2.warpAffine(slot, shiftM, (N, N))
slot_rgb = cv2.warpAffine(np.dstack([kv] * 3).astype(np.float32), shiftM, (N, N))
capz = (kr <= R_CAP).astype(np.float32); crownz = ((kr > R_CAP) & (kr <= R_RING)).astype(np.float32); ringz = ((kr > R_RING) & (kr <= 1.0)).astype(np.float32)
# matiere sans la fente pour le profil radial
kv_noslot = np.where((np.abs(kx - (kc + SX)) < 0.075 * RPX) & (ky < kc - 0.08 * RPX) & (kr < R_CAP + 0.01), np.nan, kv)
def radial_avg_nan(v, a):
    bins = np.clip((kr * 400).astype(int), 0, 799)
    ok = ~np.isnan(v)
    s_ = np.bincount(bins[ok].ravel(), (v[ok] * a[ok]).ravel(), 800); w_ = np.bincount(bins[ok].ravel(), a[ok].ravel(), 800)
    return (s_ / np.maximum(w_, 1e-6))[bins]
basek = radial_avg_nan(kv_noslot, ka)
save_layer('knob_chrome_shadow', *shadow_layer(ka))
save_layer('knob_chrome_crown', kp[..., :3], ka * crownz)                    # couronne crantee : photo, tourne
save_layer('knob_chrome_cap', np.dstack([basek] * 3), ka * (capz + ringz))   # matiere brossee invariante par rotation
save_layer('knob_chrome_marker', slot_rgb, np.clip(slot_c, 0, 1))           # fente noire, pointee vers le haut
tri = Image.new('L', (N, N), 0); dtri = ImageDraw.Draw(tri)
dtri.polygon([(kc, kc - RPX * 0.965), (kc - RPX * 0.045, kc - RPX * 0.86), (kc + RPX * 0.045, kc - RPX * 0.86)], fill=255)
tria = cv2.GaussianBlur(np.asarray(tri, np.float32) / 255.0, (0, 0), 1.0)
mal = np.clip(slot_c + tria, 0, 1)
save_layer('knob_selector_marker', (slot_rgb * slot_c[..., None] + 14.0 * tria[..., None]) / np.maximum(mal[..., None], 1e-4), mal)
slot_big = (np.abs(kx - (kc + SX)) < 0.075 * RPX) & (ky < kc - 0.08 * RPX) & (kr < R_CAP + 0.01)
tgt = cv2.inpaint(np.clip(kv, 0, 255).astype(np.uint8), slot_big.astype(np.uint8) * 255, 9, cv2.INPAINT_TELEA).astype(np.float32)   # reflets coniques continus sous la fente
tgt = np.where(slot_big, tgt, kv)
# sous la fente : interpolation ligne par ligne entre les bords gauche et droit (les reflets coniques varient doucement)
for yrow in np.where(slot_big.any(1))[0]:
    cols_ = np.where(slot_big[yrow])[0]
    c0_, c1_ = cols_.min() - 2, cols_.max() + 2
    if kr[yrow, c0_] > 0.25:
        tgt[yrow, c0_:c1_ + 1] = np.linspace(kv[yrow, c0_], kv[yrow, c1_], c1_ - c0_ + 1)
hl_rgb, hl_a = highlight_from(tgt, basek, ka * (capz * 0.72 + ringz))       # chapeau : reflets un peu adoucis (la fente reste sombre)
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

print('glissiere ok')

# ====================================================================== Layout.h
def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('·', '\\xc2\\xb7') + '"'

W_ = {'Regular': 0, 'Medium': 1, 'Bold': 2, 'Light': 3}
lines = ['#pragma once', '// genere par tools/make_art.py : ne pas modifier a la main', '#include <cstdint>', '',
         'namespace ssl', '{', '    constexpr float kW = %d.0f, kH = %d.0f;   // repere de l\'interface (100 %%)' % (W, H),
         '    constexpr float kFS = %.4ff, kFCX = %.1ff, kFCY = %.1ff;   // facade agrandie : X = kFCX + (x - kFCX) * kFS' % (FS, FCX, FCY),
         '    struct Txt { float x, y; const char* s; float size; int weight; uint32_t argb; float track; bool left; float rot; };',
         '    inline const Txt* texts (int& n)', '    {', '        static const Txt t[] = {']
for (x, y, s, size, w, col, tr, anc, rot) in TEXTS:
    argb = (col[3] << 24) | (col[0] << 16) | (col[1] << 8) | col[2]
    lines.append('            { %.2ff, %.2ff, %s, %.2ff, %d, 0x%08xu, %.2ff, %s, %.2ff },' % (x, y, cstr(s), size, W_[w], argb, tr, 'true' if anc == 'lm' else 'false', rot))
lines += ['        };', '        n = (int) (sizeof (t) / sizeof (t[0]));', '        return t;', '    }', '}', '']
open(os.path.join(ROOT, 'Layout.h'), 'w').write('\n'.join(lines))
print('Layout.h ok,', len(TEXTS), 'textes')
