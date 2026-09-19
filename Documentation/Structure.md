# Structure du mecha

Document de travail décrivant la composition physique du mecha : blocs, articulations,
énergie, thermique, localisation des dégâts et modèle physique simplifié.

Objectif : une **physique crédible** (masse, longueur, vitesse ont un vrai impact sur le
comportement et les dégâts), sans viser une simulation exacte. Une approximation
satisfaisante et lisible pour le joueur prime sur la précision.

> Ordre de grandeur de référence (Gundam SEED, GAT-X105 Strike) : ~17,7 m, ~65 t.

---

## 1. Blocs

Le mecha est un assemblage de **blocs rigides** reliés par des **articulations**.

### 1.1 Liste des blocs

| #  | Bloc               | Qté | Côté | Parent        | Notes                                              |
|----|--------------------|-----|------|---------------|----------------------------------------------------|
| 1  | Torse              | 1   | —    | *(racine)*    | **Cockpit + générateur principal.** Détruit = mort du pilote = fin de partie |
| 2  | Cou                | 1   | —    | Torse         | Surélève le pivot de la tête au-dessus du torse    |
| 2b | Tête               | 1   | —    | Cou           | Caméra et autres capteurs                          |
| 3  | Backpack           | 1   | —    | Torse         | **Propulseurs principaux**. Extensions ignorées pour l'instant |
| 4  | Bassin             | 1   | —    | Torse         | Porte les jambes                                   |
| 5  | Épaule             | 2   | G/D  | Torse         |                                                    |
| 6  | Bras               | 2   | G/D  | Épaule        |                                                    |
| 7  | Avant-bras         | 2   | G/D  | Bras          |                                                    |
| 8  | Main               | 2   | G/D  | Avant-bras    | **Prise universelle** : pas de doigts, l'arme se clipse directement |
| 9  | Cuisse             | 2   | G/D  | Bassin        |                                                    |
| 10 | Jambe              | 2   | G/D  | Cuisse        |                                                    |
| 11 | Pied               | 2   | G/D  | Jambe         |                                                    |
| —  | Extension backpack | —   | —    | Backpack      | **Hors périmètre pour le moment**                  |

Total : **18 blocs** (cou ajouté lors du premier prototype).

### 1.2 Hiérarchie

Le **torse est la racine** : il contient le pilote et le générateur principal, tout le
reste du mecha en dépend.

```
Torse (racine) — cockpit, générateur principal
├── Cou ── Tête
├── Backpack — propulseurs principaux
│   ├── (prises de rangement ?)
│   └── (extensions — plus tard)
├── Épaule G ── Bras G ── Avant-bras G ── Main G ── [Arme]
├── Épaule D ── Bras D ── Avant-bras D ── Main D ── [Arme]
└── Bassin
    ├── Cuisse G ── Jambe G ── Pied G
    └── Cuisse D ── Jambe D ── Pied D
```

Règles :
- Si un bloc est **détruit / arraché**, toute sa descendance est perdue avec lui
  (ex. bras arraché → avant-bras, main et arme perdus ; bassin arraché → les deux jambes).
- **Destruction du torse = mecha hors combat = fin de partie.** Tout autre bloc peut être
  perdu sans mettre fin à la partie.
  - *Si une campagne existe un jour (non prioritaire)* : torse détruit hors cockpit = mecha
    hors combat ; sous-zone cockpit détruite = pilote mort = fin de campagne.
- Les blocs détruits tombent au sol comme **débris physiques** : bonus de réalisme, pas
  une fonctionnalité prioritaire.

### 1.3 Propriétés d'un bloc

| Propriété                 | Unité      | Rôle                                                    |
|---------------------------|------------|---------------------------------------------------------|
| Masse                     | t          | Inertie, dégâts d'impact, consommation des moteurs      |
| Longueur (axe principal)  | m          | Bras de levier, vitesse en bout de membre               |
| Centre de masse           | m (local)  | Défini en dur par le type de bloc ; inertie, équilibre (voir `Equilibre.md`) |
| Points de structure (PS)  | —          | Résistance interne ; à 0 → bloc détruit                 |
| Armure                    | —          | Absorption / seuil avant dégâts à la structure          |
| Sous-zones                | —          | Localisation fine des dégâts (voir §5)                  |
| Composants internes       | —          | Ce que le bloc contient (voir §3)                       |
| Données thermiques        | —          | Capacité thermique, dissipation, conduction (voir §4)   |
| Prises universelles       | —          | Points de fixation d'armes/équipement (voir §1.5)       |

### 1.4 Valeurs par bloc (à remplir)

