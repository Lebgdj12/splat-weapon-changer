# Splat Weapon Changer — Aroma Plugin

Plugin Aroma pour **Splatoon 1 (Wii U, v2.12.0)** qui permet de changer d'arme en plein jeu depuis un menu sur la console. **Pas de PC requis au runtime** — tout se gère depuis le menu config Aroma.

## ⚠️ Sécurité d'abord

Si tu pushes ce repo sur GitHub :
- **NE JAMAIS** commiter un Personal Access Token (`ghp_...`) dans le code, le README, ou un fichier de config
- Si tu l'as fait par accident : revoke-le immédiatement sur https://github.com/settings/tokens, puis `git filter-branch` ou BFG repo cleaner pour purger l'historique
- Le workflow GitHub Actions de ce repo n'a **pas besoin de token** pour tourner sur ton propre repo

## Ce que fait le plugin

- Lit la chaîne de pointeurs `[0x106E46E8] → +0x290 → player struct`
- Re-poke l'ID de l'arme choisie à 60 Hz pendant que Splatoon 1 tourne
- Garde l'arme à travers les respawns / changements de map
- Menu config Aroma avec 4 catégories :
  - **General** → on/off master switch
  - **Main weapon** (94 entrées : 0x00 à 0x67 + "No Weapon")
  - **Sub weapon** (10 entrées : Splat Bomb, Suction Bomb, Burst Bomb, Seeker, Point Sensor, Ink Mine, Sprinkler, Squid Beakon, Splash Wall, Disruptor, None)
  - **Special weapon** (7 entrées : Inkstrike, Killer Wail, Bubbler, Kraken, Echolocator, Bomb Rush, Inkzooka, None)
- Toggle on/off par catégorie (tu peux override juste le main, juste le sub, etc.)
- Persistance sur la SD (`sd:/wiiu/plugins/config/splat_weapon_changer/`)

## Build via GitHub Actions (recommandé — zéro setup local)

Le repo contient `.github/workflows/build.yml` qui build automatiquement le `.wps` à chaque push sur `main`/`master`, à chaque PR, et sur tag `v*` (crée aussi une Release GitHub).

### Setup

1. Crée un nouveau repo sur GitHub (public ou private)
2. Push le contenu de ce dossier :
   ```bash
   cd splat-weapon-changer
   git init
   git add .
   git commit -m "Initial commit: Splat Weapon Changer Aroma plugin"
   git branch -M main
   git remote add origin https://github.com/TON_PSEUDO/splat-weapon-changer.git
   git push -u origin main
   ```
3. Va sur l'onglet **Actions** de ton repo sur GitHub
4. Tu verras le workflow "Build Wii U Aroma Plugin" tourner. ~2-3 min plus tard, c'est fini.
5. Clique sur le dernier run → scroll tout en bas → **Artifacts** → télécharge `splat-weapon-changer-wps.zip`
6. Dézippe → tu as `SplatWeaponChanger.wps` prêt à copier sur ta SD

### Pour release une version

```bash
git tag v1.0.0
git push origin v1.0.0
```

Le workflow va aussi créer une GitHub Release automatique avec le `.wps` attaché.

## Build local (alternative)

Si tu veux build sur ta machine (Linux/WSL) :

```bash
# Setup devkitPro (one-shot, nécessite sudo)
./build.sh setup

# Build
./build.sh build
```

Ou utilise le Docker image directement :
```bash
docker run --rm -v "${PWD}:/project" wiiuenv/cmlwut:20230522 make -C /project clean all
```

## Installation sur la Wii U

1. Copie `SplatWeaponChanger.wps` vers :
   ```
   sd:/wiiu/environments/aroma/plugins/SplatWeaponChanger.wps
   ```
2. Vérifie que tu as `WiiUPluginLoaderBackend` et `NotificationModule` dans `sd:/wiiu/environments/aroma/modules/` (inclus par défaut dans le package Aroma base).
3. Insère la SD, boot Aroma.

## Utilisation (100% console, no PC)

