# Contrôles — Thrustmaster T.16000M FCS + TWCS Throttle

Document de travail pour le mapping des commandes du simulateur de mecha.
Colonne **Action** à remplir au fur et à mesure de la conception du gameplay.

> ⚠️ Numérotation DirectInput standard (telle que vue dans `joy.cpl` / Unreal Raw Input).
> À vérifier une fois le matériel branché : Panneau de configuration → *Contrôleurs de jeu* → Propriétés.
> La numérotation des boutons du socle du joystick (5-16) est « en tour de grappe » et non en ordre de lecture.

---

## 1. Joystick — T.16000M FCS (main droite)

### 1.1 Axes

| Axe (DirectInput) | Contrôle physique                     | Type        | Action | Notes (courbe, zone morte, inversion) |
|-------------------|---------------------------------------|-------------|--------|----------------------------------------|
| X                 | Manche gauche / droite                | Analogique  | Rotation du buste (vitesse) | ±60°/s ; au-delà de 60° buste/hanches, les jambes suivent (le mecha tourne) ; secours clavier A/E |
| Y                 | Manche avant / arrière                | Analogique  | Tête : haut/bas (position absolue) | ±45°, avant = regarder en bas |
| Rz                | Torsion du manche (twist)             | Analogique  | Tête : gauche/droite, torsion du cou (position absolue) | ±90° |
| Slider            | Molette/levier sur le socle           | Analogique  |        |                                        |

### 1.2 Chapeau (POV)

| Contrôle                | Direction      | Action | Notes |
|-------------------------|----------------|--------|-------|
| Hat 8 directions (tête) | Haut           |        |       |
|                         | Haut-Droite    |        |       |
|                         | Droite         |        |       |
|                         | Bas-Droite     |        |       |
|                         | Bas            |        |       |
|                         | Bas-Gauche     |        |       |
|                         | Gauche         |        |       |
|                         | Haut-Gauche    |        |       |

### 1.3 Boutons de la poignée

| Bouton | Emplacement                                   | Action | Notes |
|--------|-----------------------------------------------|--------|-------|
| B1     | Gâchette (index)                              |        |       |
| B2     | Tête — bouton pouce (bas, côté gauche)        |        |       |
| B3     | Tête — bouton haut gauche                     |        |       |
| B4     | Tête — bouton haut droit                      |        |       |

### 1.4 Boutons du socle

Disposition (vue de dessus, pilote face au joystick) :

```
      SOCLE GAUCHE              SOCLE DROIT
   [ 5 ] [ 6 ] [ 7 ]        [13 ] [12 ] [11 ]
   [10 ] [ 9 ] [ 8 ]        [14 ] [15 ] [16 ]
```

| Bouton | Emplacement                     | Action | Notes |
|--------|---------------------------------|--------|-------|
| B5     | Socle gauche — rangée haute, 1  |        |       |
| B6     | Socle gauche — rangée haute, 2  |        |       |
| B7     | Socle gauche — rangée haute, 3  |        |       |
| B8     | Socle gauche — rangée basse, 3  |        |       |
| B9     | Socle gauche — rangée basse, 2  |        |       |
| B10    | Socle gauche — rangée basse, 1  |        |       |
| B11    | Socle droit — rangée haute, 3   |        |       |
| B12    | Socle droit — rangée haute, 2   |        |       |
| B13    | Socle droit — rangée haute, 1   |        |       |
| B14    | Socle droit — rangée basse, 1   |        |       |
| B15    | Socle droit — rangée basse, 2   |        |       |
| B16    | Socle droit — rangée basse, 3   |        |       |

---

## 2. Manette des gaz — TWCS Throttle (main gauche)

### 2.1 Axes