| Bloc        | Masse (t) | Longueur (m) | PS | Armure | Composants internes                 |
|-------------|-----------|--------------|----|--------|-------------------------------------|
| Torse       |           |              |    |        | Cockpit, générateur principal, gyroscope principal |
| Tête        |           |              |    |        |                                     |
| Backpack    |           |              |    |        | Propulseurs principaux              |
| Bassin      |           |              |    |        |                                     |
| Épaule      |           |              |    |        |                                     |
| Bras        |           |              |    |        |                                     |
| Avant-bras  |           |              |    |        |                                     |
| Main        |           |              |    |        | Prise universelle                   |
| Cuisse      |           |              |    |        |                                     |
| Jambe       |           |              |    |        |                                     |
| Pied        |           |              |    |        |                                     |

### 1.5 Prises universelles et armes

- Une **prise universelle** est un point de fixation standard. Les **mains** en sont une ;
  d'autres prises servent au **rangement** des armes sur le corps (emplacements au cas par cas).
- Une arme est **toujours un monobloc rigide**, sans articulation, **solidaire** de la
  prise sur laquelle elle est accrochée (main ou prise de rangement).
- L'arme a ses propres **sous-zones de dégâts** (répartition des dégâts reçus), mais
  aucune articulation.

### 1.6 Personnalisation

Les blocs sont **réparables et remplaçables entre les missions** : c'est la base de la
personnalisation du mecha. Cela implique des blocs interchangeables avec des
caractéristiques différentes (masse, PS, armure, composants, thermique…).

Les **articulations** sont elles aussi des composants interchangeables **à l'atelier** :
chaque modèle a ses propres valeurs en dur (limites, couple, zone suiveuse par axe…).

---

## 2. Articulations

Chaque articulation a **sa propre résistance**, indépendante des blocs qu'elle relie.
Une articulation peut être endommagée alors que les deux blocs sont intacts.
Chaque articulation embarque **son propre moteur** (voir §3).

### 2.1 Liste des articulations

| Articulation       | Qté | Relie                   | Degrés de liberté (proposition)       |
|--------------------|-----|-------------------------|----------------------------------------|
| Taille             | 1   | Torse ↔ Bassin          | Rotation (lacet) + flexion             |
| Cou                | 1   | Torse ↔ Tête            | Lacet + tangage                        |
| Fixation backpack  | 1   | Torse ↔ Backpack        | Fixe (0 DDL), mais résistance propre   |
| Clavicule          | 2   | Torse ↔ Épaule          | Faible amplitude (haussement)          |
| Épaule             | 2   | Épaule ↔ Bras           | Rotule 3 DDL                           |
| Coude              | 2   | Bras ↔ Avant-bras       | Charnière 1 DDL (+ rotation avant-bras ?) |
| Poignet            | 2   | Avant-bras ↔ Main       | 2–3 DDL                                |
| Hanche             | 2   | Bassin ↔ Cuisse         | Rotule 3 DDL                           |
| Genou              | 2   | Cuisse ↔ Jambe          | Charnière 1 DDL                        |
| Cheville           | 2   | Jambe ↔ Pied            | 2 DDL                                  |

Total : **16 articulations**.

> La liaison arme ↔ prise universelle **n'est pas une articulation** : l'arme est solidaire
> de la prise (voir §1.5).

### 2.2 Propriétés d'une articulation

| Propriété                  | Unité     | Rôle                                                        |
|----------------------------|-----------|-------------------------------------------------------------|
| Limites angulaires         | °         | Amplitude de mouvement par axe                              |
| Zone suiveuse              | ° (par axe) | Plage dans laquelle l'articulation s'adapte automatiquement aux mouvements du torse ; fixe par modèle (voir `Equilibre.md` §2.1) |
| Couple max (moteur)        | kN·m      | Capacité à mouvoir/retenir la chaîne en aval                |
| Vitesse angulaire max      | °/s       | Vitesse de rotation plafond                                 |
| Consommation électrique    | kW        | Tirée du générateur (voir §3)                               |
| Points de structure (PS)   | —         | Résistance de l'articulation                                |
| Seuil de contrainte        | kN·m      | Au-delà, l'articulation encaisse des dégâts (choc, surcharge) |
| Chaleur générée            | —         | Injectée dans les blocs adjacents (voir §4)                 |

### 2.3 États d'une articulation

| État          | Effet                                                              |
|---------------|--------------------------------------------------------------------|
| Nominale      | Fonctionnement normal                                              |
| Endommagée    | Couple et vitesse réduits, amplitude éventuellement limitée        |
| Grippée       | Bloquée dans sa position actuelle                                  |
| Libre (molle) | Moteur HS : le membre pend et subit la gravité / l'inertie         |
| Rompue        | Le bloc enfant et sa descendance se détachent                      |

---

## 3. Énergie et propulsion