1. Depuis le Wii U Menu, ouvre le **Aroma Plugin Config** (icône roue crantée en bas du menu, ou app "Patchouli" si installée)
2. Liste de tes plugins → sélectionne **"Splat Weapon Changer"**
3. Configure :
   - **General** → "Plugin enabled" = true
   - **Main weapon** → "Override main weapon" = true, puis choisis ton arme dans la liste déroulante
   - Idem pour sub et special si tu veux
4. Appuie **B** pour sauvegarder et revenir
5. Lance Splatoon 1 → ton Inkling porte l'arme choisie, même si tu ne l'avais pas débloquée

## Notes techniques

### Offsets mémoire (Splatoon 1 v2.12.0 — dernière version, jamais re-patchée)

| Champ | Offset | Source |
|-------|--------|--------|
| Base pointer | `0x106E46E8` | Code "Live Weapon ID Changer" de Red (SHRL-Red/Splatoon-HaxPack) |
| Player struct offset | `+0x290` | Idem |
| Main weapon ID | `+0x78` | Idem |
| Sub weapon ID | `+0x7C` | Code "Live Sub Weapon ID Changer" de Red |
| Special weapon ID | `+0x80` | **Présomption** (struct ordering, 4 octets après sub) |
| Ink tank variant | `+0x88` | Code "Ink Tank Modifier" de Red (non utilisé par ce plugin) |

⚠️ Si tu es sur **version US ou JP**, le base pointer peut différer. Le code assume EUR par défaut. Si l'arme ne change pas, c'est la première chose à vérifier.

### Sécurité runtime

- Le plugin check les bornes `0x10000000`..`0x50000000` avant chaque write → pas de crash pendant les transitions d'écran
- Thread sur **CPU2** pour ne pas empiéter sur l'audio / UI
- Si tu décoches "Override" pour une catégorie, le plugin n'y touche plus → tu retrouves ton arme normale

### Compatibilité

- **Pretendo Network** : marche en online. Évite les setups toxiques en ranked.
- **Offline / Story mode** : OK, parfait pour tester des armes non débloquées.
- **Local multiplayer (The Shoal)** : OK.

## Structure du repo

```
splat-weapon-changer/
├── .github/
│   └── workflows/
│       └── build.yml          # GitHub Actions: auto-build du .wps
├── src/
│   ├── main.cpp               # Plugin: lifecycle, memory poking, config menu
│   └── weapons.h              # Weapon ID tables (main/sub/special)
├── .gitignore
├── Makefile                   # Build standard WUPS (inclut wups_rules)
├── build.sh                   # Helper local (setup devkitPro + build)
└── README.md                  # Ce fichier
```

## Plan B : .gctu (si tu veux vraiment pas compiler)

Si tu veux pas utiliser GitHub Actions ni installer devkitPro :

1. Sur un PC (une fois) : lance JGecko U, ouvre `AGME01.xml` de Red (https://github.com/SHRL-Red/Splatoon-HaxPack), sélectionne les codes "Live Weapon ID Changer" + "Live Sub Weapon ID Changer", **Code → Export to .gctu**
2. Copie le fichier vers : `sd:/codes/0005000010162200.gctu` (EUR) / `0005000010112300.gctu` (US) / `0005000010110F00.gctu` (JP)
3. Installe **TCPGecko-Plugin** de PinkDiamondTeam dans `sd:/wiiu/environments/aroma/plugins/`
4. Active **"Enable SD cheats"** dans la config du plugin TCPGecko
5. Lance Splatoon → codes chargés auto

⚠️ Avec .gctu, l'arme est figée dans le fichier (tu peux pas la changer depuis la console). Le plugin compilé reste plus souple.

## Crédits

- **Codes mémoire & weapon IDs** : Red (SHRL-Red) — https://github.com/SHRL-Red/Splatoon-HaxPack
- **TCPGecko-Plugin Aroma** (référence d'implémentation) : PinkDiamondTeam — https://github.com/PinkDiamondTeam/TCPGecko-Plugin
- **WUPS / Aroma framework** : wiiu-env / Maschell — https://github.com/wiiu-env
- **Plugin code** : généré par GLM (Z.ai)

## Licence

MIT — fais ce que tu veux avec, juste pas de responsabilité si tu bricks ta Wii U (ça n'arrivera pas, c'est un plugin userspace).
