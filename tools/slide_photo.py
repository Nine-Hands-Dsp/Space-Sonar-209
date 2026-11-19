"""
Glissières de Space Sonar 209 : d'après la photo de l'utilisateur (assets/photos/slide_src.png, deux interrupteurs OFF / ON).
On ne garde que le bouton et sa cavité (pas la plaque, ni les vis, ni les inscriptions).

- cavité vide : moitié gauche de la photo du haut (bouton à droite) + moitié droite de la photo du bas (bouton à gauche)
- bouton : photo du haut, détouré par différence avec la photo du bas (son ombre portée dans la cavité est gardée)
- cavité à 3 positions (inventée) : la cavité est rallongée en répétant des colonnes du fond (aucun agrandissement)

Sorties : assets/slide_cavity2.png, assets/slide_cavity3.png, assets/slide_button.png ; géométrie affichée pour l'éditeur.
"""
import os
import numpy as np, cv2
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), 'assets')
src = np.asarray(Image.open(os.path.join(OUT, 'photos', 'slide_src.png')).convert('RGB'), np.float32)
DY = 569                                   # décalage vertical entre les deux interrupteurs de la photo
X0, X1, Y0, Y1 = 386, 890, 276, 490        # cavité + son rebord (px, interrupteur du haut)
SEAM = 636                                 # entre le bouton à gauche (bas) et le bouton à droite (haut)

top = src[Y0:Y1, X0:X1]
bot = src[Y0 + DY:Y1 + DY, X0:X1]
h, w = top.shape[:2]
xs = np.arange(w)[None, :, None] + X0
blend = np.clip((xs - (SEAM - 6)) / 12.0, 0, 1)
empty = top * (1 - blend) + bot * blend     # cavité vide

# bouton : différence lissée entre « bouton à droite » et « cavité vide », à droite de la couture
d = cv2.GaussianBlur(np.abs(top - empty).mean(2), (0, 0), 2.0)
al = np.clip((d - 6.0) / 22.0, 0, 1) * (xs[..., 0] >= SEAM - 4)
al = cv2.GaussianBlur(al.astype(np.float32), (0, 0), 0.8)
bx = np.where(al.max(0) > 0.05)[0]
b0, b1 = int(bx.min()) - 2, int(bx.max()) + 3
b1 = min(b1, w)
button = np.dstack([top[:, b0:b1], al[:, b0:b1, None] * 255])
# centre du bouton : sur le corps du bouton (alpha franc)
core = np.where((al > 0.9).any(0))[0]
bc_right = (core.min() + core.max()) / 2.0                 # px, dans la cavité
dl = cv2.GaussianBlur(np.abs(bot - empty).mean(2), (0, 0), 2.0)
corel = np.where(((dl > 28) & (xs[..., 0] < SEAM)).any(0))[0]
bc_left = (corel.min() + corel.max()) / 2.0
travel = bc_right - bc_left


def rounded_alpha(hh, ww, r=16.0, inset=2.0):
    yy, xx = np.mgrid[0:hh, 0:ww].astype(np.float32)
    cx = np.clip(xx, inset + r, ww - 1 - inset - r); cy = np.clip(yy, inset + r, hh - 1 - inset - r)
    return np.clip(r + 0.5 - np.hypot(xx - cx, yy - cy), 0, 1)


def save(name, rgb, a):
    Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.clip(a, 0, 1) * 255]).astype(np.uint8), 'RGBA').save(os.path.join(OUT, name), optimize=True)


save('slide_cavity2.png', empty, rounded_alpha(h, w))
# 3 positions : même pas de bouton, cavité rallongée d'un pas en répétant des colonnes du fond au milieu
step = int(round(travel))
mid = int(round((bc_left + bc_right) / 2.0))
tile = empty[:, mid - 60:mid + 60]
filler = np.concatenate([tile if i % 2 == 0 else tile[:, ::-1] for i in range(step // 120 + 1)], axis=1)[:, :step]   # miroirs : aucune couture
filler = filler[:, ::-1] if (step // 120) % 2 else filler
cav3 = np.concatenate([empty[:, :mid], filler[:, ::-1], empty[:, mid:]], axis=1)
# raccords adoucis (fondu de 10 px de part et d'autre)
for j in (mid, mid + step):
    l, r = cav3[:, j - 10:j].mean(1, keepdims=True), cav3[:, j:j + 10].mean(1, keepdims=True)
    ramp = np.linspace(0, 1, 20)[None, :, None]
    cav3[:, j - 10:j + 10] += (r - l) * (ramp - 0.5) * -1.0 * (1 - np.abs(ramp - 0.5) * 2)
save('slide_cavity3.png', cav3, rounded_alpha(h, cav3.shape[1]))
save('slide_button.png', button[..., :3], button[..., 3] / 255.0)
print('cavite2 %d x %d px, cavite3 %d px de large, bouton %d px (decalage %.1f px depuis le bord gauche du sprite)' % (w, h, cav3.shape[1], b1 - b0, bc_right - b0))
print('centre du bouton a gauche %.1f px, a droite %.1f px, pas %.1f px' % (bc_left, bc_right, travel))