| Élément                   | Emplacement                         | Rôle                                         |
|---------------------------|-------------------------------------|----------------------------------------------|
| Générateur principal      | Torse                               | Source d'énergie principale                  |
| Propulseurs principaux    | Backpack                            | Poussée (saut, dash, vol ?)                  |
| Moteurs d'articulation    | Chaque articulation                 | Mouvement des membres                        |
| Générateurs secondaires   | Membres — **au cas par cas**        | Appoint / autonomie locale                   |
| Propulseurs secondaires   | Membres — **au cas par cas**        | Manœuvre, stabilisation                      |
| Gyroscope principal       | Torse                               | Deux stats (stabilité, inertie) filtrant les forces extérieures sur le torse ; consomme et chauffe (voir `Equilibre.md`) |
| Processeur (ordinateur de bord) | À définir                     | Exécute les routines ; capacité choisie au montage, consomme et chauffe selon sa puissance (voir `Routines.md` §6) |

- La **répartition de puissance** se fait au clavier (console de cockpit, voir `Control.md`).
- Un générateur secondaire **ne maintient pas** en fonctionnement un membre séparé du
  générateur principal : il ne sert que d'appoint.

---

## 4. Thermique

Chaque **bloc a sa propre température**. La chaleur :

1. est **produite** localement (générateur, propulseurs, moteurs d'articulation, armes, impacts) ;
2. est **transmise** aux blocs voisins par conduction à travers les articulations ;
3. est **dissipée** vers l'extérieur (radiateurs, surface, environnement).

Modèle proposé (par pas de temps, pour chaque bloc `i`) :

```
ΔT_i = ( Q_produit_i
       − Q_dissipé_i
       + Σ_voisins k_ij · (T_j − T_i) ) · Δt / C_i
```

- `C_i` : capacité thermique du bloc (≈ proportionnelle à sa masse)
- `k_ij` : conductance thermique de l'articulation entre `i` et `j`
- `Q_dissipé_i` : dissipation propre du bloc (dépend de ses radiateurs, de l'environnement
  et de la priorité de refroidissement choisie au clavier)

Effets de la surchauffe : au cas par cas, selon deux réponses principales :
- **Détérioration** : l'élément encaisse des dégâts ou perd en performance.
- **Coupure de sécurité** : l'élément se désactive pour se protéger.

---

## 5. Localisation des dégâts (sous-zones)

Les sous-zones **ne sont pas des articulations** : elles servent uniquement à savoir *où*
un bloc (ou une arme) a été touché (armure locale, composants internes touchés).

Proposition de découpage (à affiner) :

| Bloc        | Sous-zones proposées                                           |
|-------------|----------------------------------------------------------------|
| Torse       | Avant haut (cockpit ?), avant bas, flanc G, flanc D, arrière  |
| Bassin      | Avant, arrière, flanc G, flanc D                              |
| Tête        | Avant (capteurs), arrière                                     |
| Backpack    | Centre, propulseur G, propulseur D                            |
| Épaule      | Dessus, avant, arrière                                        |
| Bras / Avant-bras / Cuisse / Jambe | Proximal, distal (× avant/arrière ?)   |
| Main        | Unique                                                        |
| Pied        | Avant, talon                                                  |
| Arme        | Selon l'arme (ex. canon, corps, chargeur)                     |

---

## 6. Modèle physique (approximation)

### 6.1 Principe

Chaque bloc est un **corps rigide** avec masse et moment d'inertie ; chaque articulation
est une **contrainte motorisée** avec un couple max. Conséquences naturelles recherchées :

- Une arme lourde au bout d'un bras long **ralentit le mouvement** (inertie plus grande
  pour un même couple moteur).
- Plus un membre est long et rapide, plus **la vitesse en bout de chaîne** est élevée.
- Un coup puissant **sollicite aussi les articulations de l'attaquant** (frapper trop fort
  peut abîmer son propre poignet/coude).

### 6.2 Inertie d'une chaîne (bras)

Moment d'inertie de la chaîne vue depuis l'épaule :

```
I_épaule ≈ Σ (I_i + m_i · r_i²)
```

- `m_i` : masse du bloc i (bras, avant-bras, main, arme)
- `r_i` : distance entre l'épaule et le centre de masse du bloc i
- `I_i` : inertie propre du bloc (barre : m·L²/12)

Accélération angulaire disponible : `α = C_max / I` → vitesse de frappe.

### 6.3 Dégâts d'un coup au corps à corps

1. **Vitesse au point d'impact** `v` : donnée directement par le moteur physique
   (vitesse linéaire du corps au point de contact).
2. **Masse effective** `m_eff` : masse de ce qui frappe, pondérée par la rigidité de la chaîne :

   ```
   m_eff ≈ m_arme + m_main + k · (m_avant-bras + m_bras)      avec k ≈ 0,3–0,5 (à régler)
   ```
3. **Énergie d'impact** :

   ```
   E = ½ · m_eff · v_n²        (v_n = composante de v normale à la surface touchée)
   ```
