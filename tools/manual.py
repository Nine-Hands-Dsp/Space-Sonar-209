# SPACE SONAR 209 (NINE HANDS | DSP) - notice PDF en francais, illustree avec le rendu reel du plugin (tools/render).
# Usage : python3 tools/manual.py <dossier des captures (tools/render)> <fichier PDF de sortie>
import os, sys, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS, OUT = sys.argv[1], sys.argv[2]
VERSION = next(l.split("VERSION")[1].strip(" )\n") for l in open(os.path.join(ROOT, "CMakeLists.txt")) if l.startswith("project("))
TMP = os.path.dirname(os.path.abspath(OUT))
for n, f in (("Pop", "fonts/Poppins-Regular"), ("PopL", "fonts/Poppins-Light"), ("PopM", "fonts/Poppins-Medium"), ("PopB", "fonts/Poppins-Bold"),
             ("Title", "tools/fonts_notice/BarlowCondensed-ExtraBold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(ROOT, f + ".ttf")))

ACC = colors.HexColor("#34843A"); ACC_L = colors.HexColor("#8EDB6C")
DARK = colors.HexColor("#17201A"); GREY = colors.HexColor("#5F6A62"); LIGHT = colors.HexColor("#EEF3E8")
LOGO_PNG = os.path.join(TMP, "nine_hands_logo_blanc.png")
subprocess.run(["rsvg-convert", "-w", "1200", os.path.join(ROOT, "logo", "NineHandsLogoWhite.svg"), "-o", LOGO_PNG], check=True)

def shot(name): return Image.open(os.path.join(SHOTS, name)).convert("RGB")
src = shot("ss_default.png")
k = src.width / 1600.0                                     # repere de l'interface : 1600 x 1400 unites

# ---------- capture annotee ----------
MARKS = [(1, 440, 48), (2, 1172, 40), (3, 1245, 86), (4, 1366, 86), (5, 1461, 86), (6, 1540, 86),
         (7, 150, 410), (8, 112, 600), (9, 770, 410), (10, 840, 440), (11, 1210, 440), (12, 840, 575), (13, 1210, 575),
         (14, 1470, 545), (15, 300, 805), (16, 640, 805), (17, 1060, 805), (18, 1360, 805),
         (19, 120, 975), (20, 840, 1090), (21, 1195, 1210), (22, 1250, 1010), (23, 850, 1268), (24, 1330, 1385)]
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(ROOT, "fonts", "Poppins-Bold.ttf"), int(13 * k))
for n, x, y in MARKS:
    cx, cy, r = x * k, y * k, 14 * k
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(52, 132, 58), width=int(2.5 * k))
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(20, 32, 22), anchor="mm")
annotated = os.path.join(TMP, "ss_annotated.jpg"); ann.convert("RGB").resize((1600, 1400), Image.LANCZOS).save(annotated, quality=90)

def crop_u(im, box, w=None):
    c = im.crop(tuple(int(v * k) for v in box))
    if w: c = c.resize((w, int(c.height * w / c.width)), Image.LANCZOS)
    return c
def save(im, name): p = os.path.join(TMP, name); im.save(p, quality=90); return p
facade = save(crop_u(src, (100, 270, 1500, 800), 1800), "ss_facade.jpg")
diag_dub = save(crop_u(shot("ss_dub.png"), (40, 960, 810, 1390), 1200), "ss_diagram.jpg")
lower = save(crop_u(src, (810, 990, 1590, 1390), 1200), "ss_lower.jpg")
header_st = save(crop_u(shot("ss_status.png"), (0, 0, 1600, 100), 1800), "ss_header.jpg")
red = shot("ss_single_reverb_only.png").resize(src.size, Image.LANCZOS)
jacks = save(crop_u(red, (110, 686, 1490, 790), 1800), "ss_jacks.jpg")

