# Routines

Document de travail décrivant le système de **routines** : les scripts embarqués qui
pilotent partiellement le mecha à la place du pilote.

> ⚠️ Tous les exemples de ce document (boutons, noms de routines, verbes, syntaxe, méthode
> de marche) sont **illustratifs** et ne constituent pas des règles de gameplay.

---

## 1. Concept

Une **routine** est un script qui prend en charge une partie du pilotage pour la rendre
plus facile.

Exemple : récupérer l'épée plasma dans son slot. Le slot est fixe et toujours au même
endroit, il est inutile de guider le bras à la main : une routine s'en charge.

Les routines sont **la partie la plus complexe du pilotage** : ce sont de véritables blocs
de programmation, même si le nombre d'appels système (verbes d'action) reste limité.

### 1.1 Risque / récompense

| Beaucoup de routines                         | Peu de routines                              |
|----------------------------------------------|----------------------------------------------|
| Pilotage **plus facile**                     | Pilotage **plus exigeant**                   |
| Mouvements standardisés, prévisibles         | Contrôle **plus précis**, mouvements libres  |

L'équilibre entre les deux est un choix du pilote : c'est une part centrale de son style.

### 1.2 Programmer fait partie du pilotage

La **création et la configuration des routines** fait partie intégrante du « pilotage ».
Elle se fait **au clavier**, via une interface de terminal **rétrofuturiste, sans souris**
(conception de l'interface : plus tard).

### 1.3 Bibliothèque de base

- **Beaucoup de routines sont fournies d'avance**, et toutes sont **configurables**.
- Le joueur peut les modifier ou écrire les siennes.

### 1.4 Indépendance vis-à-vis du périphérique

Le contrôle est pensé aujourd'hui pour le HOTAS Thrustmaster, mais **ce sont les routines
(et leur affectation aux entrées par le kernel) qui font le lien entre les entrées et le
mecha**. Un jeu de routines adapté permet de jouer à la **manette** ou au
**clavier/souris** **sans modifier le moteur**.

→ Le mapping décrit dans `Control.md` est celui du **jeu de routines par défaut pour le HOTAS**.

---

## 2. Structure d'une routine

### 2.1 Trois zones

Il n'existe **qu'un seul type de routine**. Une routine est découpée en trois zones :

| Zone        | Exécution                                          |
|-------------|----------------------------------------------------|
| `INIT`      | Une fois, au lancement de la routine               |
| `PROCESS`   | À chaque cycle, tant que la routine est active     |
| `END`       | Une fois, à l'arrêt de la routine                  |

Ce qu'on pourrait appeler « action », « passive » ou « configuration » n'est qu'une façon
de remplir ces zones :

| Usage (illustratif)                  | INIT                        | PROCESS                   | END                       |
|--------------------------------------|-----------------------------|---------------------------|---------------------------|
| Dégainer une arme                    | Toute la séquence           | *(vide)*                  | *(vide)*                  |
| Maintenir l'équilibre                | Initialisation              | Toute la logique          | *(vide)*                  |
| Passer en mode tir                   | Préparation, ouvrir la caméra | Suivi de visée          | Fermer la caméra          |

Un changement de configuration est un ensemble de commandes système **au même titre**
qu'un mouvement de membre : aucune séparation entre les deux.

Une routine peut **s'arrêter d'elle-même** : elle passe alors directement à `END`.

### 2.2 Blocs, chaînes et branches parallèles

Une routine est composée de **trois arbres de blocs** indépendants : un arbre `INIT`,
un arbre `PROCESS` et un arbre `END` (logique no-code). Dans chaque arbre :

- des blocs **enchaînés** forment une **branche** : chaque bloc attend la fin du précédent ;
- plusieurs branches peuvent s'exécuter **en parallèle**.

Ce découpage est important pour les autorisations (voir §5) : quand un bloc est refusé,
**sa branche meurt**, mais les branches parallèles continuent.

Quand **toutes les branches d'un arbre sont mortes**, l'arbre est considéré comme
**terminé**, et la routine continue normalement :

| Arbre entièrement mort | Conséquence                                                         |
|------------------------|---------------------------------------------------------------------|
| `INIT`                 | Initialisation considérée comme terminée → passage à `PROCESS`      |
| `PROCESS`              | Cycle considéré comme terminé ; les blocs sont **réinitialisés à chaque cycle** → le cycle suivant recommence normalement |
| `END`                  | `END` terminé → **fin de la routine**                               |

Tuer des blocs ne met donc **jamais** fin à une routine, sauf dans `END` (où c'est la fin
normale de toute façon).

### 2.3 Paramètres d'une routine

| Paramètre       | Description                                                          |
|-----------------|----------------------------------------------------------------------|
| Nom             | Identifiant                                                          |
| Priorité        | Valeur numérique, basse = prioritaire (voir §5). Défaut : 10000      |
| `active`        | Booléen piloté par le kernel (voir §4)                               |
| `fullPlaying`   | Demande toutes les autorisations au lancement ; annule tout si une est refusée (voir §5.3) |
| Coût            | Calculé à partir des verbes utilisés (voir §6)                       |

### 2.4 Exemples (syntaxe purement illustrative)

```
ROUTINE  DEGAINE_EPEE                 PRIO 10000
INIT:
    TEST     MAIN_D.LIBRE
    SAUTF    STOP
    CIBLE    BRAS_D, POSE.SLOT_DOS_D
    ATTENDRE BRAS_D.ATTEINT
    SAISIR   MAIN_D, SLOT_DOS_D
    CIBLE    BRAS_D, POSE.GARDE
PROCESS:
END:
```
*Déclenchée par le kernel en **impulsion** : joue `INIT` puis `END`, pas de `PROCESS`.*

```
ROUTINE  MODE_TIR                     PRIO 9000
INIT:
    DESACTIVER VISEE_TETE
    VUE        OUVRIR  CAM_ARME_D
PROCESS:
    LIRE       JOY.XY  -> R1
    CIBLE      BRAS_D.VISEE, R1
END:
    VUE        FERMER  CAM_ARME_D
    ACTIVER    VISEE_TETE
```
*Déclenchée par le kernel en **on/off** sur B5. Sa priorité (9000) est strictement
meilleure que celle de `VISEE_TETE` (10000), ce qui l'autorise à la désactiver (voir §5.4).*

---

## 3. Exécution

### 3.1 Les routines trichent (volontairement)

Les routines **ne passent pas** par le moteur physique ni par les moteurs d'articulation
simulés (voir `Structure.md` §6). Elles agissent **directement sur le rig et sur les
statistiques** du mecha.

**Pourquoi :** alléger le moteur. Pas besoin d'un contrôleur physique capable d'atteindre
précisément un slot d'arme.

### 3.2 … mais restent soumises à l'état du mecha

- Une routine **respecte l'état du mecha** : dégâts, articulations grippées ou molles,
  blocs détruits, surchauffe, énergie disponible.
- Une routine **consomme de l'énergie et produit de la chaleur** exactement comme un
  mouvement manuel.

### 3.3 Les routines visent des cibles, elles ne rejouent pas des animations

Une routine fixe des **objectifs** (ex. « bras gauche en position 2 ») et essaie de les
atteindre en continu. Si le monde perturbe le mecha, la routine **continue d'essayer
depuis la nouvelle situation**.

Exemple : pendant « mettre le bras gauche en position 2 », un coup dans le bras gauche le
dévie. La routine continue de ramener le bras vers la position 2, mais **de plus loin**
qu'elle n'en était → le mouvement prend plus de temps.

### 3.4 Lecture de paramètres en direct

Les routines peuvent **lire des valeurs en temps réel** : entrées du pilote, capteurs,
état des blocs et articulations, températures, énergie…

Exemple (illustratif, méthode de marche non retenue à ce stade) :
- une routine tente en permanence de **maintenir l'équilibre** ;
- le pilote contrôle un **gyroscope interne** et **déséquilibre volontairement** le mecha ;
- pour compenser, la routine est obligée de **faire un pas** → le mecha avance.

---

## 4. Kernel : activation des routines et entrées

Le **kernel** est avant tout un **menu de configuration**. Ce n'est pas réellement un
kernel : il porte ce nom pour le lore et pour sa place dans les règles de priorité
(priorité 0, au-dessus de tout). Il gère notamment :

1. l'**affectation des entrées** (boutons et axes des périphériques) ;
2. l'**activation des routines** (booléen `active` de chaque routine).

### 4.1 Commandes d'activation

Un événement lié par le kernel peut envoyer l'une de ces commandes à une routine :

| Commande      | Effet                                                                    |
|---------------|--------------------------------------------------------------------------|
| **On**        | Active la routine : `INIT`, puis `PROCESS` à chaque cycle                |
| **Off**       | Désactive la routine (voir ci-dessous)                                   |
| **Switch**    | Bascule : On si la routine est inactive, Off si elle est active          |
| **Impulsion** | **On suivi instantanément d'un Off** : la routine joue `INIT` puis `END`, **sans `PROCESS`** |

**Désactivation** (par le kernel ou par une autre routine) : l'étape en cours est toujours
**terminée** avant de jouer `END`.

| Désactivation reçue pendant | Comportement                                                   |
|-----------------------------|----------------------------------------------------------------|
| `INIT`                      | Termine `INIT`, **saute `PROCESS`**, joue `END`                |
| `PROCESS`                   | Termine le cycle en cours, ne lance plus de nouveau cycle, joue `END` |

C'est ce qui fait fonctionner l'impulsion : le Off arrive pendant `INIT`.

### 4.2 Événements d'entrée

Le kernel peut lier l'activation d'une routine à un événement de bouton :

| Événement    | Déclenchement                                                            |
|--------------|--------------------------------------------------------------------------|
| `onPress`    | Au moment où le bouton est enfoncé                                       |
| `onRelease`  | Au moment où le bouton est relâché                                       |
| `pressed`    | Appui bref : press puis release en moins que le **temps de réactivité** (configurable, **200 ms** par défaut) |

Exemple : **« tant que le bouton est appuyé »** = routine **On** sur `onPress` et **Off**
sur `onRelease`.

### 4.3 Axes

Le kernel peut aussi affecter les **axes** (stick, gaz, mini-stick, molettes…) de deux façons :

| Mode              | Fonctionnement                                                        |
|-------------------|-----------------------------------------------------------------------|
| **Axe → bouton(s)** | On définit un **point de bascule** : franchir le seuil se comporte comme un bouton (`onPress`, `onRelease`, `pressed`). **Plusieurs points de bascule = plusieurs boutons virtuels** sur le même axe |
| **`onChange`**    | Déclenché chaque fois que la valeur de l'axe change. Le kernel autorise n'importe quelle commande (On, Off, Switch, Impulsion), même si toutes n'ont pas forcément de sens |

Les routines peuvent par ailleurs **lire la valeur** d'un axe directement (§3.4).

---

## 5. Priorités et autorisations

### 5.1 Valeurs de priorité

**Plus la valeur est basse, plus l'action est prioritaire.**

| Source                                | Priorité par défaut | Notes                                   |
|---------------------------------------|---------------------|-----------------------------------------|
| Kernel (fictif)                       | **0**               | Non simulé, sert à la visualisation et à l'affectation |
| Entrées directes du pilote            | **1000**            | Gérées comme des routines               |
| Routines                              | **10000**           |                                         |

Toutes les priorités sont **configurables individuellement**.

### 5.2 Demande d'autorisation

Avant d'agir, **chaque bloc d'action demande l'autorisation** d'agir sur ce qu'il cible.

- **Autorisé** : le bloc s'exécute.
- **Refusé** (un acteur plus prioritaire agit déjà sur la cible) : le bloc est **annulé**,
  ainsi que **tous les blocs qui s'enchaînent après lui** → **la branche meurt**.
- Un bloc **déjà en cours** d'exécution (ex. un mouvement sur 0,6 s) est **interrompu**
  si un acteur plus prioritaire prend sa cible → sa branche meurt également.
- Les **branches parallèles** ne sont pas affectées : elles continuent de s'exécuter.
- Dans `PROCESS`, la demande est **renouvelée à chaque cycle**.
- **Même priorité** sur la même cible : les deux sont autorisées et s'appliquent **en même
  temps**, avec un résultat potentiellement incohérent. C'est voulu : au pilote de mieux
  gérer ses routines.

### 5.3 Paramètre `fullPlaying`

- Au lancement, la routine demande **toutes** ses autorisations d'un coup.
- Si **une seule est refusée**, la routine entière est **annulée** avant d'avoir agi.
- Ce n'est **pas une garantie absolue** : si une routine plus prioritaire est déclenchée
  **après** le lancement, elle prend quand même le pas. `fullPlaying` est une sécurité ;
  la gestion des routines reste une compétence de pilotage.

### 5.4 Contrôle d'une routine par une autre

Une routine peut **activer ou désactiver** une autre routine **uniquement si elle est
strictement plus prioritaire** qu'elle (valeur strictement plus basse ; **pas en cas
d'égalité**).

---

## 6. Coût et processeur

- Chaque **verbe** a un **coût**.
- Les verbes placés dans `PROCESS` ont un **coût multiplié par un ratio** (ils tournent à
  chaque cycle).
- Le **coût d'une routine** est la somme de ses verbes : c'est un **coût fixe**, calculé
  une fois, pas mesuré en direct.
- Le **processeur** du mecha a une **capacité** exprimée dans la même unité.
  La somme des coûts des routines actives ne peut pas la dépasser.
- Activer une routine qui **dépasse la capacité restante** → **refus**. Aucune routine
  moins prioritaire n'est coupée pour faire de la place : dans le lore, le processeur ne
  connaît la priorité d'une routine qu'à son exécution (simple justification, ce
  comportement n'est pas simulé).
- Le processeur est un **composant choisi au montage du mecha** : plus il est puissant, plus
  il **consomme d'énergie** et **chauffe** (voir `Structure.md`).
- Le nombre de routines **stockées** est illimité.

---

## 7. Verbes d'action

Les **verbes d'action** sont l'équivalent des **appels système** : les primitives que le
moteur met à disposition des routines.

**Règle de développement :** les verbes sont **ajoutés au cas par cas**, selon les besoins.
Tant que le moteur interprète correctement les routines, on avance.

### 7.1 Registre des verbes

| Verbe       | Paramètres              | Effet                                   | Coût | Statut |
|-------------|-------------------------|-----------------------------------------|------|--------|
| *(exemple)* `CIBLE`    | élément, pose           | Fixe l'objectif d'un membre             |      | Idée   |
| *(exemple)* `SAISIR`   | prise, slot             | Fixe l'arme d'un slot sur une prise     |      | Idée   |
| *(exemple)* `LACHER`   | prise, slot (optionnel) | Range / lâche l'arme                    |      | Idée   |
| *(exemple)* `VUE`      | fenêtre, source         | Ouvre/ferme une fenêtre d'affichage     |      | Idée   |
| *(exemple)* `LIRE`     | paramètre               | Lit une valeur en direct                |      | Idée   |
| *(exemple)* `ATTENDRE` | durée / condition       | Pause                                   |      | Idée   |
| *(exemple)* `TEST` / `SAUT` | condition, étiquette | Logique de contrôle                   |      | Idée   |
| *(exemple)* `STOP`     | —                       | Arrête la routine (passe à `END`)       |      | Idée   |
| *(exemple)* `STABILITE` | bloc, niveau           | Règle la stabilité d'un bloc (voir `Equilibre.md`) |  | Idée |
| *(exemple)* `GYRO`     | orientation / vitesse / couple / position | Commande le gyroscope principal |  | Idée |
| *(exemple)* `ACTIVER` / `DESACTIVER` | routine   | Active / désactive une routine moins prioritaire (§5.4) | | Idée |

Statuts possibles : Idée → Spécifié → Implémenté.

---

## 8. Langage et éditeur

- Logique de **blocs**, comme les outils **no-code** modernes…
- … mais avec un **visuel d'assembleur** (terminal rétrofuturiste, clavier uniquement).
- Conception détaillée de l'éditeur : plus tard.

---

## 9. Architecture moteur (pistes UE5)

- **Kernel** (menu de configuration) : table d'affectation entrées → routines (événements
  `onPress` / `onRelease` / `pressed`, points de bascule et `onChange` pour les axes),
  gestion du booléen `active`, refus d'activation si la capacité processeur est dépassée.
- **Arbitre d'autorisations** : reçoit les demandes des blocs, compare les priorités sur
  chaque cible, répond oui/non.
- **Interpréteur** C++ : exécute les zones `INIT` / `PROCESS` / `END`, gère les branches
  sous forme de trois arbres (chaînes et parallèles), tue une branche dont un bloc est
  refusé ou interrompu, considère un arbre terminé quand toutes ses branches sont mortes,
  et réinitialise l'arbre `PROCESS` à chaque cycle.
- Chaque verbe = une fonction native enregistrée dans un registre, avec son coût.
- Les routines agissent sur le **rig** (Control Rig / poses cibles) et sur les **stats** du
  mecha, en contournant la simulation physique des articulations, mais en respectant
  l'état du mecha et en consommant énergie et chaleur.
- Les perturbations externes (impacts) modifient la pose réelle ; les routines continuent
  de viser leur objectif depuis cette pose.

---

## 10. Décisions prises

| Sujet                         | Décision                                                          |
|-------------------------------|-------------------------------------------------------------------|
| Types de routines             | Un seul type, découpé en `INIT` / `PROCESS` / `END`               |
| Structure interne             | Graphe de blocs : chaînes et branches parallèles                  |
| Respect de l'état du mecha    | Oui                                                               |
| Énergie / chaleur             | Consommées/produites comme un mouvement manuel                    |
| Impact pendant une routine    | La routine continue de viser son objectif depuis la nouvelle position |
| Priorités                     | Valeur basse = prioritaire ; kernel 0, entrées pilote 1000, routines 10000 ; configurables |
| Autorisations                 | Chaque bloc demande l'autorisation ; refus = la branche meurt ; renouvelée à chaque cycle dans `PROCESS` |
| Conflit à priorité égale      | Les actions s'appliquent en même temps (résultat incohérent possible) |
| `fullPlaying`                 | Toutes les autorisations au lancement, sinon annulation ; pas une garantie absolue |
| Activation                    | Booléen `active` géré par le kernel, en on/off ou en impulsion    |
| Entrées                       | Kernel : `onPress`, `onRelease`, `pressed` (seuil 200 ms configurable) |
| Arrêt autonome                | Une routine peut s'arrêter d'elle-même                            |
| Coût                          | Somme des coûts des verbes (ratio pour `PROCESS`), coût fixe      |
| Processeur                    | Capacité dans la même unité, choisi au montage, consomme et chauffe |
| Stockage                      | Illimité                                                          |
| Bibliothèque                  | Nombreuses routines fournies et configurables                     |
| Périphériques                 | Support manette / clavier-souris via les routines                 |
| Langage                       | Blocs type no-code avec un visuel d'assembleur                    |
| Lecture en direct             | Les routines lisent entrées, capteurs et états en temps réel      |
| Kernel                        | Menu de configuration (nom lié au lore et à la priorité 0)        |
| Bloc en cours                 | Interrompu si un acteur plus prioritaire prend sa cible           |
| Structure en arbres           | Trois arbres (`INIT`, `PROCESS`, `END`) ; arbre entièrement mort = étape terminée, la routine continue (fin seulement après `END`) |
| Désactivation                 | L'étape en cours se termine, puis `END` (pendant `INIT` : `PROCESS` est sauté) |
| Impulsion                     | = On suivi instantanément d'un Off                                |
| Commandes kernel              | On, Off, Switch, Impulsion ; `onChange` accepte toutes les commandes |
| Routine → routine             | Activer/désactiver seulement si strictement plus prioritaire      |
| Dépassement processeur        | Refus d'activation                                                |
| Axes                          | Affectables en boutons virtuels (points de bascule) ou en `onChange` |

## 11. Questions ouvertes

*(aucune pour le moment)*
