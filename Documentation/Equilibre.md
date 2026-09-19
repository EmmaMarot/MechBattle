# Équilibre

Document de travail décrivant la gestion de l'équilibre du mecha : centres de gravité,
gyroscopes et stabilité des membres.

Principe général : on définit des **données simples par bloc**, **le reste est de la
physique de base**.

> Voir aussi : `Structure.md` (blocs, articulations, modèle physique) et `Routines.md`
> (le déplacement et l'équilibre sont en grande partie gérés par des routines).

---

## 1. Centre de gravité

### 1.1 Par bloc

Chaque bloc possède :

| Donnée              | Origine                                             |
|---------------------|-----------------------------------------------------|
| **Poids** (masse)   | Défini par le bloc                                  |
| **Centre de gravité** | **Défini en dur** dans la nature du bloc (position locale fixe) |

### 1.2 Blocs reliés par une prise universelle

Quand deux blocs sont reliés par une **prise universelle** (main + arme, arme rangée sur le
torse, etc.), ils forment un **bloc combiné** dont le centre de gravité est la **moyenne
des deux, pondérée par le poids** :

```
G_combiné = (m₁ · G₁ + m₂ · G₂) / (m₁ + m₂)
m_combiné = m₁ + m₂
```

Exemple (illustratif) : une main de 2 t avec une arme de 6 t fixée → le centre de gravité
du bloc combiné se trouve aux 3/4 du chemin entre le centre de la main et celui de l'arme.

### 1.3 Mecha complet

Le même calcul, étendu à tous les blocs, donne le **centre de gravité global** du mecha :

```
G_mecha = Σ (mᵢ · Gᵢ) / Σ mᵢ
```

Il évolue en permanence avec la posture, les armes tenues ou rangées, et la perte de blocs.

### 1.4 Modules modifiant le centre de gravité

Certains **modules** peuvent déplacer le centre de gravité d'un bloc (ex. **contrepoids
déplaçables**). Traitement **au cas par cas**.

---

## 2. Gyroscopes

### 2.1 Gyroscope principal (torse)

- Situé dans le **torse**.
- **Seul gyroscope entièrement contrôlable.**
- Il définit l'**orientation du torse par rapport à la gravité**.
- Le torse étant la racine du mecha, il sert de **référence à tout le mecha** : les autres
  blocs se positionnent par rapport au torse.

**Commandes possibles** (par le pilote ou les routines, au choix) :

| Type de commande          | Exemple                                              |
|---------------------------|------------------------------------------------------|
| Orientation cible         | « Torse incliné de 10° vers l'avant »                |
| Vitesse de rotation       | « Tourner à 20°/s en lacet »                         |
| Force (couple) de rotation | « Appliquer tel couple en roulis »                  |
| **Position cible XYZ**    | Déplacer le torse lui-même (pas seulement l'orienter) |

> La commande en position s'éloigne de ce que fait réellement un gyroscope : c'est une
> **approximation assumée** pour rendre le gameplay faisable.

**Adaptation des membres** : quand le torse est déplacé (ou orienté), les articulations
**s'adaptent automatiquement** (ex. abaisser le torse → les genoux plient), mais
uniquement dans leur **zone suiveuse**. Chaque articulation a sa propre zone suiveuse,
et **ne s'adapte pas au-delà**.

- **Zone suiveuse fixe par composant** : définie en dur **pour chaque axe** de
  l'articulation, selon le modèle d'articulation. Elle n'est pas réglable en jeu (ni par
  les routines) ; elle se change **à l'atelier**, en changeant de composant, comme les blocs.
- **Dépassement** : si une commande du torse sort de la zone suiveuse d'une articulation,
  **le torse est bloqué** à la limite.
- ⚠️ **Limites combinées** : la limite réelle du torse dépend de la **combinaison** des
  zones suiveuses de toutes les articulations concernées (ex. abaisser le torse sollicite
  hanches, genoux et chevilles des deux jambes ; le torse s'arrête dès que l'une d'elles
  atteint sa limite). À surveiller de près lors de la conception et du réglage.

**Limites** : le gyroscope principal a un **couple max**, **consomme de l'énergie** et
**produit de la chaleur**. Il peut être **endommagé** avec le torse.

### 2.2 Gyroscopes secondaires (blocs) = stabilité

- Chaque bloc dispose d'un « gyroscope » secondaire, mais **ce n'est pas un vrai gyroscope
  dans le lore** : c'est un **stabilisateur** (moteurs d'articulation + IA embarquée) qui
  tient le membre en place. Le terme « gyroscope secondaire » est conservé dans les docs
  pour le moment.
- On ne lui donne **pas de commandes d'orientation** comme au gyroscope principal ; seul
  son **niveau de stabilité** est réglable (par les routines, voir plus bas).