# ---------- couverture : l'appareil detoure (coffre en tolex + embouts), reflet au sol, halo vert tres doux ----------
unit = crop_u(src, (14, 134, 1586, 920))
uw, uh = unit.size
yy, xx = np.mgrid[0:uh, 0:uw].astype(np.float64)
def rr(x0, y0, x1, y1, r):
    x0, y0, x1, y1, r = [(v) * k for v in (x0 - 14, y0 - 134, x1 - 14, y1 - 134, r)]
    cx = np.clip(xx, x0 + r, x1 - r); cy = np.clip(yy, y0 + r, y1 - r)
    return np.clip(0.5 - (np.hypot(xx - cx, yy - cy) - r), 0, 1)
m = rr(40, 140, 1560, 900, 40)
for (x0, x1) in ((20, 46), (1554, 1580)):
    for yc in (300, 700): m = np.maximum(m, rr(x0, yc - 34, x1, yc + 34, 8))
m = np.maximum(m, rr(560, 896, 1040, 914, 4))
cut = Image.new("RGBA", unit.size, (0, 0, 0, 0)); cut.paste(unit, (0, 0), Image.fromarray((m * 255).astype(np.uint8)))
cut = cut.crop(cut.getbbox()); cw, ch = cut.size
pad = int(cw * 0.16)
canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
yy2, xx2 = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
gcx, gcy = canvas.width / 2, pad + ch * 0.45
glow = np.exp(-(((xx2 - gcx) / (cw * 0.62)) ** 2 + ((yy2 - gcy) / (ch * 0.62)) ** 2)) * 26
edge = np.clip(np.minimum.reduce([xx2, canvas.width - 1 - xx2, yy2, canvas.height - 1 - yy2]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
canvas = Image.alpha_composite(canvas, Image.fromarray(np.dstack([glow * 0.30, glow * 0.85, glow * 0.34, np.full_like(glow, 255)]).astype(np.uint8), "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM); ra = np.array(refl).astype(np.float64)
ra[..., 3] *= (np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22)[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
cover = os.path.join(TMP, "ss_cover_cutout.jpg"); canvas.convert("RGB").save(cover, quality=92)

def img(path, width):
    im = Image.open(path); return RLImage(path, width=width, height=width * im.height / im.width)

st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=12.5, leading=16, textColor=DARK, spaceAfter=3, spaceBefore=8, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "tip": ParagraphStyle("tip", fontName="Pop", fontSize=9.0, leading=12.4, textColor=DARK, spaceAfter=3),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY, spaceAfter=4),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cells": ParagraphStyle("cells", fontName="Pop", fontSize=7.6, leading=9.6, textColor=DARK),
    "cellbs": ParagraphStyle("cellbs", fontName="PopM", fontSize=7.6, leading=9.6, textColor=DARK),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items, s="p"): return [P("&bull;&nbsp;&nbsp;" + i, s) for i in items]
def table(rows, widths, header=True, small=False):
    data = [[P(c, ("cellbs" if small else "cellb") if (header and r == 0) else ("cells" if small else "cell")) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#D5DFCF")),
             ("TOPPADDING", (0, 0), (-1, -1), 2 if small else 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 2 if small else 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

W = A4[0] - 36 * mm

# ---------- donnees (identiques au code livre : PluginProcessor.cpp, SpaceDsp.h) ----------
MODES = [["1", "1", "—"], ["2", "2", "—"], ["3", "3", "—"], ["4", "2 + 3", "—"], ["5", "1", "oui"], ["6", "2", "oui"], ["7", "3", "oui"],
         ["8", "1 + 2", "oui"], ["9", "2 + 3", "oui"], ["10", "1 + 3", "oui"], ["11", "1 + 2 + 3", "oui"], ["REVERB ONLY", "—", "oui (seule)"]]
PRESETS = [
    ("Default", "Mode 3 (tête 3), Repeat Rate 5, Intensity 4,5, Echo Volume 6 : l'écho de bande classique, réverbe coupée."),
    ("Classic Slapback", "Tête 1 seule, bande rapide, une répétition franche : le slapback des années 50."),
    ("Rockabilly Slap", "Encore plus court et plus brillant, presque sans réinjection."),
    ("Dub Siren", "Têtes 2 + 3 et réverbe (mode 9), bande lente, Intensity juste sous l'auto-oscillation, graves en avant, bande usée : poussez Intensity ou tournez Repeat Rate pour les envolées dub."),
    ("Triple Head Rhythm", "Les trois têtes et la réverbe (mode 11) : un motif rythmique dense, à jouer en notes détachées."),
    ("Spring Only", "REVERB ONLY : la réverbe à ressorts seule, sombre et métallique."),
    ("Runaway Oscillation", "Intensity au-delà du seuil, moteur mou : l'écho s'emballe et oscille seul. Tournez Repeat Rate pour faire hurler la bande."),
    ("Space Drift", "Tête 3 + réverbe, LFO de vitesse lent et wow & flutter marqués : des répétitions qui flottent."),
    ("Wobbly Old Tape", "Condition 9, W/F 8 : une bande fatiguée qui pleure, perd des aigus et du niveau."),
    ("Ambient Wash", "Mode 11, Rate Range SLOW, beaucoup de réinjection et de réverbe, aigus coupés : une nappe pour les ambiances."),
    ("Lead Echo 1/4 (Sync)", "Tempo Sync, tête 1 + réverbe à la noire : l'écho de solo calé sur le tempo du projet."),
    ("Metal Lead 7-String", "Tempo Sync à la croche pointée, Low Cut 250 Hz et High Cut 6 kHz : l'écho reste derrière le jeu en accordage grave."),
    ("Mic Grit", "Entrée MIC, préampli micro poussé : le grain chaud de l'étage micro, comme les chanteurs dub l'utilisaient."),
    ("Chopped Vibrato", "LFO carré rapide sur la vitesse de bande : des répétitions hachées qui sautent de hauteur."),
]

def body():
    f = [NextPageTemplate("p"), PageBreak()]
    f.append(P("1. Présentation", "h1"))
    f += [P("<b>SPACE SONAR 209</b> est un plugin NINE HANDS | DSP qui reprend l'ergonomie, les couleurs et le comportement de la "
            "<b>Roland RE-201 Space Echo</b> (1974), l'écho à bande à trois têtes et réverbe à ressorts qui a façonné le dub, le reggae "
            "et des milliers de disques rock et ambient."),
          P("Comme sur l'appareil, une boucle de bande sans fin passe devant une tête d'enregistrement puis <b>trois têtes de lecture "
            "espacées régulièrement</b> : la tête 2 répète deux fois plus tard que la tête 1, la tête 3 trois fois plus tard. Le "
            "<b>MODE SELECTOR</b> à 12 positions choisit les têtes et la réverbe. <b>REPEAT RATE</b> change la vitesse de la bande : "
            "tourné pendant le jeu, il fait glisser la hauteur des répétitions, avec l'inertie du moteur. <b>INTENSITY</b> règle la "
            "réinjection jusqu'à l'auto-oscillation. La bande sature doucement, perd des aigus quand elle ralentit ou s'use, et "
            "le wow & flutter du moteur donne la modulation vivante du Space Echo."),
          P("Le panneau du bas ajoute des réglages qui n'existent pas sur l'appareil : usure de la bande, wow & flutter, couple du "
            "moteur, LFO de vitesse de bande, synchro au tempo du DAW, plage de vitesse, mode stéréo, filtres et niveau général. "
            "L'écran <b>BLOCK DIAGRAM</b> montre en direct le chemin du signal.")]
    f.append(P("2. Installation", "h1"))
    for h, x in (("Windows", "Téléchargez <b>SpaceSonar209-VST3-Windows.zip</b> (bouton du README du dépôt GitHub Nine-Hands-Dsp/space-sonar-209, "
                              "ou l'application Backline), décompressez-le et copiez le dossier <b>Space Sonar 209.vst3</b> dans "
                              "<b>C:\\Program Files\\Common Files\\VST3</b>."),
                 ("macOS", "Téléchargez <b>SpaceSonar209-VST3-macOS.zip</b>, décompressez-le et copiez <b>Space Sonar 209.vst3</b> dans "
                           "<b>~/Bibliothèque/Audio/Plug-Ins/VST3</b>. Si macOS le bloque, tapez <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/\"Space Sonar 209.vst3\"</b> "
                           "dans le Terminal. Le binaire est universel (Apple Silicon et Intel)."),
                 ("Reaper", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Le plugin apparaît sous le nom "
                            "<b>Space Sonar 209 (NINE HANDS | DSP)</b>. Pour le piloter en MIDI (M-Vave Chocolate Plus, pédale d'expression), "
                            "la piste doit recevoir le MIDI du contrôleur.")):
        f.append(P(h, "h2")); f.append(P(x))
    f.append(PageBreak())

    f.append(P("3. Vue d'ensemble de l'interface", "h1"))
    f.append(img(annotated, W * 0.74))
    rows = [["#", "Commande", "Rôle"],
            ["1", "Barre de presets / ligne d'état", "Flèches = preset précédent / suivant, clic sur le nom = menu des presets. Pendant un réglage : valeur en direct, puis retour au preset."],
            ["2", "IN / OUT", "Vumètres d'entrée et de sortie du plugin."],
            ["3", "BYPASS", "Contourne l'effet (même réglage que POWER)."],
            ["4", "A / B", "Deux jeux de réglages à comparer : clic sur A ou B."],
            ["5", "Annuler / rétablir", "Revient sur les derniers réglages (souris, menus, presets)."],
            ["6", "Menu ≡", "Le menu principal (aussi au clic droit sur la façade)."],
            ["7", "VU, PEAK LEVEL", "Niveau envoyé à la bande ; la LED s'allume quand le préampli sature."],
            ["8", "MIC / FROM P.A. / INSTRUMENT VOLUME", "Niveau de chaque étage d'entrée."],
            ["9", "MODE SELECTOR", "Têtes et réverbe, 12 positions."],
            ["10", "BASS / TREBLE", "Tonalité de l'écho, dans la boucle."],
            ["11", "REVERB VOLUME", "Niveau de la réverbe à ressorts."],
            ["12", "REPEAT RATE", "Vitesse de bande = temps des répétitions."],
            ["13", "INTENSITY / ECHO VOLUME", "Réinjection ; niveau de l'écho."],
            ["14", "POWER", "Marche / arrêt (témoin rouge)."],
            ["15", "Prises MIC, FROM P.A., INSTRUMENT", "Clic sur une case = choix de l'entrée (anneau bleu)."],
            ["16", "ECHO / NORMAL, MIC · P.A. · INST", "Coupure « dub » de l'entrée de la bande ; sélecteur d'entrée."],
            ["17", "OUTPUT H / M / L", "Niveau de sortie +6 / 0 / -6 dB."],
            ["18", "ECHO CANCEL", "Clic = coupe l'écho (anneau rouge)."],
            ["19", "BLOCK DIAGRAM", "Chemin du signal en direct, temps des têtes."],
            ["20", "TEMPO SYNC, STEREO MODE", "Synchro au tempo ; SINGLE (mono) ou DUAL (stéréo)."],
            ["21", "RATE RANGE, SPEED", "Plage et réglage fin de la vitesse de bande."],
            ["22", "TAPE SPEED LFO", "Modulation de la vitesse de bande : forme, RATE, WAVEFORM, DEPTH."],
            ["23", "CONDITION, W/F, MOTOR TORQUE", "Usure de la bande, wow & flutter, inertie du moteur."],
            ["24", "LOW CUT, HIGH CUT, MASTER", "Filtres de l'écho et de la réverbe, niveau général."]]
    f.append(table(rows, [8 * mm, 52 * mm, W - 60 * mm], small=True))
    f.append(PageBreak())

    f.append(P("4. La façade en détail", "h1"))
    f.append(img(facade, W))
    f.append(P("Toutes les commandes : glisser vers le haut / le bas (Maj = réglage fin), molette, double-clic = valeur par défaut, clic droit = MIDI Learn.", "small"))
    det = [
        ("MIC VOLUME, FROM P.A. VOLUME, INSTRUMENT VOLUME", "Le niveau de chacun des trois étages d'entrée de l'appareil (unité à 5, jusqu'à +12 dB à 10). Seul l'étage choisi avec le sélecteur <b>MIC · P.A. · INST</b> (ou un clic sur la case de sa prise) reçoit le signal du plugin ; son anneau s'allume en bleu. <b>MIC</b> : préampli micro, gain plus élevé, plus coloré et plus chaud ; <b>P.A.</b> : entrée ligne, la plus propre ; <b>INST</b> (par défaut) : préampli instrument, légère saturation. Comme sur la RE-201, le son sec passe aussi par le préampli."),
        ("VU et PEAK LEVEL", "Le VU-mètre rétroéclairé montre le niveau envoyé à la bande (0 VU ≈ -18 dBFS efficaces). La LED <b>PEAK LEVEL</b> s'allume quand le préampli commence à saturer : baissez le volume d'entrée si elle reste allumée."),
        ("MODE SELECTOR", "Choisit les têtes de lecture et la réverbe (tableau ci-dessous). Les numéros verts (5 à 11) ajoutent la réverbe ; en bas, <b>REVERB ONLY</b> : la réverbe seule, ECHO VOLUME et ECHO / NORMAL n'agissent plus."),
        ("REPEAT RATE", "La vitesse de la bande. Tout à gauche, la tête 1 répète à <b>177 ms</b> (tête 2 : 354 ms, tête 3 : 531 ms) ; tout à droite, à <b>69 ms</b> (138 et 207 ms). Le moteur met un peu de temps à changer de vitesse (MOTOR TORQUE) : tourner le potard pendant que les répétitions sonnent fait glisser leur hauteur, l'effet de « sirène » du dub. En TEMPO SYNC, le potard choisit une valeur de note."),
        ("INTENSITY", "La réinjection (nombre de répétitions). La boucle s'emballe et oscille seule vers <b>8,5</b> avec une tête, <b>7,8</b> avec deux et <b>7,5</b> avec trois ; la saturation de la bande et de l'ampli d'écho tient le niveau sous 0 dBFS."),
        ("ECHO VOLUME", "Le niveau de l'écho ajouté au son sec. À 0, plus d'écho."),
        ("BASS / TREBLE", "La tonalité de l'écho (±12 dB vers 160 Hz et 2,8 kHz), sans toucher au son sec ni à la réverbe. Elle est dans la boucle : chaque répétition s'assombrit (TREBLE baissé) ou s'éclaircit un peu plus que la précédente."),
        ("REVERB VOLUME", "Le niveau de la réverbe à ressorts (modes 5 à 12). Trois ressorts dispersifs, sombres et métalliques, en parallèle sur le signal d'entrée, comme sur l'appareil."),
        ("POWER", "Marche / arrêt (BYPASS dans l'en-tête fait la même chose). Arrêté, le signal passe sans traitement. Rallumer efface la bande, comme sur l'appareil."),
    ]
    MODES_T = table([["Position", "Têtes", "Réverbe"]] + MODES, [26 * mm, 26 * mm, 26 * mm], small=True)
    for h, x in det:
        f.append(P(h, "h2")); f.append(P(x))
        if h == "MODE SELECTOR": f.append(MODES_T)
    f.append(P("Le bandeau des prises", "h2"))
    f.append(img(jacks, W))
    f += bullets(["<b>ECHO / NORMAL</b> : en NORMAL, plus rien n'entre sur la bande, mais les répétitions déjà enregistrées continuent : le geste « dub » pour isoler une phrase et la laisser tourner.",
                  "<b>MIC · P.A. · INST</b> : choix de l'étage d'entrée (voir plus haut).",
                  "<b>OUTPUT H / M / L</b> : niveau de sortie +6 / 0 / -6 dB, comme le commutateur de niveau de l'appareil (M par défaut).",
                  "<b>ECHO CANCEL</b> : un clic sur la case coupe l'écho (l'anneau de la prise s'allume en rouge) ; à assigner à un footswitch MIDI pour couper l'écho au pied. L'anneau orange de OUTPUT rappelle la sortie."])
    f.append(P("5. Le panneau du bas", "h1"))
    f.append(img(lower, W * 0.75))
    low = [
        ("TEMPO SYNC", "REPEAT RATE choisit alors une valeur de note (de la triple croche à la ronde, en passant par les triolets et notes pointées), calée sur la <b>tête la plus courte du mode</b> au tempo du DAW. Le RATE du LFO passe aussi en valeurs de mesure."),
        ("STEREO MODE", "<b>SINGLE</b> : mono, comme l'original (gauche + droite mélangés, même son des deux côtés). <b>DUAL</b> (par défaut) : la gauche et la droite ont chacune leur bande, leur préampli et leur réverbe, avec des fluctuations différentes : un écho large."),
        ("RATE RANGE, SPEED", "<b>NORMAL</b> : la plage de l'appareil ; <b>SLOW</b> : temps doublés (jusqu'à 354 ms sur la tête 1) ; <b>FAST</b> : temps divisés par deux. <b>SPEED</b> affine la vitesse de -24 % à +32 % autour de REPEAT RATE."),
        ("TAPE SPEED LFO", "Module la vitesse de la bande, donc la hauteur des répétitions. Six formes : sinus, triangle, rampe descendante, rampe montante, carré, aléatoire. <b>RATE</b> : 0,05 à 10 Hz (ou valeur de mesure en TEMPO SYNC) ; <b>WAVEFORM</b> : adoucit la forme (à gauche les arêtes vives, à droite très arrondie) ; <b>DEPTH</b> : jusqu'à ±6 % de vitesse (environ un demi-ton)."),
        ("CONDITION", "L'état de la bande et des têtes : en tournant vers la droite, la bande perd des aigus, souffle davantage, sature plus vite et, au-delà de la moitié, perd ponctuellement du niveau comme une vieille bande."),
        ("W/F", "Le wow (fluctuation lente) et le flutter (rapide, du cabestan) du moteur. Une bande usée (CONDITION) les amplifie."),
        ("MOTOR TORQUE", "L'inertie du moteur : à gauche (couple faible), la vitesse change lentement et les glissements de hauteur sont longs ; à droite, elle suit le potard presque tout de suite."),
        ("LOW CUT, HIGH CUT", "Filtres de l'écho et de la réverbe (le son sec n'est pas filtré) : 20 Hz à 1 kHz et 1 kHz à 20 kHz, « Off » aux butées."),
        ("MASTER", "Niveau général de -24 à +12 dB. Un limiteur doux garde la sortie sous 0 dBFS, même en auto-oscillation."),
    ]
    for h, x in low: f.append(P(h, "h2")); f.append(P(x))
    f.append(KeepTogether([P("BLOCK DIAGRAM", "h2"), img(diag_dub, W * 0.72), P("Le schéma bloc en mode 9, entrée MIC, DUAL : les cases et les fils du chemin actif s'allument.", "small")]))
    f.append(P("L'écran suit les réglages : entrée choisie (MIC passe par son MIC AMP), ECHO TAPE et TONE allumés quand l'écho est actif, REVERB et son ampli de retour (MIC AMP) dans les modes 5 à 12, AMP (OUTPUT H / M / L) puis OUT L / OUT R ; la voie droite s'allume en DUAL. En bas, le mode et le temps de chaque tête en millisecondes (et la valeur de note en TEMPO SYNC)."))

    f.append(P("6. MIDI", "h1"))
    f.append(P("Toutes les commandes (potards, sélecteurs, POWER, ECHO CANCEL, boutons du panneau du bas) se pilotent en MIDI. Space Sonar 209 écoute tous les canaux. <b>Aucun CC n'est assigné d'origine</b> : chaque assignation se fait par <b>MIDI Learn</b>."))
    f += bullets(["<b>MIDI Learn</b> : clic droit sur une commande &gt; <b>MIDI Learn: &lt;nom&gt;</b>, puis bougez la pédale ou appuyez sur le bouton MIDI ; la ligne d'état confirme (ex. <i>CC 64 · Echo Cancel</i>). <b>Clear MIDI</b> retire l'assignation ; clic droit sur la façade &gt; MIDI &gt; <b>Clear all MIDI assignments</b> les retire toutes. Les assignations sont enregistrées avec le projet.",
                  "<b>Pédale d'expression</b> : la course réelle de la pédale est apprise (une pédale qui n'envoie que 2 à 116 atteint quand même 0 et 100 %). Idéal sur REPEAT RATE (sirène dub), INTENSITY (emballement) ou ECHO VOLUME.",
                  "<b>Interrupteurs</b> (POWER, ECHO CANCEL, TEMPO SYNC) : 64 ou plus = marche. <b>Sélecteurs</b> (MODE, entrée, sortie, plage…) : la course du CC est partagée entre les positions.",
                  "<b>Program Change</b> 1 à 14 = presets usine. Tous les paramètres sont aussi automatisables dans le DAW."])
    f.append(P("7. Fenêtre", "h1"))
    f.append(P("Clic droit &gt; <b>Plugin size</b> : <b>60 %</b> (par défaut, taille minimale), <b>75 %</b> ou <b>100 %</b> ; <b>Default size</b> choisit la taille d'ouverture de toutes les nouvelles fenêtres. Le coin en bas à droite agrandit jusqu'à 150 % (proportions fixes). <b>Glow intensity</b> (25 à 100 %) dose le halo des LED et des anneaux des prises."))
    f.append(KeepTogether([img(header_st, W), P("L'en-tête : la ligne d'état remplace le nom du preset pendant un réglage, puis s'efface.", "small")]))

    f.append(P("8. Presets", "h1"))
    f.append(P("Flèches de la barre de presets, clic sur le nom, clic droit, ou menu de presets du DAW. <b>Save user preset</b> enregistre jusqu'à 9 presets personnels (dossier « Nine Hands DSP/Space Sonar 209/User Presets »). Les presets ne changent ni POWER ni ECHO CANCEL."))
    rows = [["PC", "Preset", "Description"]] + [[str(i + 1), "<b>" + n + "</b>", d] for i, (n, d) in enumerate(PRESETS)]
    f.append(table(rows, [8 * mm, 36 * mm, W - 44 * mm], small=True))
    f.append(P("9. Conseils", "h1"))
    f += bullets(["Dans Reaper, placez Space Sonar 209 <b>après</b> vos saturations et avant Ampire / Impulse pour un écho « dans l'ampli », comme une RE-201 branchée devant l'ampli ; ou après Impulse (en DUAL, entrée P.A.) pour un écho de studio sur le son final.",
                  "Guitare 7 cordes en G ou plus bas : LOW CUT vers 150 à 250 Hz et BASS un peu sous 5 gardent les répétitions derrière les riffs ; preset <i>Metal Lead 7-String</i>.",
                  "Dub : ECHO / NORMAL au pied (MIDI Learn) pour capturer une phrase, puis INTENSITY vers 8 et REPEAT RATE à la pédale d'expression.",
                  "Si la LED PEAK LEVEL reste allumée, baissez le volume de l'entrée choisie : la saturation du préampli s'ajoute à celle de la bande.",
                  "Pour retrouver le Space Echo d'origine : STEREO MODE SINGLE, RATE RANGE NORMAL, SPEED au centre, LFO à 0."], "tip")
    tech = [["Format", "VST3 (Windows 64 bits, macOS universel)"], ["Canaux", "Mono ou stéréo ; SINGLE (mono) ou DUAL (stéréo)"],
            ["Latence", "0 échantillon"], ["Temps des têtes", "tête 1 : 69 à 177 ms (NORMAL), x 2 en SLOW, x 0,5 en FAST ; têtes 2 et 3 : x 2 et x 3 ; tête 1 jusqu’à 0,95 s en TEMPO SYNC"],
            ["Modes", "12 : 4 écho seul, 7 écho + réverbe, 1 réverbe seule"], ["Réverbe", "3 ressorts dispersifs, environ 2,2 s"],
            ["Traitement", "bande à interpolation d'Hermite, saturations anti-repliement (ADAA), filtres TPT, paramètres lissés"],
            ["CPU", "environ 5 % d'un cœur à 48 kHz en DUAL (mode 11, LFO)"],
            ["MIDI", "tous canaux ; MIDI Learn sur toutes les commandes (rien d'assigné d'origine) ; Program Change 1-14"],
            ["Fenêtre", "60 / 75 / 100 %, jusqu'à 150 %, jamais sous 60 % ; anti-gel v2 sous Windows"]]
    f.append(KeepTogether([P("10. Caractéristiques techniques", "h1"), table(tech, [30 * mm, W - 30 * mm], header=False, small=True)]))
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Space Sonar 209 (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="SPACE SONAR 209 - Notice v" + VERSION, author="NINE HANDS | DSP")
# ---- Couverture « minimale » (modele 6) : gabarit fixe commun a toutes les notices ----
PEDAL_H, PEDAL_MAXW, PEDAL_TOP = 82 * mm, 115 * mm, 84 * mm
LANG_TOP = 18 * mm
NAME_TOP, NAME_PT = 194 * mm, 30
SUB_TOP, SUB_PT = 208 * mm, 10
LOGO_W, LOGO_BOTTOM = 38 * mm, 22 * mm
BAND_H = 3 * mm
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, 0, A4[0], BAND_H, stroke=0, fill=1)
    def line(text, top, font, size, cs, col):
        w = pdfmetrics.stringWidth(text, font, size) + cs * (len(text) - 1)
        t = c.beginText((A4[0] - w) / 2, A4[1] - top - size * 0.8); t.setFont(font, size); t.setCharSpace(cs)
        t.setFillColor(col); t.textOut(text); c.drawText(t)
    line("FRANÇAIS", LANG_TOP, "PopM", 8.5, 0, ACC_L)
    kk = min(PEDAL_H / ch, PEDAL_MAXW / cw)
    iw, ih = canvas.size
    dw, dh = iw * kk, ih * kk
    y0 = A4[1] - (PEDAL_TOP + (PEDAL_H - ch * kk) / 2) + pad * kk
    c.drawImage(cover, (A4[0] - dw) / 2, y0 - dh, dw, dh)
    line("SPACE SONAR 209", NAME_TOP - NAME_PT * 0.02, "Title", NAME_PT, NAME_PT * 0.5, colors.white)
    line("Écho à bande et réverbe à ressorts", SUB_TOP, "Pop", SUB_PT, 1.5, colors.HexColor("#BDC4B8"))
    lw, lh = Image.open(LOGO_PNG).size
    c.drawImage(LOGO_PNG, (A4[0] - LOGO_W) / 2, LOGO_BOTTOM, LOGO_W, LOGO_W * lh / lw, mask="auto")
    c.restoreState()
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
doc.build([Spacer(1, 1)] + body())
print(OUT)
