# Space Sonar 209 — NINE HANDS | DSP

Écho à bande et réverbe à ressorts **VST3** (Windows 64 bits + macOS universel Apple Silicon/Intel), dans l'ergonomie
et les couleurs de la **Roland RE-201 Space Echo** (1974) : trois têtes de lecture, sélecteur de mode à 12 positions,
vitesse de bande réglable avec glissement de hauteur, auto-oscillation, BASS / TREBLE dans la boucle, réverbe à ressorts,
préamplis MIC / P.A. / INST, plus un panneau de réglages étendus (usure de la bande, wow & flutter, inertie du moteur,
LFO de vitesse, synchro au tempo, stéréo, filtres).

<p align="center">
  <a href="https://github.com/Nine-Hands-Dsp/space-sonar-209/releases/latest/download/SpaceSonar209-VST3-Windows.zip"><img alt="Télécharger pour Windows" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-Windows-0078D6?style=for-the-badge"></a>
  <a href="https://github.com/Nine-Hands-Dsp/space-sonar-209/releases/latest/download/SpaceSonar209-VST3-macOS.zip"><img alt="Télécharger pour macOS" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-macOS-111111?style=for-the-badge&logo=apple&logoColor=white"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/space-sonar-209/releases/latest/download/SpaceSonar209-Notice.pdf"><img alt="Télécharger la notice (PDF)" src="https://img.shields.io/badge/%F0%9F%93%96%20Notice-PDF-919496?style=for-the-badge"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/space-sonar-209/releases/latest"><img alt="Dernière version" src="https://img.shields.io/github/v/release/Nine-Hands-Dsp/space-sonar-209?label=derni%C3%A8re%20version&color=34843a"></a>
</p>

## Installation
Les boutons ci-dessus téléchargent toujours la dernière version.
- **Windows** : copie `Space Sonar 209.vst3` dans `C:\Program Files\Common Files\VST3\`
- **macOS** : copie `Space Sonar 209.vst3` dans `~/Library/Audio/Plug-Ins/VST3/` puis lance une fois
  `xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Space Sonar 209.vst3"`
- **Reaper** : re-scanne les plug-ins. Le plugin apparaît comme **Space Sonar 209 (NINE HANDS | DSP)**.

## Commandes
| Commande | Rôle |
|---|---|
| **MIC / FROM P.A. / INSTRUMENT VOLUME** | niveau de chaque étage d'entrée (unité à 5). Le sélecteur **MIC · P.A. · INST** (ou un clic sur la case de la prise) choisit l'étage : MIC = préampli micro chaud, P.A. = ligne propre, INST = préampli instrument. L'anneau bleu montre l'entrée active |
| **MODE SELECTOR** | 1 à 4 : têtes seules (1, 2, 3, 2+3) ; 5 à 11 : têtes + réverbe (1, 2, 3, 1+2, 2+3, 1+3, 1+2+3) ; en bas : **REVERB ONLY** |
| **REPEAT RATE** | vitesse de bande : tête 1 de 177 ms à 69 ms (têtes 2 et 3 : x 2 et x 3). Tourner pendant le jeu fait glisser la hauteur des répétitions |
| **INTENSITY** | réinjection ; l'auto-oscillation démarre vers 8,5 (une tête), 7,8 (deux) et 7,5 (trois) |
| **ECHO VOLUME / REVERB VOLUME** | niveau de l'écho et de la réverbe à ressorts |
| **BASS / TREBLE** | tonalité de l'écho seulement, dans la boucle (les répétitions s'assombrissent ou s'éclaircissent) |
| **ECHO / NORMAL** | NORMAL coupe l'entrée de la bande : les répétitions en cours continuent (geste « dub ») |
| **OUTPUT H / M / L** | niveau de sortie +6 / 0 / -6 dB |
| **ECHO CANCEL** | clic sur la case : coupe l'écho (anneau rouge) ; assignable en MIDI |
| **POWER** / **BYPASS** | marche / contournement ; rallumer efface la bande, comme sur l'appareil |
| **VU / PEAK LEVEL** | niveau envoyé à la bande ; la LED s'allume quand le préampli sature |
| **TEMPO SYNC** | REPEAT RATE en valeurs de notes, calées sur la tête la plus courte du mode (et le RATE du LFO en mesure) |
| **RATE RANGE** | NORMAL, SLOW (temps x 2), FAST (temps x 0,5) |
| **SPEED** | réglage fin de la vitesse de bande (-24 % à +32 %) |
| **STEREO MODE** | SINGLE (mono, comme l'original) ou DUAL (gauche et droite indépendants) |
| **TAPE SPEED LFO** | 6 formes (sinus, triangle, rampes, carré, aléatoire), RATE, WAVEFORM (adoucit la forme), DEPTH |
| **CONDITION / W/F / MOTOR TORQUE** | usure de la bande (aigus, souffle, pertes de niveau), wow & flutter, inertie du moteur (glissements lents ou rapides) |
| **LOW CUT / HIGH CUT / MASTER** | filtres de l'écho et de la réverbe, niveau général |
| En-tête | presets (◀ ▶, clic = menu, presets utilisateur), vumètres IN / OUT, BYPASS, comparaison **A / B**, annuler / rétablir, menu ≡ |
| **BLOCK DIAGRAM** | le chemin du signal s'allume selon l'entrée, le mode, ECHO CANCEL et la stéréo ; temps de chaque tête en bas |

Clic droit sur une commande : MIDI Learn / Clear MIDI (aucun CC par défaut). Clic droit sur la façade : presets,
réglages, MIDI, taille de la fenêtre (60 / 75 / 100 %, jusqu'à 150 % au coin), Glow. Program Change = presets usine.

## Compiler avec GitHub
Un push sur `main` lance l'onglet **Actions** → *Build Space Sonar 209 VST3 (Windows + macOS)* puis publie la Release
(`SpaceSonar209-VST3-Windows.zip`, `SpaceSonar209-VST3-macOS.zip`, `SpaceSonar209-Notice.pdf`). La version Windows sort
sans attendre le Mac. Images de l'interface : `tools/make_art.py` ; contrôle du moteur et captures : `tools/render`
(Linux, `analyse` pour les tests de stabilité, de temps et de charge).

Anti-gel v2 : rendu logiciel sous Windows, caches refaits par le timer seulement, chien de garde des redessins,
aucun appel à l'hôte depuis le thread audio, JUCE 8.0.15.