- Par défaut, chaque bloc **tente de maintenir sa position relative au torse**.
- Cette tenue est modélisée par un **paramètre de stabilité**, **porté par le bloc** (pas
  par l'articulation) :

| Situation                                   | Stabilité                                    |
|---------------------------------------------|----------------------------------------------|
| Bloc au repos, position tenue               | Haute : le bloc résiste aux perturbations    |
| Juste avant une action d'un moteur          | **Baisse** pour autoriser le mouvement       |
| Une fois la nouvelle position atteinte      | **Remonte** : la nouvelle position est tenue |

- La **force** de cette tenue varie selon les **réglages**, les **routines**, les
  **contrôles**, etc.
- Les **routines peuvent modifier la stabilité directement** (ex. bras « raide » pour
  parer, « souple » pour amortir un choc).

**Pourquoi par bloc** : des blocs d'extrémité comme la **main** ou le **pied** peuvent avoir
une stabilité **bien plus élevée** que le bras ou la jambe, ce qui **force le reste du
membre à suivre leur mouvement** (la main « mène », le bras suit).

**Le stabilisateur est un composant** de chaque membre : il peut être **endommagé**,
**surchauffer** ou **manquer d'énergie**, ce qui **réduit la stabilité** du bloc.

### 2.3 Lien avec les autres systèmes

- **Impacts** : un choc déplace un membre en fonction de la force reçue face à sa
  stabilité. Une routine qui vise une pose continue ensuite depuis la position déviée
  (voir `Routines.md` §3.3).
- **Routines** : elles agissent sur le rig ; la stabilité est la « raideur » avec laquelle
  le rig tient sa pose face au monde extérieur, et elles peuvent la régler bloc par bloc.
- **État du mecha** : dégâts, surchauffe et manque d'énergie du stabilisateur réduisent la
  stabilité.

---

## 3. Physique de base de l'équilibre

Rappel des règles physiques utilisées (sans règle de gameplay ajoutée) :

- Le mecha est **stable** tant que la **projection verticale de son centre de gravité
  global** reste dans la **zone d'appui** (surface délimitée par les pieds au sol, ou tout
  autre point de contact).
- Si le centre de gravité sort de la zone d'appui, le mecha **bascule**, sauf si quelque
  chose compense : déplacer la zone d'appui (faire un pas), déplacer le centre de gravité
  (posture, bras, contrepoids), ou appliquer une force (propulseurs).
- Les accélérations (marche, coups, recul d'une arme, impacts) s'ajoutent à la gravité et
  déplacent l'équilibre dynamique.

Exemple (illustratif, repris de `Routines.md` §3.4) : le pilote incline le torse avec le
gyroscope principal → le centre de gravité sort de la zone d'appui → une routine
d'équilibre fait un pas pour compenser → le mecha avance.

---

## 4. Pistes Unreal Engine 5

- Centre de gravité par bloc : **Center of Mass offset** de chaque corps du Physics Asset.
- Bloc combiné (prise universelle) : arme attachée au corps de la main (ou du torse) en
  soudant les corps, ou calcul du centre de gravité combiné côté code.
- Centre de gravité global : calculé à chaque tick (somme pondérée), utile pour le HUD et
  les routines.
- Gyroscope principal : contrôle de l'orientation du torse (couple appliqué ou orientation
  cible).
- Stabilité : **raideur / amortissement** des drives de contraintes (ou du Physical
  Animation) de chaque bloc, abaissés pendant un mouvement puis remontés.

---

## 5. Décisions prises

| Sujet                       | Décision                                                         |
|-----------------------------|------------------------------------------------------------------|
| Données par bloc            | Poids + centre de gravité défini en dur ; le reste = physique de base |
| Prise universelle           | Centre de gravité combiné = moyenne pondérée par le poids        |
| Modules de centre de gravité | Contrepoids et autres, au cas par cas                           |
| Gyroscope principal         | Dans le torse, seul entièrement contrôlable, référence de tout le mecha |
| Gyroscopes secondaires      | Pas réels dans le lore : moteurs + IA, non contrôlables, modélisés par un paramètre de stabilité |
| Stabilité                   | Portée par le bloc ; baisse avant une action moteur, remonte une fois la position atteinte ; modifiable par les routines |
| Commandes du gyroscope principal | Orientation cible, vitesse, couple, ou position XYZ (approximation assumée) |
| Limites du gyroscope principal | Couple max, énergie, chaleur ; endommageable avec le torse     |
| Main / pied                 | Peuvent avoir une stabilité très supérieure pour que le membre suive |
| Stabilisateur               | Composant de chaque membre ; dégâts, surchauffe, manque d'énergie réduisent la stabilité |
| Adaptation au torse         | Automatique, limitée à la **zone suiveuse** propre à chaque articulation |
| Zone suiveuse               | Définie en dur par axe et par modèle d'articulation ; modifiable seulement à l'atelier |
| Dépassement de zone suiveuse | Le torse est bloqué ; limite réelle = combinaison des zones de toutes les articulations concernées |

## 6. Questions ouvertes

*(aucune pour le moment)*