4. **Dégâts** :

   ```
   Dégâts = E · F_arme − Armure_locale        (minimum 0)
   ```
   `F_arme` : facteur selon le type (tranchant, contondant, perforant…).
5. **Contrecoup** : une fraction de l'impulsion `m_eff · v_n` est renvoyée dans les
   articulations de l'attaquant (poignet → coude → épaule) ; si elle dépasse leur
   seuil de contrainte, elles encaissent des dégâts.

### 6.4 Équilibre

**Point central du gameplay — détaillé dans `Equilibre.md`** (centres de gravité,
gyroscope principal, stabilité des blocs). Le déplacement et l'équilibre sont en grande partie gérés
par des **routines** (logique continue dans leur zone `PROCESS`), tout comme le fait de se relever
(voir `Routines.md`).
Le mecha peut tomber (jambe endommagée, articulation molle, perte d'un bloc, choc violent).
Éléments à prendre en compte : centre de masse global, polygone d'appui des pieds,
couple disponible aux hanches/genoux/chevilles, propulseurs de stabilisation.

### 6.5 Pistes Unreal Engine 5

- Chaque bloc = un corps du **Physics Asset** (Chaos), articulations = **Physics Constraints**
  avec moteurs angulaires (drives) → couple max, limites.
- **Physical Animation** / contrôle hybride : l'animation donne la cible, la physique
  décide si le mecha y arrive (poids, dégâts).
- Collisions de frappe via hit events : récupérer la vitesse au point d'impact et la normale.
- Racine du Physics Asset = **torse** (cohérent avec la hiérarchie).

### 6.6 Animation et locomotion (prototype)

Le mouvement des membres n'est **pas scripté** : un squelette humanoïde invisible (Manny, ×10), le
« bonhomme bâton », joue des animations classiques (idle / marche / jogging / chute). Le vrai mecha
cherche à reproduire cette animation sous contrainte physique :

- **Jambes simulées** (Chaos) et tirées vers la pose animée par le **Physical Animation Component**
  (cibles orientation + position en espace monde, raideur 20 000, amortissement critique 2·√raideur).
  Si une jambe s'écarte de plus de 3 m de sa cible (coincée), elle y est recalée.
- **Haut du corps** : pose animée, tournée autour de la taille par la rotation du buste, l'inclinaison
  en mouvement et les impacts.
- Les **blocs** sont posés sur les os ; le polygone d'appui vient des pieds posés.

Règles de gabarit (longueur de jambe L = 9 m) :

- **Cadence fixe** (la jambe est un pendule) : un pas = 0,53 · π · √(L/g) ≈ **1,6 s**.
  L'**amplitude** varie avec la vitesse (idle → marche → jogging dans le blend space).
- **Marche max** (nombre de Froude 0,7) : v = √(0,7 · g · L) ≈ **7,9 m/s (28 km/h)**. Sans
  réacteurs, le mecha ne fait que marcher ; les réacteurs avant le font courir (élan, amplitude
  au-delà de la marche), la cadence ne monte qu'au-delà de l'amplitude maximale.
- En vol : boucle de chute (jambes pendantes). La transition sol/vol est adoucie par la physique.

---

## 7. Décisions prises

| Sujet                    | Décision                                                                 |
|--------------------------|--------------------------------------------------------------------------|
| Cockpit                  | Dans le torse, avec le générateur principal. Torse = racine de l'arbre   |
| Fin de partie            | Destruction du torse = mort du pilote = fin de partie                    |
| Énergie                  | Générateur principal (torse), propulseurs principaux (backpack), moteurs par articulation, secondaires sur membres au cas par cas |
| Thermique                | Température par bloc, transmission entre blocs, dissipation              |
| Débris                   | Les blocs détruits tombent au sol : bonus de réalisme, non prioritaire    |
| Réparation / remplacement | Oui, entre les missions (personnalisation)                              |
| Armes                    | Monobloc rigide, solidaire de sa prise universelle, sous-zones de dégâts uniquement |
| Équilibre                | Le mecha peut tomber — géré en grande partie par des routines (voir `Routines.md`) |
| Se relever               | Géré par une routine (voir `Routines.md`)                                 |
| Prises de rangement      | Emplacements au cas par cas                                              |
| Sous-zone cockpit        | Sans effet pour l'instant (torse détruit = fin de partie). Ne servirait qu'en cas de campagne — non prioritaire, peut-être jamais |
| Tête                     | Contient la caméra et d'autres capteurs                                  |
| Surchauffe               | Au cas par cas ; deux réponses principales : détérioration ou coupure de sécurité |
| Générateur secondaire    | Ne maintient **pas** un membre séparé du générateur principal            |

## 8. Questions ouvertes

*(aucune pour le moment)*