| Axe (DirectInput) | Contrôle physique                          | Type        | Action | Notes (courbe, zone morte, inversion) |
|-------------------|--------------------------------------------|-------------|--------|----------------------------------------|
| Z                 | Manette des gaz principale                 | Analogique  | Réacteurs dorsaux avant (0-100 %) | ≤ 55 % : le mecha court (jusqu'à ~57 km/h, le double de la marche) ; > 55 % : porté par les réacteurs (~2,5 m du sol, ~160 km/h max). Poussé vers l'avant = puissance. Armement : ramener à 0 |
| X                 | Mini-stick gauche / droite                 | Analogique  | Déplacement latéral (pas chassés) | Natif, 60 % de la vitesse de marche (~4,7 m/s) ; secours clavier ←/→ |
| Y                 | Mini-stick haut / bas                      | Analogique  | Déplacement avant/arrière | Natif, marche seulement : ~7,9 m/s (28 km/h) avant, moitié en arrière ; secours clavier ↑/↓ |
| Rz                | Palonnier à bascule (rocker) sous la poignée | Analogique | Réacteurs dorsaux latéraux (±100 %) | Droite = positif |
| Slider 0          | Molette rotative « antenne »               | Analogique  | Réacteurs dorsaux verticaux (0-100 %) | ≤ 80 % : mecha « moins lourd » ; > 80 % : décolle. Armement : ramener à 0 |

> Axes supplémentaires uniquement si un palonnier Thrustmaster (TFRP/TPR) est branché sur le TWCS :
> Rx (frein droit), Ry (frein gauche), Slider 1 (palonnier). Le rocker Rz peut alors être désactivé.

### 2.2 Boutons

| Bouton | Emplacement                                     | Action | Notes |
|--------|-------------------------------------------------|--------|-------|
| B1     | Bouton pouce (grand bouton)                     |        |       |
| B2     | Bouton auriculaire                              | Verrouiller / déverrouiller le torse | Verrouillé : le buste garde son angle avec les jambes, le joystick X fait tourner tout le mecha. Clavier F |
| B3     | Bouton annulaire                                | Aligner le torse sur les jambes | Fonctionne même verrouillé. Clavier G |
| B4     | Bascule majeur — haut                           |        |       |
| B5     | Bascule majeur — bas                            |        |       |
| B6     | Clic du mini-stick (appui)                      |        |       |

### 2.3 Chapeaux

| Contrôle                         | Direction | Bouton | Action | Notes |
|----------------------------------|-----------|--------|--------|-------|
| Hat 4 directions — milieu        | Haut      | B7     |        |       |
|                                  | Droite    | B8     |        |       |
|                                  | Bas       | B9     |        |       |
|                                  | Gauche    | B10    |        |       |
| Hat 4 directions — bas           | Haut      | B11    |        |       |
|                                  | Droite    | B12    |        |       |
|                                  | Bas       | B13    |        |       |
|                                  | Gauche    | B14    |        |       |
| Hat 8 directions — pouce (POV)   | Haut      | POV    |        |       |
|                                  | Haut-Droite | POV  |        |       |
|                                  | Droite    | POV    |        |       |
|                                  | Bas-Droite | POV   |        |       |
|                                  | Bas       | POV    |        |       |
|                                  | Bas-Gauche | POV   |        |       |
|                                  | Gauche    | POV    |        |       |
|                                  | Haut-Gauche | POV  |        |       |

---

## 3. Modificateurs / couches (optionnel)

Un bouton peut servir de « Shift » pour doubler les actions disponibles.

| Modificateur (bouton) | Nom de la couche | Description |
|-----------------------|------------------|-------------|
|                       |                  |             |

---

## 4. Récapitulatif matériel

| Périphérique      | Axes | Boutons | Chapeaux                        |
|-------------------|------|---------|---------------------------------|
| T.16000M FCS      | 4    | 16      | 1 × 8 directions (POV)          |
| TWCS Throttle     | 5 (+3 avec palonnier) | 14 | 1 × 8 dir. (POV) + 2 × 4 dir. (boutons 7-14) |
| **Total**         | **9** | **30** | 2 POV 8 dir. (+ 2 hats 4 dir. déjà comptés en boutons) |

---

## 5. Clavier — console de cockpit

Inspiration **Gundam SEED** : le pilote pilote avec les deux manches (HOTAS) et utilise un
**clavier de cockpit** pour reconfigurer le mecha en cours de mission, y compris en combat.
Le clavier n'est donc **pas un fallback** : il fait partie intégrante du gameplay.

Le clavier sert aussi à **créer et configurer les routines** (voir `Routines.md`), via une
interface de terminal rétrofuturiste sans souris.

> Le mapping HOTAS décrit dans ce document est celui du **jeu de routines par défaut** :
> ce sont les routines qui relient les entrées au mecha, et elles peuvent le modifier en
> jeu (ex. passage en mode tir). D'autres jeux de routines permettront la manette ou le
> clavier/souris sans modifier le moteur.
>
> Chaque bouton peut activer des routines via le kernel sur trois événements : `onPress`,
> `onRelease` et `pressed` (appui bref < 200 ms par défaut), avec une commande On, Off, Switch ou Impulsion
> (voir `Routines.md` §4). Les axes peuvent être découpés en boutons virtuels (points de
> bascule) ou déclencher sur `onChange`.

Pistes de fonctions (à préciser) :

| Domaine                         | Exemples d'actions                                        | Touche(s) | Notes |
|---------------------------------|-----------------------------------------------------------|-----------|-------|
| Vue debug (hors jeu final)      | Vue troisième personne derrière le buste ; commande `baton` = afficher le bonhomme bâton animé, `jambes [force amort.]` = suivi physique des jambes | V         | Debug uniquement |
| Console de configuration        | Ouvrir la console ; `exit` la ferme, `quit` quitte le jeu | Espace    | Implémenté (écran du cockpit) |
| Répartition de puissance        | Moteurs / armes / systèmes / boucliers                    |           |       |
| Refroidissement                 | Priorité de refroidissement par bloc, purge thermique     |           |       |
| Gestion des dégâts              | Isoler un bloc, couper une articulation, rerouter         |           |       |
| Configuration des armes         | Groupes de tir, modes, affectation main gauche/droite     |           |       |
| Paramètres de mouvement         | Limites de couple, courbes de réponse, stabilisation      |           |       |
| Capteurs / HUD                  | Modes radar, filtres, pages d'affichage                   |           |       |

## 6. Souris

À définir (éventuellement navigation dans les écrans du cockpit).
