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

## 2. Gyroscope principal et stabilité

> **Révision après le premier prototype jouable** : la simulation ne doit pas prendre le pas sur le gameplay.
> Les « gyroscopes secondaires » (stabilisateurs par bloc) **n'existent plus**. Le pilote ne contrôle plus
> l'équilibre : il contrôle le **mouvement**.

### 2.1 Un mecha stable par défaut

- Le mecha est stable **au même titre qu'un personnage d'un autre jeu** : il tient debout seul et pose ses pieds
  pour suivre son corps.
- Les **centres de gravité par bloc** et l'**inertie** (masse, accélération limitée) sont conservés.
- En mouvement, le mecha **se penche de manière cohérente** : c'est de l'**animation** (quasi nulle en marche,
  nette en course, légère anticipation à l'accélération), même si elle déplace réellement le centre de gravité.

### 2.2 Gyroscope principal = deux stats

Le gyroscope principal est un **composant du torse** (il peut être détruit, consomme de l'énergie et produit de la
chaleur), mais ce n'est **plus un vrai gyroscope simulé** : il se résume à deux statistiques.

| Stat          | Plage     | Rôle                                                                             |
|---------------|-----------|----------------------------------------------------------------------------------|
| **Stabilité** | 1 … N     | Diviseur des forces extérieures appliquées au torse                              |
| **Inertie**   | ≥ 0 (kN)  | Force minimale (après division) pour que le torse bouge                          |

Règle : quand le mecha subit une **force extérieure** (gravité, impact…) qui devrait faire bouger son torse :

```
F_effective = F / Stabilité
si |F_effective| <= Inertie  -> absorbée, le torse ne bouge pas
sinon                       -> seule la part (F_effective - Inertie) fait bouger le torse
```

- Seul le **torse** compte : c'est le « vrai » mecha. Un bras écarté de force, par exemple, n'entre pas en jeu.
- Le **pilotage** (déplacement, rotation, tête) n'est **pas** une force extérieure.
- **Gravité** : si le centre de gravité sort du polygone d'appui, le couple de bascule (ramené à une force
  horizontale) passe par la même règle ; s'il n'est pas absorbé, le mecha tombe.
- **Impact** : l'excédent donne un recul (Δv = excédent × durée / masse), le torse encaisse puis revient ; au-delà
  d'un recul seuil, le mecha tombe.
- **Gyroscope détruit** : stabilité 1, inertie 0 (plus aucun filtrage).

Valeurs du prototype : stabilité 4, inertie 150 kN (un impact de 400 kN est absorbé, 5 000 kN fait reculer,
40 000 kN fait tomber). Commandes terminal de test : `gyro`, `impact <kN> [direction]`.
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

> Ancien exemple (marche par déséquilibre volontaire via le gyroscope) : **abandonné** après le premier prototype, le pilote contrôle désormais directement le mouvement.

---

## 4. Pistes Unreal Engine 5

- Centre de gravité par bloc : **Center of Mass offset** de chaque corps du Physics Asset.
- Bloc combiné (prise universelle) : arme attachée au corps de la main (ou du torse) en
  soudant les corps, ou calcul du centre de gravité combiné côté code.
- Centre de gravité global : calculé à chaque tick (somme pondérée), utile pour le HUD et
  les routines.
- Gyroscope principal : deux stats appliquées dans `AMech::ApplyExternalForce` et `AMech::CheckGravity`.
- Inclinaison en mouvement : animation dans `AMech::UpdateLean`.

---

## 5. Décisions prises

| Sujet                       | Décision                                                         |
|-----------------------------|------------------------------------------------------------------|
| Données par bloc            | Poids + centre de gravité défini en dur ; le reste = physique de base |
| Prise universelle           | Centre de gravité combiné = moyenne pondérée par le poids        |
| Modules de centre de gravité | Contrepoids et autres, au cas par cas                           |
| Gyroscope principal         | Composant du torse réduit à deux stats : stabilité (1..N, diviseur) et inertie (seuil) ; filtre les forces extérieures sur le torse |
| Gyroscopes secondaires      | Supprimés : le mecha est stable par défaut, le pilote contrôle le mouvement et non l'équilibre |
| Gyroscope comme composant   | Endommageable/destructible (détruit = stabilité 1, inertie 0), consomme et chauffe (pas encore simulé) |
| Adaptation au torse         | Automatique, limitée à la **zone suiveuse** propre à chaque articulation |
| Zone suiveuse               | Définie en dur par axe et par modèle d'articulation ; modifiable seulement à l'atelier |
| Dépassement de zone suiveuse | Le torse est bloqué ; limite réelle = combinaison des zones de toutes les articulations concernées |

## 6. Questions ouvertes

*(aucune pour le moment)*
