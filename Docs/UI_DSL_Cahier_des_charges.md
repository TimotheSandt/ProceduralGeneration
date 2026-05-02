# UI DSL - Cahier des charges et plan d'implementation

Version cible: 0.2

## 0. Changelog

### 0.2 (en cours)

- Surface du langage refondue dans un style SwiftUI.
- `body { ... }` n'est plus obligatoire : le corps de la vue est le contenu direct du bloc `view`.
- Suppression des qualifiants `let / ref / state` dans la liste de parametres. A leur place : property wrappers `@Observed`, `@Snapshot`, `@State` et `@Binding`.
- Interpolation de chaine `"\(expr)"` ajoutee. Concatenation `+` toujours toleree.
- Acces propriete pour les getters : `window.averageFPS` resout `Get` + lower-camelCase vers `Window::GetAverageFPS()`.
- Ordre des modifieurs : enfants d'abord, modifieurs chaines apres.
- `func name(args) -> T = expression` pour les helpers mono-expression. `@cui-cpp` reste en escape hatch.
- Appels de namespace en notation pointee : `Profiler.averageTime(...)` (le `::` reste accepte).
- Litteraux numeriques : pixels par defaut, plus besoin de suffixe `.px`.
- Cas d'enum a tete pointee : `.topLeft` quand le type est connu du contexte.
- **Modes de capture explicites pour les expressions reactives** : `once expr` capture la valeur une seule fois au moment du `Build()`, `always expr` force une re-evaluation a chaque `Update()`, defaut = reactif (re-evalue uniquement quand une dependance change).
- **Tag `@cui-volatile`** pour marquer les fonctions / methodes C++ qui lisent le temps, un RNG ou un etat global (non-pures). Le precompilateur force leur re-evaluation a chaque tick meme si leurs arguments n'ont pas change. Defaut = pure, cachee sur les arguments.
- **Modifieur `.throttle(period)`** : plafonne le taux de re-evaluation d'un composant et de **tous ses descendants**. Une expression interne ne peut jamais s'updater plus souvent que le `.throttle` de son ancetre le plus restrictif. Periode en secondes (`0.5` ou suffixe explicite `.s`, `.ms`).

Les sections runtime / pipeline (§2, §7, §9, §10, §12, §13, §14, §16-§19, §21, §23, §24, §26) sont conservees telles quelles : seul le langage de surface change.

### 0.1

Premiere version. Surface inspiree de SwiftUI / Kotlin DSL avec qualifiants `let / ref / state` et bloc `body { }`.

## 1. Objectif

Le but est de definir un DSL declaratif pour construire l'UI plus simplement qu'en C++, tout en gardant:

- la structure de rendu actuelle du projet
- le systeme de dirty tracking deja en place
- les composants existants
- la possibilite d'ajouter de nouveaux composants sans re-ecrire le langage

Le DSL ne doit pas remplacer le moteur UI actuel. Il doit compiler vers lui.

## 2. Contraintes du code C++ deja en place

Le DSL doit s'appuyer sur les elements existants:

- `UI::View` cree son arbre une seule fois via `Build()`
- `UI::ComponentBase` gere deja plusieurs niveaux de dirty state
- `UI::TextContent` sait composer du texte statique, des valeurs, des fonctions et des methodes
- `UI::BindValue`, `UI::BindFunc` et `UI::BindMethod` savent detecter les changements et cacher le dernier resultat
- `UI::CreateText`, `UI::CreateLabel`, `UI::CreateComponent` et les factories existantes doivent rester utilisables

Le DSL ne doit pas contourner cette logique. Il doit l'utiliser.

## 3. Principes de conception

1. Declaratif avant tout.
2. Une vue est decrite comme une fonction de ses donnees.
3. Un affichage ne change que si une dependance a change.
4. Les valeurs statiques sont snapshottees une fois.
5. Les valeurs dynamiques sont suivies via comparaison.
6. Les objets bindes dans les methodes font partie des dependances.
7. Les composants futurs doivent pouvoir etre ajoutes sans changer la grammaire de base.
8. Le code genere doit rester lisible et debug-able.
9. La surface du langage doit ressembler a SwiftUI : une vue est une struct dont le corps est l'arbre UI direct, les dependances reactives sont declarees par des property wrappers (`@Observed`, `@Snapshot`, `@State`), les modifieurs s'enchainent apres le bloc d'enfants, le texte se compose par interpolation `"\(expr)"`.

## 4. Format cible

Le DSL doit pouvoir decrire:

- une `view` avec ses property wrappers (dependances reactives, snapshots, etat local)
- des composants enfants imbriques
- des valeurs statiques (literaux, `let` local, `@Snapshot`)
- des valeurs dynamiques (`@Observed`, `@Binding`, `@State`)
- des appels de fonctions, libres ou exposees via namespace
- des appels de methodes sur objet, en forme propriete (getter sans argument) ou en appel explicite
- des modificateurs chaines apres le bloc d'enfants
- de l'interpolation de chaine `"\(expr)"`
- des helpers mono-expression `func name(args) -> T = expr`
- des blocs conditionnels et listes / boucles plus tard

Le format cible suit la convention SwiftUI:

- pas de bloc `body` obligatoire : le corps de la vue est le contenu direct du `view { }`
- les property wrappers `@Observed`, `@Snapshot`, `@State`, `@Binding` declarent les dependances en tete de vue
- les composants imbriques contiennent leurs enfants entre `{ }`, puis les modifieurs sont chaines a la suite via `.modifier(...)`
- les donnees externes ne sont jamais devinees : elles arrivent par un wrapper explicite

## 5. Grammaire minimale

La grammaire 0.2 couvre au minimum:

```txt
file        := import* view_decl+

import      := "import" string_literal

view_decl   := "view" IDENT view_block
view_block  := "{" view_stmt* "}"
view_stmt   := wrapped_field | let_decl | var_decl | func_decl | cpp_block | element

wrapped_field := wrapper "var" IDENT ":" type [ "=" expr ]
wrapper       := "@Observed" | "@Snapshot" | "@State" | "@Binding"

let_decl    := "let" IDENT ("=" expr | ":" type "=" expr)
var_decl    := "var" IDENT ("=" expr | ":" type "=" expr)

func_decl   := "func" IDENT "(" param_list? ")" "->" type "=" expr
param_list  := param ("," param)*
param       := IDENT ":" type [ "=" expr ]

cpp_block   := "@cui-cpp" "{" /* C++ verbatim */ "}"

element     := IDENT call_args? children_block? modifier_chain?
call_args   := "(" arg_list? ")"
arg_list    := arg ("," arg)*
arg         := [ IDENT ":" ] expr
children_block := "{" element* "}"
modifier_chain := ("." IDENT call_args)*

expr        := capture_expr
            |  interp_string
            |  number
            |  duration_literal
            |  bool_literal
            |  enum_case
            |  identifier
            |  property_access
            |  call_expr
            |  binary_expr
            |  paren_expr

capture_expr := ("once" | "always") expr

interp_string := '"' (text_seg | "\(" expr ")")* '"'
enum_case     := "." IDENT
property_access := expr "." IDENT
call_expr   := (expr | namespace_path) call_args
namespace_path := IDENT ("." IDENT | "::" IDENT)*
binary_expr := expr ("+" | "-" | "*" | "/") expr

duration_literal := number ("." ("s" | "ms" | "us" | "ns"))?
```

Notes:

- pas de bloc `body` obligatoire ; le contenu de `view_block` est lu en sequence et l'unique element racine y est attendu
- `@Observed` = pointeur ou reference externe suivie en lecture seule ; declenche un `Bind(...)` au site d'usage
- `@Snapshot` = valeur externe figee a la construction
- `@State` = etat appartenant a la vue ; ecriture marque dirty le sous-arbre
- `@Binding` = pointeur ou reference externe suivie et modifiable ; lecture/ecriture vers une source externe
- `let` = constante locale a la vue
- `var` = variable locale mutable, non persistante et non reactive par defaut
- `const` n'est pas un mot-cle separe en 0.2 ; utiliser `let` pour une constante locale ou `@Snapshot` pour une entree externe figee
- `func` = helper mono-expression ; le bloc `{ ... }` reste reserve a 0.3
- `enum_case` a tete pointee : `.topLeft` resout un cas dont le type est connu via le contexte (parametre, modifieur)
- les modifieurs viennent toujours apres le bloc d'enfants
- les composants sont resolus par nom dans le registre
- l'interpolation de chaine produit une suite de fragments transmis au constructeur variadique de `UI::TextContent`
- `capture_expr` : `once expr` snapshote la valeur lors du `Build()` initial (compile en `BindStatic` ou copie locale) ; `always expr` force la re-evaluation a chaque `Update()` independamment de tout cache d'arguments. Sans prefixe, l'expression est reactive : reevaluee uniquement quand une dependance change.
- `duration_literal` : un nombre nu represente des secondes ; les suffixes `.s`, `.ms`, `.us`, `.ns` permettent une lecture explicite (`0.5`, `0.5.s`, `500.ms` sont equivalents).

## 6. Modeles de donnees

### 6.1 `@Snapshot` - valeur statique

Une valeur `@Snapshot` est capturee une seule fois a la construction de la vue.

Exemples:

- texte litteral passe en parametre
- nombre fixe pour la duree de vie de la vue
- valeur C++ non reactive

Regle:

- ne devient jamais dirty
- compilee vers une copie locale ou `UI::BindStatic(...)`
- le champ `@Snapshot` apparait comme parametre du constructeur C++ genere
- equivalent local : un simple `let X = expr` dans le corps de la vue

### 6.2 `@Observed` - valeur dynamique

Une valeur `@Observed` est un pointeur ou une reference externe suivie en lecture seule. La vue ne possede pas la source et ne doit pas pouvoir la modifier depuis le DSL.

Difference avec `@Binding`:

- `@Observed` lit la source externe actuelle
- `@Observed` detecte les changements de la source externe
- `@Observed` interdit les ecritures directes et les appels de methodes mutables depuis le DSL
- `@Binding` lit et peut aussi ecrire dans la source externe

Regle:

- si `==` dit que la valeur (ou l'objet pointe) a change, elle devient dirty
- apres `apply()` ou evaluation, la snapshot est mise a jour
- si le type ne sait pas se comparer ou se copier, on tombe sur un mode "toujours dirty"
- le champ `@Observed` apparait comme parametre du constructeur C++ genere
- le C++ genere doit privilegier `const T *` ou `const T &` quand c'est possible
- seules les methodes exposees comme lecture seule peuvent etre appelees sur un `@Observed`

Correspondance C++ actuelle:

- `UI::BindDynamic(...)`
- `UI::Bind(this->field, &T::Method, ...)` au site d'appel
- `UI::BindValue::IsDirty()`

### 6.3 `@State` - etat local de vue

Un champ `@State` appartient a la vue. Il est initialise inline, n'apparait pas dans le constructeur, conserve sa valeur entre les updates et participe au dirty tracking.

Il sert aux donnees UI internes, par exemple:

- panneau ouvert/ferme
- onglet selectionne
- valeur courante d'un controle local
- texte en cours de saisie
- compteur interne

Difference avec `let` et `var`:

- `let` est une constante locale, non modifiable et non reactive par elle-meme
- `var` est une variable locale mutable, mais temporaire et non reactive par defaut
- `@State` est mutable, persistant, reactive, et stocke comme membre de la vue generee

Regle:

- seule la vue le modifie
- l'ecriture marque dirty le sous-arbre concerne
- compile vers `DeferredValue<T>` ou un wrapper equivalent
- peut etre passe a une vue enfant sous forme de `@Binding` via une syntaxe du type `$stateName`

Exemple:

```cui
view Counter {
    @State var count: int = 0

    VBox {
        Text("Count: \(count)")
        Button("Add") {
            count = count + 1
        }
    }
}
```

### 6.4 `@Binding` - reference externe modifiable

Une valeur `@Binding` est une reference ou un pointeur externe suivi, comme `@Observed`, mais modifiable depuis la vue.

Elle ne possede pas la source. Elle permet a une vue enfant ou a un composant de lire et modifier une valeur qui appartient ailleurs:

- un `@State` d'une vue parente
- une variable C++ explicitement passee comme binding
- une propriete d'un modele expose en lecture/ecriture

Regle:

- dirty si la source externe change
- dirty si la vue ecrit dans la source
- doit compiler vers un acces non-const (`T *`, `T &`, ou wrapper equivalent)
- peut appeler des methodes mutables explicitement exposees
- ne doit pas etre cree implicitement depuis n'importe quelle expression

Le cas principal est le passage d'un `@State` parent a une vue enfant:

```cui
view Parent {
    @State var volume: float = 0.5

    VBox {
        Slider(value: $volume)
        Text("Volume: \(volume)")
    }
}

view Slider {
    @Binding var value: float
}
```

Etat d'implementation:

- semantique definie en 0.2
- implementation complete reservee a 0.3 si le runtime n'a pas encore le wrapper bidirectionnel

### 6.5 `let`, `var`, `const` - variables locales non reactives

Ces mots ne declarent pas des dependances externes. Ils servent uniquement a organiser le code de la vue.

`let`:

- constante locale
- non modifiable
- peut etre compilee en `constexpr`, `const auto`, ou variable locale simple
- a utiliser pour les valeurs repetees, comme une taille, une couleur ou un label fixe

`var`:

- variable locale mutable
- utile pour des calculs temporaires
- non persistante entre les updates
- ne marque pas l'UI dirty par elle-meme

`const`:

- non retenu comme mot-cle DSL en 0.2
- ferait doublon avec `let`
- si une valeur vient de l'exterieur et doit etre figee, utiliser `@Snapshot`

Exemple:

```cui
view Metrics {
    let scale = 0.45

    Text("FPS")
        .SetTextScale(scale)
}
```

### 6.6 Fonction

Une fonction pure ou impure peut etre utilisee dans une expression. Trois sources :

- helper mono-expression `func name(args) -> T = expr` declare dans la vue
- fonction libre exposee par `@cui-expose`
- methode statique dans un namespace exposee par `@cui-expose`

Regle:

- si un argument est dirty, le resultat est dirty
- si aucun argument n'a change, le dernier resultat est re-utilise
- si la fonction n'a aucun argument, elle reste reevaluee a chaque fois tant que l'implementation C++ actuelle le demande

Correspondance C++ actuelle:

- `UI::Call(...)`
- `UI::BindFunc`

### 6.7 Methode sur objet

Une methode d'objet est une dependance a deux niveaux:

- l'objet lui-meme
- les arguments de la methode

Deux notations dans le DSL :

- forme propriete : `window.averageFPS` resout vers la methode `Window::GetAverageFPS()` exposee
- forme appel : `window.someMethod(arg)` ou `Profiler.averageTime("Render")`

Regle:

- si l'objet a change, le resultat est dirty
- si un argument a change, le resultat est dirty
- si l'objet est copiable et comparable, on garde une snapshot et on compare avec `==`
- si l'objet n'est pas snapshotable, on le traite comme dependance externe et on reevalue

Correspondance C++ actuelle:

- `UI::Bind(...)`
- `UI::BindMethod`

### 6.8 Modes de capture (`once`, `always`, defaut)

Toute expression reactive (interpolee dans un `Text(...)`, passee a un modifieur, etc.) peut etre prefixee par un mot-cle de capture qui controle quand elle est evaluee.

| Forme | Quand l'expression est-elle evaluee ? | Compile vers |
|---|---|---|
| `once expr`      | une seule fois lors du `Build()` initial | `UI::BindStatic(value)` ou copie locale ; `IsDirty()` retourne toujours `false` |
| (sans prefixe)   | reactif : reevaluee quand une dependance change (mode 6.1-6.7) | `UI::Bind(...)`, `UI::Call(...)`, `UI::BindMethod(...)` selon la nature |
| `always expr`    | a chaque `Update()`, independamment de tout cache | wrapper `UI::BindAlways(...)` ou `IsDirty()` qui retourne toujours `true` |

Regles :

- `once` est l'equivalent expression d'un `@Snapshot` : la valeur est figee, le precompilateur peut materialiser le resultat dans une `std::string` ou un `T` simple.
- `always` doit etre utilise avec parcimonie : il neutralise le cache de `BindFunc`/`BindMethod` et force le recalcul. Utile pour les valeurs intrinsequement non-pures dont les dependances ne sont pas exprimables (timers internes, RNG, frame counter).
- les deux mots-cles ne se composent pas : `once always expr` est rejete par la passe semantique.
- les deux mots-cles peuvent envelopper n'importe quelle sous-expression : litteral, propriete, appel, binaire.

Exemples :

```cui
Text("Construction date: \(once Time.now())")  // snapshot
Text("FPS: \(window.averageFPS)")               // reactif (defaut)
Text("Random tick: \(always nextRandom())")     // re-evalue chaque frame
```

### 6.9 Purete des fonctions exposees (`@cui-volatile`)

Par defaut, toute fonction ou methode exposee via `@cui-expose` est consideree **pure** : son resultat depend uniquement de ses arguments. Le precompilateur cache donc le dernier resultat et ne reevalue que si un argument est dirty.

Le tag `@cui-volatile` (place sur la declaration C++ a cote de `@cui-expose`) marque la fonction comme **non-pure** : son resultat peut changer sans que ses arguments aient change. Causes courantes : lecture du temps systeme, RNG, etat global mute par un autre thread.

Regle :

- une dependance dont la racine est un appel `@cui-volatile` est consideree dirty a chaque `Update()`
- la propagation suit la regle 7 standard : un `Text(...)` qui contient un fragment volatile devient dirty a chaque tick
- l'effet peut etre limite par un `.throttle(period)` ancetre (voir 6.10)

Exemple :

```cpp
namespace Profiler {
    /**
     * @cui-expose
     * @cui-volatile  // resultat depend des mesures glissantes recentes
     */
    std::chrono::nanoseconds GetAverageTime(const char* name);
}
```

### 6.10 Throttle (limitation du taux de mise a jour)

Le modifieur `.throttle(period)` plafonne la frequence a laquelle les expressions reactives portees par un composant peuvent etre re-evaluees.

Semantique :

- `period` est en secondes (`0.5`, `0.5.s`, `500.ms` sont equivalents)
- une expression `IsDirty()` reportee comme dirty par sa source n'est ramenee a `apply()` que si **au moins `period` secondes se sont ecoulees** depuis la derniere evaluation effective sur ce composant
- entre deux evaluations effectives, la valeur cachee precedente est reutilisee meme si la source est dirty
- le throttle est un **plafond hereditaire** : un descendant ne peut jamais s'updater plus souvent que le throttle de son ancetre le plus restrictif. Si un parent declare `.throttle(0.5)` et un enfant `.throttle(0.1)`, le plafond effectif de l'enfant reste `0.5`.
- a l'inverse, un descendant qui declare `.throttle(2.0)` sous un parent a `.throttle(0.5)` voit son propre plafond a `2.0` s'appliquer (le plus restrictif gagne, par composition `max(parent_period, self_period)`)
- aucun throttle = pas de limitation (re-evalue a chaque `Update()` si dirty)

Effet pratique :

- les compteurs de FPS peuvent rafraichir 4 fois par seconde au lieu de 60 sans changer le DSL (`.throttle(0.25)`)
- une vue de profilage globale peut imposer `.throttle(0.5)` au conteneur racine et tous les `Text` enfants en heritent automatiquement
- les `@cui-volatile` sont desactives "implicitement" par un throttle ancetre : ils restent volatiles mais ne sont evalues qu'au prochain creneau autorise

## 7. Regle de dirty tracking

Le DSL doit respecter la logique suivante:

1. Chaque expression compilee possede une derniere valeur ou un dernier resultat.
2. Chaque expression expose un etat dirty.
3. Si dirty est faux, la valeur cachee est re-utilisee.
4. Si dirty est vrai, l'expression est reevaluee.
5. Apres reevaluation, la snapshot est synchronisee.

Cas par cas:

- `static` ou `once expr` -> jamais dirty
- `@Observed` -> dirty si la valeur actuelle est differente de la snapshot, lecture seule cote DSL
- `@Binding` -> dirty si la valeur actuelle est differente de la snapshot ou si la vue ecrit dans la source, lecture/ecriture cote DSL
- `@State` -> dirty quand la vue l'a modifie
- `function` (pure, defaut) -> dirty si un argument est dirty
- `function` annotee `@cui-volatile` -> dirty a chaque tick
- `method` (pure) -> dirty si un argument est dirty ou si l'objet a change
- `method` annotee `@cui-volatile` -> dirty a chaque tick
- `always expr` -> dirty a chaque tick (force la re-evaluation)
- `text` -> dirty si au moins une piece est dirty
- `container` -> dirty si un enfant structurel change ou si un modificateur change

Modulation par `.throttle(period)`:

- chaque composant porteur d'un `.throttle` (et tous ses descendants) garde un timestamp `lastApplied`
- une expression dirty est ignoree (cache reutilise) tant que `now - lastApplied < period`
- au premier `apply()` apres expiration, l'expression est reevaluee normalement et `lastApplied` est mis a jour
- la periode effective sur un sous-arbre est `max(period_self, period_parent_chain)` : le plafond le plus restrictif gagne

## 8. Gestion de l'affichage

Le DSL ne doit pas forcer un repaint complet a chaque update.

Regle:

- un composant feuille ne se recalcule que si ses dependances ont change
- un `Text` recalcule uniquement sa chaine finale si une piece a change
- un conteneur ne recalcule sa disposition que si ses enfants ou ses parametres changent
- une vue ne reconstruit pas son arbre complet a chaque frame

Dans l'implementation actuelle, cela doit rester compatible avec:

- `View::Initialize()` qui appelle `Build()` une seule fois
- `ComponentBase` qui porte les dirty flags de rendu
- `TextContent::BuildText()` qui assemble et cache la chaine finale

## 9. Gestion des composants

Le langage doit pouvoir gerer:

- les composants deja existants
- les composants futurs
- les composants feuilles
- les composants conteneurs
- les composants avec modificateurs
- les composants avec enfants

Pour cela, le compilateur ne doit pas coder en dur chaque composant dans la grammaire.

Il doit utiliser un registre de composants contenant:

- nom du composant
- factory ou constructeur cible
- liste des arguments attendus
- liste des modificateurs disponibles
- capacite a contenir des enfants
- contraintes de type

Ajout d'un nouveau composant:

1. on l'enregistre dans le registre
2. le DSL le voit automatiquement
3. le compilateur peut alors generer l'appel C++ correspondant

Dans la version cible, ce registre ne doit pas etre ecrit a la main. Il doit etre genere par un precompilateur a partir d'annotations minimales placees dans le C++.

## 10. Format de generation

Recommandation MVP:

- un fichier DSL source genere un fichier `.cpp` C++
- un petit `.h` peut etre genere si la vue doit etre re-utilisable depuis d'autres fichiers
- le code genere doit appeler directement les factories C++ existantes

Le compilateur DSL doit produire du C++ lisible, par exemple:

```cpp
auto content = UI::TextContent("Score: ");
content.AppendValue(score);
content.AppendMethod(window, &Window::GetAverageFPS);
return UI::CreateText(bounds, std::move(content), 0.45f);
```

## 11. Syntaxe cible proposee

Exemple de vue minimale (style SwiftUI) :

```txt
view PerformanceView {
    @Observed var window: Window?

    let scale = 0.45

    VBox {
        Text("FPS: \(window.averageFPS)")
            .scale(scale)

        Text("Render: \(averageRenderTimeMs())")
            .scale(scale)
    }
    .spacing(4)
}
```

Exemple plus proche d'un ecran simple :

```txt
view ScoreView {
    @Observed var score: int
    @Snapshot var title: string

    Text("\(title): \(score)")
        .scale(0.8)
}
```

Regles d'ecriture :

- `view` definit une classe de vue compilee vers `UI::View`
- pas de bloc `body` : le contenu de `view { }` est lu en sequence ; il doit contenir un et un seul element racine apres les declarations de wrappers, `let` et `func`
- `@Observed`, `@Snapshot`, `@State` declarent les dependances de la vue
- `Text(...)` produit un composant texte ; ses parties sont les fragments d'interpolation
- les modifieurs viennent toujours apres le bloc d'enfants (`.modifier(args)`)
- les noms de modifieurs sont en camelCase et ne portent pas le prefixe `Set`
- une reference externe non declaree comme `@Observed` ou `@Snapshot` est rejetee : pas de capture implicite

## 12. Equivalent C++ genere

Le meme exemple `PerformanceView` peut devenir:

```cpp
class PerformanceView : public UI::View
{
public:
    explicit PerformanceView(Bounds bounds, const Window *window = nullptr);

protected:
    std::shared_ptr<UI::ContainerBase> Build() override
    {
        auto root = UI::CreateVBox(Bounds());
        root->AddChild(
            UI::CreateText(Bounds(), UI::TextContent("FPS: ", UI::Bind(window, &Window::GetAverageFPS)), 0.45f));
        return root;
    }
};
```

Le point important est que le DSL ne remplace pas votre runtime:

- il le nourrit
- il l'encode plus simplement
- il laisse le moteur UI actuel faire le travail

## 13. Appel depuis une fonction C++

Le code C++ doit pouvoir utiliser la vue generee comme n'importe quelle autre vue.

Exemple:

```cpp
#include "Generated/UI/PerformanceView.gen.h"

std::shared_ptr<UI::View> BuildPerformanceOverlay(const Window *window)
{
    return ui::generated::CreatePerformanceView(window);
}
```

Exemple d'utilisation dans une vue ou un conteneur:

```cpp
auto overlay = BuildPerformanceOverlay(window);
children.push_back(std::move(overlay));
```

Si la vue doit etre configuree apres creation:

```cpp
auto textView = ui::generated::CreateScoreView(score, "Score");
textView->SetTextScale(0.9f);
```

## 14. Compatibilite avec le C++ actuel

Le DSL doit utiliser les memes regles que le code deja ecrit:

- `TextContent` pour toute composition de texte
- `BindValue` pour les valeurs statiques et dynamiques
- `BindFunc` pour les fonctions
- `BindMethod` pour les methodes
- `ComponentBase` pour les dirty flags de rendu
- `View` pour les arbres de vue construits une fois

Cela permet:

- de garder le code existant
- de migrer progressivement
- de tester chaque brique independamment

## 15. Plan d'implementation

### Etape 0 - Systeme de precompilation

Objectif:

- definir comment le precompilateur detecte les composants, les modificateurs et les fonctions exposees
- choisir les annotations minimales a ajouter dans le C++
- definir les fichiers generes

Livrable:

- convention d'annotation stable
- format des fichiers `.generated.h/.cpp`
- prototype du precompilateur

### Etape 1 - Spec et registre

Objectif:

- figer la syntaxe 1.0
- decrire le registre de composants genere
- definir le modele static / ref / state / function / method

Livrable:

- spec ecrite
- inventaire des composants UI existants
- schema de registre de composants

### Etape 2 - AST

Objectif:

- definir les noeuds du DSL
- representer les vues, les composants, les expressions et les modificateurs

Livrable:

- structure AST
- tests sur l'arbre genere

### Etape 3 - Lexer / parser

Objectif:

- lire le fichier DSL
- construire l'AST
- remonter des erreurs lisibles

Points de vigilance specifiques 0.2 :

- segments `\(...)` dans une chaine litterale doivent etre splits en plusieurs noeuds d'expression
- declarations `@Observed var`, `@Snapshot var`, `@State var` et `@Binding var` doivent etre reconnues en tete de bloc `view`
- cas d'enum a tete pointee (`.topLeft`) doivent etre acceptes la ou un type d'enum est attendu, et resolus en passe semantique
- helper `func name(...) -> T = expression` accepte uniquement la forme mono-expression (pas de bloc `{}`)
- chemin de namespace : `Profiler.averageTime(...)` et `Profiler::averageTime(...)` doivent etre acceptes
- ordre des elements : declarations puis exactement un element racine (composant)
- modifieurs `.foo(...)` toujours apres un bloc `{ ... }` d'enfants ou apres une expression d'element
- mots-cles de capture `once expr` et `always expr` reconnus comme prefixes d'expression ; rejet de `once always` ou `always once` combines
- litteraux de duree : `0.5`, `0.5.s`, `500.ms`, `1000.us`, `100.ns` tous parsables et normalisables vers `std::chrono::duration<double>`

Livrable:

- parseur minimal
- messages d'erreur de base

### Etape 4 - Validation semantique

Objectif:

- verifier les types
- verifier les arguments
- verifier les composants connus
- verifier les wrappers `@Observed`, `@Snapshot`, `@State` et `@Binding`
- verifier qu'un cas d'enum pointe (`.topLeft`) correspond bien a un cas du type attendu
- verifier qu'un acces propriete (`obj.foo`) correspond a une methode `Get*` exposee a zero argument
- verifier qu'un appel de namespace cible un symbole `@cui-expose`
- verifier que `once expr` et `always expr` ne sont pas combines (rejet en passe semantique)
- propager le flag `volatile` issu de `@cui-volatile` sur les chaines d'appel pour orienter le choix `BindFunc` / `BindAlways`
- valider que `.throttle(period)` recoit une duree compatible (nombre nu = secondes, ou suffixe `.s`/`.ms`/`.us`/`.ns`)

Livrable:

- diagnostics precis
- refus des combinaisons invalides

### Etape 5 - Generation C++

Objectif:

- convertir l'AST en code C++
- appeler l'API UI existante

Livrable:

- fichier `.cpp` genere
- optionnellement un `.h`

### Etape 6 - Integration build

Objectif:

- integrer la generation dans le build system
- regen uniquement des fichiers modifies

Livrable:

- commande de generation
- compilation automatique des sources generees

### Etape 7 - Dirty tracking de bout en bout

Objectif:

- confirmer qu'un changement de valeur ne recalcule que ce qui est necessaire
- confirmer que les methodes suivent aussi l'objet
- confirmer que `once expr` est evalue exactement une fois (pas de re-eval ulterieure meme si la source change)
- confirmer que `always expr` est dirty a chaque tick (jamais cache)
- confirmer qu'une fonction `@cui-volatile` neutralise le cache d'arguments
- confirmer que `.throttle(period)` plafonne la frequence reelle d'`apply()` et que la composition parent/enfant respecte `max(period_chain)`

Livrable:

- tests unitaires
- tests d'integration UI
- support runtime du throttle dans `ContainerBase` (champ `throttlePeriod`, calcul `effectivePeriod` lors du parcours, gating des `apply()`)

### Etape 8 - Extensions

Objectif:

- `if`
- `for`
- `switch` si utile
- layouts plus riches
- helpers de theme / styles

Livrable:

- syntaxe et generation pour les nouveaux cas

## 16. Criteres d'acceptation

Le projet sera considere correct si:

- une vue DSL peut etre compilee en C++
- la vue DSL peut utiliser tous les composants existants
- un nouveau composant peut etre ajoute sans changer la grammaire principale
- une valeur `ref` modifiee met a jour l'affichage
- une valeur `static` ne provoque aucune reevaluation inutile
- une methode reevalue quand son objet ou ses arguments changent
- l'affichage reste stable quand rien n'a change
- le code genere reste lisible et testable

## 17. Hors perimetre initial

A ne pas faire tout de suite:

- interpreter le DSL a chaque frame
- faire un moteur de layout complet dans le DSL
- supporter toute la syntaxe C++ generale
- remplacer le systeme de composants actuel
- gerer tous les patterns SwiftUI/KotlinUI des le debut

## 18. Conclusion

Le bon chemin est:

1. garder l'UI C++ actuelle comme runtime
2. ajouter un DSL declaratif par-dessus
3. compiler ce DSL en C++ lisible
4. brancher la generation sur le dirty tracking existant
5. ajouter les composants futurs via un registre, pas via la grammaire

Cela donne une syntaxe plus simple a ecrire, plus proche de SwiftUI/KotlinUI, tout en restant compatible avec le moteur deja present.

## 19. Decision d'architecture sur la precompilation

Le systeme recommande est le suivant:

1. Les composants et la logique de rendu restent en C++.
2. Un precompilateur scanne les headers C++ annotees.
3. Il genere automatiquement:
   - les metadonnees d'exposition au DSL
   - les factories si elles n'existent pas
   - les wrappers chainables si ils n'existent pas
   - les classes concretes si elles n'existent pas
4. Un second passage compile les fichiers `.cui` en vrai C++.
5. Le build C++ normal compile ensuite les fichiers generes.

Pourquoi cette approche est meilleure qu'un registre manuel:

- elle evite de dupliquer les signatures dans plusieurs fichiers
- elle reduit fortement le boilerplate pour les nouveaux composants
- elle garde le runtime actuel intact
- elle permet de migrer progressivement les composants deja presents
- elle reste compatible avec les composants futurs

Ce que la precompilation ne doit pas essayer de faire:

- deviner toute seule tous les symboles C++ sans aucune annotation
- remplacer le compilateur C++
- contourner `BindValue`, `BindFunc`, `BindMethod`, `TextContent` ou `View`

Le bon compromis est donc:

- reflection "explicite minimale"
- generation automatique maximale

## 20. Modele d'exposition minimal cote C++

### 20.0 Choix technique : commentaires Doxygen

Le systeme d'annotation utilise des **commentaires Doxygen `@cui-*`** places immediatement au-dessus de la declaration cible. Le precompilateur les parse.

Raisons du choix:
- zero impact sur le compilateur C++ (commentaires ignores)
- pattern eprouve (Qt moc, Unreal UHT)
- parseur simple : chercher `@cui-*` dans les commentaires `/** */`
- facile a ecrire et a lire

Tags supportes:

| Tag | Cible | Signification |
|---|---|---|
| `@cui-component` | classe | expose un composant existant |
| `@cui-generate-component` | classe Base | genere wrapper + concret + factory |
| `@cui-accepts-children <bool>` | classe | capacite a contenir des enfants |
| `@cui-content-model <kind>` | classe | `none`, `text_content` |
| `@cui-modifier [dsl_name]` | methode `DoSetX` | expose comme modifieur DSL, genere le chainable `SetX` si absent |
| `@cui-expose` | fonction / methode | rend appelable depuis le DSL |
| `@cui-volatile` | fonction / methode | force la re-evaluation a chaque tick (resultat non-pur). Defaut = pure, cache sur les arguments |
| `@cui-alias <component>` | classe | alias avec factory differente |
| `@cui-enum` | enum | expose l'enum au DSL |
| `@cui-dsl-name <name>` | methode / enum value | renomme dans le DSL |
| `@cui-factory <name>` | classe | override de la factory auto-detectee |
| `@cui-concrete <name>` | classe | override du type concret auto-detecte |

### 20.0.1 Auto-detection des noms

Pour une classe `Foo` dans le namespace `UI`, le precompilateur infere par convention :

- `@cui-factory` → `UI::CreateFoo` (convention `Create` + nom de classe)
- `@cui-concrete` → `UI::Foo` (namespace + nom de classe)
- nom DSL → `Foo` (nom de classe tel quel)

Ces inferences peuvent toutes etre ecrasees avec un tag explicite si la convention ne s'applique pas.

### 20.0.2 Regles pour `@cui-modifier`

Le tag `@cui-modifier` se place sur la methode `DoSetX` dans la classe Base, pas sur la methode chainable.

Comportement du precompilateur:
- detecte `DoSetFoo(T arg)` annote `@cui-modifier`
- nom DSL infere : retire le prefixe `DoSet`, met en lower-camelCase (`DoSetTextScale` → `textScale`, `DoSetPadding` → `padding`)
- le nom peut etre overridde : `@cui-modifier scale` permet d'ecrire `.scale(...)` au lieu de `.textScale(...)`
- des alias multiples sont autorises : `@cui-modifier scale,textScale` enregistre les deux
- si `SetFoo(T arg)` chainable existe deja → enregistre seulement dans le registre
- si `SetFoo` n'existe pas → genere le wrapper chainable retournant `std::shared_ptr<Derived>`

Deux modes doivent coexister.

### 20.0.3 Acces propriete pour les getters

Toute methode exposee par `@cui-expose` qui :

- a un nom commencant par `Get`
- prend zero argument
- est `const` ou retourne par valeur

est automatiquement aliasee en propriete dans le DSL : on retire `Get` et on met le premier caractere en minuscule.

Exemples :
- `Window::GetAverageFPS()`            → `window.averageFPS`
- `Window::GetMaxFPS()`                 → `window.maxFPS`
- `Profiler::GetAverageTime(name)`      → `Profiler.averageTime(name)` (1 argument, pas un getter, donc pas en propriete : `Profiler.getAverageTime(...)` reste valide aussi avec strip de `Get`)

L'appel explicite avec parentheses (`window.averageFPS()`) reste valide. Le tag `@cui-dsl-name` peut toujours forcer un nom different.

### 20.0.4 Cas d'enum a tete pointee

Quand le contexte fournit un type d'enum (par exemple un parametre nomme `anchor: Anchor` ou un modifieur `.anchor(...)`), le DSL accepte la notation courte `.topLeft` au lieu de `Anchor.topLeft`. La forme qualifiee reste valide. Le precompilateur enregistre les noms d'enum exposes par `@cui-enum` en lower-camelCase.

### 20.0.5 Tag `@cui-volatile`

Le tag `@cui-volatile` se place sur une declaration C++ deja annotee `@cui-expose`. Il declare que le resultat de la fonction / methode peut changer sans que ses arguments aient change.

Causes typiques :
- lecture d'un timer systeme (`std::chrono::steady_clock::now()`)
- generateur pseudo-aleatoire
- moyenne glissante calculee a partir d'echantillons internes (`Profiler::GetAverageTime`)
- etat partage modifie par un autre thread

Effet sur la generation :
- l'entree du registre porte un flag `volatile = true`
- a chaque appel dans le DSL, l'expression est consideree dirty meme si tous ses arguments sont stables
- combine avec un `.throttle(period)` ancetre, l'evaluation reste plafonnee a `period` (voir §20.0.6)

Sans `@cui-volatile`, le precompilateur suppose la fonction pure : il delegue le cache a `UI::BindFunc` / `UI::BindMethod` qui detectent les changements d'arguments via `==`.

Exemple :

```cpp
namespace Profiler {
    /**
     * @cui-expose
     * @cui-volatile
     */
    std::chrono::nanoseconds GetAverageTime(const char* name);
}
```

### 20.0.6 Modifieur `.throttle(period)`

`.throttle(period)` est un modifieur natif du DSL (pas un `@cui-modifier` de composant) reconnu sur **tout** composant. Il plafonne le taux de re-evaluation des expressions reactives portees par le sous-arbre.

Comportement :

- la `period` accepte un nombre nu (secondes) ou un litteral avec suffixe `.s`, `.ms`, `.us`, `.ns`
- le runtime stocke un horodatage `lastApplied` par composant throttle
- une expression dirty est servie depuis son cache tant que `now - lastApplied < period`
- la propagation aux enfants suit la regle "plafond le plus restrictif gagne" : si `parent.period = 0.5` et `child.period = 0.1`, le plafond effectif de l'enfant est `0.5`. La regle de composition est `effective = max(parent_chain_period, self_period)`.
- aucun `.throttle` dans la chaine = pas de limitation (defaut UI/runtime, evaluation a chaque `Update()` si dirty)

Implementation cote runtime (recommandation) :

- `ContainerBase` (et donc tout composant) gagne un champ optionnel `std::optional<std::chrono::duration<double>> throttlePeriod`
- une methode `DoSetThrottlePeriod(...)` traitee comme un `@cui-modifier` natif inscrit dans le registre par defaut
- la passe d'`Update()` calcule `effectivePeriod = max(parent_effective, self_period)` au moment de la descente, exactement comme l'opacite ou le clipping se composent dans un graphe scenique
- chaque `BindValue::IsDirty()` est consulte uniquement si `now - lastApplied >= effectivePeriod`

Exemple DSL :

```cui
VBox {
    Text("FPS: \(window.averageFPS)")            // plafonne a 0.5s
    Text("Render: \(profiler.averageMs)")        // plafonne a 0.5s
}
.throttle(0.5)                                    // ou .throttle(500.ms)
```

Composition imbriquee :

```cui
VBox {
    Text("Coarse: \(slowMetric)")
        .throttle(0.1)        // demande 0.1s mais le parent gagne -> plafond effectif 1.0s

    Text("Fine: \(fastMetric)")
        .throttle(2.0)        // 2.0s > parent 1.0s -> plafond effectif 2.0s
}
.throttle(1.0)
```

### 20.1 Mode A - composant deja implemente

Ce mode sert pour les composants deja presents dans le projet : `Text`, `VBox`, `Container`, `Label`.

Le developpeur ajoute les tags `@cui-*` dans les commentaires existants. Le nom, la factory et le type concret sont **infers automatiquement** depuis le nom de classe et le namespace.

Annotations pour `Text` (`Widgets/Text.h`):

```cpp
/**
 * @cui-component
 * @cui-accepts-children false
 * @cui-content-model text_content
 */
class Text : public ChainableTextWidget<TextWidgetBase, Text> { ... };
```

Dans `TextWidgetBase` (les `DoSet...` existants):

```cpp
/** @cui-modifier */  // infere "textScale" depuis DoSetTextScale
void DoSetTextScale(float scale);

/** @cui-modifier */  // infere "textAnchor"
void DoSetTextAnchor(TextAnchor anchor);

/** @cui-modifier */  // infere "autoSize"
void DoSetAutoSize(bool enabled);
```

Annotations pour `VBox` (`Layout/VBox.h`):

```cpp
/**
 * @cui-component
 * @cui-accepts-children true
 */
class VBox : public ChainableVBox<VBoxBase, VBox> { ... };
```

Dans `VBoxBase`:

```cpp
/** @cui-modifier */  // infere "childAlignment"
void DoSetChildAlignment(HAlign align);

/** @cui-modifier */  // infere "justifyContent"
void DoSetJustifyContent(JustifyContent j);
```

Dans `ContainerBase` (herite par tous les conteneurs):

```cpp
/** @cui-modifier */  void DoSetPadding(float p);
/** @cui-modifier */  void DoSetSpacing(float s);
/** @cui-modifier */  void DoSetOverflowMode(OverflowMode mode);
```

Annotations pour `Label` (alias de `Text` avec factory differente):

```cpp
/**
 * @cui-alias Text
 * @cui-factory UI::CreateLabel
 */
class Label : public ChainableTextWidget<TextWidgetBase, Label> { ... };
```

### 20.2 Mode B - composant a generer

Ce mode sert pour un nouveau composant C++ quand on ne veut pas ecrire a la main le wrapper chainable, la classe concrete, ni la factory.

Le developpeur ecrit seulement la classe `Base` avec ses `DoSet...` annotes:

```cpp
/**
 * @cui-generate-component
 * @cui-accepts-children false
 */
class ProgressBarBase : public UI::ComponentBase
{
public:
    using UI::ComponentBase::ComponentBase;

    /** @cui-modifier */  void DoSetValue(float value);
    /** @cui-modifier */  void DoSetMaxValue(float value);
    /** @cui-modifier */  void DoSetShowLabel(bool enabled);
};
```

Le precompilateur genere automatiquement:

- `ChainableProgressBar<Base, Derived>` avec `SetValue`, `SetMaxValue`, `SetShowLabel`
- `ProgressBar : public ChainableProgressBar<ProgressBarBase, ProgressBar>`
- `CreateProgressBar(Bounds bounds = Bounds{})`
- l'entree de registre DSL avec les modificateurs detectes

Aucun code C++ supplementaire a ecrire pour utiliser `ProgressBar(...)` dans un `.cui`.

## 20.3 Helpers et echappement vers C++

Trois mecanismes coexistent dans un fichier `.cui`, du plus DSL-natif au plus brut :

1. `let X = expr` — constante locale a la vue. Compilee en `constexpr` ou en variable locale `auto` selon que `expr` est `constexpr` ou non.
2. `func name(args) -> T = expr` — helper mono-expression. Compile en methode statique privee de la classe generee.
3. `@cui-cpp { ... }` — escape hatch C++ verbatim. Reste l'option de derniere ressource pour ce que le DSL ne sait pas exprimer.

### 20.3.1 `func` mono-expression

Syntaxe :

```cui
func toMs(d: nanoseconds) -> double = double(d.count) * 1e-6
```

Compile vers :

```cpp
private:
    static double toMs(std::chrono::nanoseconds d) {
        return static_cast<double>(d.count) * 1e-6;
    }
```

Regles :
- le corps est une seule expression DSL (pas un bloc) ; multi-instructions reservees a 0.3
- le corps peut appeler tout autre `func` declare dans la meme vue, tout symbole `@cui-expose`, et tout cas d'enum exposes
- les types de parametres et de retour sont des types DSL ; le precompilateur les resout vers leur equivalent C++ via le registre
- le nom DSL et le nom C++ generes sont identiques (lower-camelCase preserve)

### 20.3.2 Bloc `@cui-cpp` (escape hatch)

Syntaxe :

```cui
view MyView {
    @Observed var window: Window?

    @cui-cpp {
        static double ToMs(std::chrono::nanoseconds d) {
            return static_cast<double>(d.count()) * 1e-6;
        }
    }

    Text("...")
}
```

Regles :
- le contenu est copie tel quel dans le `.gen.cpp` / `.gen.h`, dans la section `private` de la classe
- les methodes declarees dans `@cui-cpp` sont appelables depuis le corps de la vue comme n'importe quel symbole expose, en lower-camelCase si le nom commence par une majuscule (`ToMs` -> appelable `toMs(...)`)
- les includes necessaires doivent etre ajoutes via `@cui-cpp-include "file.h"` en tete de fichier `.cui` ; pour les symboles connus du registre (parametres `@Observed`, fonctions `@cui-expose`, types de cas d'enum), les includes sont auto-derives
- le compilateur CUI ne valide pas la syntaxe C++ du bloc : c'est le compilateur C++ qui detectera les erreurs (les messages C++ pointeront vers la ligne du `.gen.cpp`, pas du `.cui` - limitation connue)

Syntaxe `@cui-cpp-include` :

```cui
@cui-cpp-include "Profiler.h"
@cui-cpp-include <chrono>

view PerformanceView { ... }
```

### 20.4 Appels de fonctions de namespace dans le DSL

Le DSL supporte deux notations equivalentes pour les fonctions enregistrees `@cui-expose` dans un namespace C++ :

- forme pointee (canonique 0.2) : `Profiler.averageTime("Render")`
- forme `::` (toleree, equivalente) : `Profiler::averageTime("Render")`

Regle de resolution :

- le precompilateur doit avoir vu `@cui-expose` sur `Profiler::GetAverageTime`
- le nom DSL est derive du nom C++ : `Get` initial est retire si present, premier caractere mis en minuscule (`GetAverageTime` → `averageTime`)
- `@cui-dsl-name` permet de forcer un nom different
- la resolution utilise le nom qualifie complet comme cle dans le registre

Exemple sur `Profiler`:

```cpp
namespace Profiler {
    /** @cui-expose */
    std::chrono::nanoseconds GetAverageTime(const char* name);
}
```

Accessible dans le DSL via `Profiler.averageTime("Render")` (canonique) ou `Profiler::averageTime("Render")` ou `Profiler.getAverageTime("Render")` (sans strip de `Get`, toujours autorise).

## 21. Ce que le precompilateur doit generer pour les composants existants

Pour les composants deja presents dans le projet, la precompilation doit privilegier le mode "metadata-first".

Autrement dit:

- si le composant existe deja completement, ne rien regenerer inutilement
- si seule l'exposition manque, generer seulement la metadonnee
- si la factory manque, generer seulement la factory
- si le wrapper chainable manque, generer seulement le wrapper

### 21.1 Cas `Text`

Etat actuel:

- `UI::Text` existe deja
- `CreateText(...)` existe deja
- les modificateurs `SetTextScale`, `SetTextAnchor`, `SetAutoSize` existent deja
- `TextContent` sait deja gerer texte, valeurs, fonctions et methodes

Le precompilateur ne doit donc pas regenerer le runtime du composant `Text`. Il doit seulement generer:

- la fiche de composant du DSL
- la table de modificateurs autorises
- la regle speciale "les arguments de `Text(...)` vont vers `TextContent`"

Exemple DSL:

```txt
Text("FPS: \(window.averageFPS)")
    .scale(0.45)
```

Equivalent C++ genere:

```cpp
auto node = UI::CreateText(
    Bounds(),
    UI::TextContent("FPS: ", UI::Bind(window, &Window::GetAverageFPS)))
    ->SetTextScale(0.45f);
```

### 21.2 Cas `VBox`

Etat actuel:

- `UI::VBox` existe deja
- `CreateVBox(...)` existe deja
- `SetPadding`, `SetSpacing`, `SetOverflowMode`, `SetChildAlignment`, `SetJustifyContent` existent deja
- `VBox` accepte des enfants

Le precompilateur doit donc generer la fiche "container avec enfants".

Exemple DSL:

```txt
VBox {
    Text("FPS")
    Label("Profiler")
}
.padding(8)
.spacing(4)
```

Equivalent C++ genere:

```cpp
auto root = UI::CreateVBox(Bounds())
    ->SetPadding(8.0f)
    ->SetSpacing(4.0f);

root->AddChild(UI::CreateText(Bounds(), "FPS"));
root->AddChild(UI::CreateLabel(Bounds(), "Profiler"));
```

### 21.3 Cas `Container`

Etat actuel:

- `UI::Container` existe deja
- `CreateContainer(...)` existe deja
- les modificateurs de `ContainerBase` existent deja

Le precompilateur doit le traiter comme un composant conteneur generique.

Exemple DSL:

```txt
Container {
    Text("Logs")
}
.padding(12)
.overflowMode(.scroll)
```

Equivalent C++ genere:

```cpp
auto panel = UI::CreateContainer(Bounds())
    ->SetPadding(12.0f)
    ->SetOverflowMode(UI::OverflowMode::SCROLL);

panel->AddChild(UI::CreateText(Bounds(), "Logs"));
```

## 22. Exposition des fonctions et methodes appelables

Le DSL doit pouvoir appeler:

- des fonctions libres
- des methodes statiques
- des methodes d'instance
- des getters sans argument
- des methodes avec arguments statiques ou dynamiques

Mais ces symboles ne doivent pas etre devines par magie. Ils doivent etre exposes explicitement.

Exemple conceptuel:

```cpp
UI_CUI_EXPOSE_METHOD(Window, GetAverageFPS);
UI_CUI_EXPOSE_METHOD(Window, GetAverageFrameTimeMs);
UI_CUI_EXPOSE_METHOD(PerformanceView, GetAverageRenderTimeMs);
UI_CUI_EXPOSE_FUNCTION(Math, FormatPercent);
```

Regles de generation:

- si la methode est appelee sur un objet `ref`, generer `UI::Bind(...)`
- si la fonction a des arguments reactifs, generer `UI::Call(...)`
- si l'appel est utilise dans `Text(...)`, integrer le resultat a `TextContent`
- si la methode prend un objet dont l'etat peut etre compare, conserver le suivi de l'objet deja present dans `BindMethod`

Exemple DSL (style 0.2) :

```txt
Text("FPS: \(window.averageFPS)")
Text("Render: \(averageRenderTimeMs())")
Text("Usage: \(Math.formatPercent(used, total))")
```

Equivalent C++ genere:

```cpp
UI::CreateText(Bounds(), UI::TextContent("FPS: ", UI::Bind(window, &Window::GetAverageFPS)));
UI::CreateText(Bounds(), UI::TextContent("Render: ", UI::Bind(this, &PerformanceView::GetAverageRenderTimeMs)));
UI::CreateText(Bounds(), UI::TextContent("Usage: ", UI::Call(&Math::FormatPercent, used, total)));
```

## 23. Fichiers generes par le precompilateur

Le precompilateur doit produire deux familles de fichiers.

### 23.1 Fichiers de runtime genere

Ils servent a completer les composants C++ si necessaire.

Exemples:

- `Generated/UI/Runtime/ProgressBar.generated.h`
- `Generated/UI/Runtime/ProgressBar.generated.cpp`
- `Generated/UI/Meta/ComponentRegistry.generated.h`
- `Generated/UI/Meta/FunctionRegistry.generated.h`

Ces fichiers contiennent:

- les wrappers chainables manquants
- les factories manquantes
- les aliases de composant
- le registre compile des composants, modificateurs, fonctions et methodes

### 23.2 Fichiers de vue DSL generee

Ils servent a compiler les fichiers `.cui`.

Exemples:

- `Generated/UI/Views/PerformanceView.gen.h`
- `Generated/UI/Views/PerformanceView.gen.cpp`
- `Generated/UI/Views/ScoreView.gen.h`
- `Generated/UI/Views/ScoreView.gen.cpp`

Chaque vue DSL doit generer:

- une vraie classe C++ derivant de `UI::View` si la vue est structurelle
- ou une fonction factory C++ si un helper suffit
- le code de `Build()`
- les champs necessaires pour stocker les `ref`, `let`, `state`

## 24. Integration build exacte

Le build doit etre decoupe en deux etages avant la compilation C++ normale.

### 24.1 Etage A - scan C++

Entrees:

- les headers contenant `UI_CUI_COMPONENT`, `UI_CUI_GENERATE_COMPONENT`, `UI_CUI_MODIFIER`
- les headers contenant `UI_CUI_EXPOSE_FUNCTION` et `UI_CUI_EXPOSE_METHOD`

Sorties:

- le registre de composants
- le registre de symboles appelables
- les fichiers runtime generes manquants

### 24.2 Etage B - compilation DSL

Entrees:

- les fichiers `.cui`
- le registre genere a l'etape A

Sorties:

- les `.gen.h/.gen.cpp` de chaque vue

### 24.3 Etage C - compilation C++

Entrees:

- le code C++ normal du projet
- les fichiers `.generated.*`
- les fichiers `.gen.*`

Sortie:

- le binaire final

### 24.4 Regeneration incrementale

Le build doit recalculer seulement ce qui a change:

- si un header annote change, regen seulement les metadonnees et artefacts dependants
- si un `.cui` change, regen seulement sa vue
- si un composant expose change de signature, invalider les vues qui l'utilisent

Le systeme ideal repose sur:

- hash de contenu
- graphe de dependances
- dossier `Generated/` proprement integre dans les includes

## 25. Exemple complet de bout en bout avec les composants actuels

### 25.1 Exposition C++ minimale (annotations ajoutees dans les headers existants)

Dans `Widgets/Text.h`:

```cpp
/**
 * @cui-component
 * @cui-accepts-children false
 * @cui-content-model text_content
 */
class Text : public ChainableTextWidget<TextWidgetBase, Text> { ... };

// Dans TextWidgetBase :
/** @cui-modifier */  void DoSetTextScale(float scale);
/** @cui-modifier */  void DoSetTextAnchor(TextAnchor anchor);
```

Dans `Layout/VBox.h`:

```cpp
/**
 * @cui-component
 * @cui-accepts-children true
 */
class VBox : public ChainableVBox<VBoxBase, VBox> { ... };
```

Dans `Core/Container.h` (herite par VBox, HBox, Container):

```cpp
/** @cui-modifier */  void DoSetPadding(float p);
/** @cui-modifier */  void DoSetSpacing(float s);
/** @cui-modifier */  void DoSetOverflowMode(OverflowMode mode);
```

Dans `Window.h`:

```cpp
/** @cui-expose */
float GetAverageFPS() const;
```

Dans `Profiler.h`:

```cpp
namespace Profiler {
    /**
     * @cui-expose
     * @cui-volatile
     */
    std::chrono::nanoseconds GetAverageTime(const char* name);
}
```

Le tag `@cui-volatile` indique que la fonction depend de mesures glissantes (changeantes meme avec un argument constant) et doit donc etre re-evaluee a chaque tick. Sans throttle ancetre, elle se recalcule a chaque frame ; avec un `.throttle(period)`, elle se recalcule au plus une fois par `period`.

### 25.2 Fichier CUI

```cui
view PerformanceView {
    @Observed var window: Window?

    func toMs(d: nanoseconds) -> double = double(d.count) * 1e-6

    let fpsScale    = 0.45
    let metricScale = 0.30

    VBox {
        Text("FPS: \(window.averageFPS)")
            .scale(fpsScale)

        Text("Render: \(toMs(Profiler.averageTime("Render"))) ms").scale(metricScale)
        Text("Render World: \(toMs(Profiler.averageTime("RenderWorld"))) ms").scale(metricScale)
        Text("Upscale: \(toMs(Profiler.averageTime("Upscale"))) ms").scale(metricScale)
        Text("UI Upscale: \(toMs(Profiler.averageTime("UIUpscale"))) ms").scale(metricScale)
        Text("Swap Buffers: \(toMs(Profiler.averageTime("SwapBuffers"))) ms").scale(metricScale)
    }
    .frame(width: 260, height: 160, anchor: .topLeft)
    .padding(12)
    .spacing(4)
    .throttle(0.25)         // au plus 4 mises a jour par seconde pour TOUS les enfants
}
```

Notes :

- aucun `@cui-cpp-include` : `Profiler.h` est auto-derivee de l'usage `Profiler.averageTime`, `<chrono>` est auto-derivee de `nanoseconds`
- aucun bloc `body { }` : le contenu de `view { }` est lu en sequence (declarations puis l'unique racine `VBox`)
- aucun qualifiant `ref` : `@Observed var window: Window?` declare la dependance reactive
- `window.averageFPS` resout `Window::GetAverageFPS()` (regle propriete, §20.0.3)
- `Profiler.averageTime("X")` resout `Profiler::GetAverageTime("X")` (regle namespace, §20.4)
- `Profiler::GetAverageTime` est annotee `@cui-volatile` (§20.0.5) : sans le `.throttle` parent, elle se recalculerait a chaque frame ; avec `.throttle(0.25)` elle est plafonnee a 4 Hz et tous les `Text` enfants en heritent
- `nanoseconds` est un alias DSL pour `std::chrono::nanoseconds` (registre de types)
- `260` est en pixels par defaut ; `.frame(width: 260.percent, ...)` serait l'equivalent en pourcentage
- `.topLeft` est resolu vers `UI::Anchor::TOP_LEFT` car le parametre attendu est de type `Anchor`

Variations utiles des modes de capture :

```cui
// snapshot d'une chaine au moment du Build (jamais re-evaluee)
Text("Build version: \(once Version.current())")

// expression reactive standard (defaut, recommande pour la majorite des cas)
Text("FPS: \(window.averageFPS)")

// force la re-evaluation a chaque tick meme si les arguments ne changent pas
Text("Random: \(always Math.nextRandom())")
```

### 25.3 C++ genere

```cpp
// AUTO-GENERATED - do not edit. Source: PerformanceView.cui

#include "UI/View.h"
#include "UI/Layout/VBox.h"
#include "UI/Widgets/Text.h"
#include "UI/Utils/TextContent.h"
#include "UI/Core/Bounds.h"
#include "Window.h"
#include "Profiler.h"
#include <chrono>

namespace ui::generated
{

class PerformanceView : public UI::View
{
public:
    explicit PerformanceView(UI::Bounds bounds, const Window *window = nullptr)
        : UI::View(bounds), window(window) {}

protected:
    std::shared_ptr<UI::ContainerBase> Build() override
    {
        constexpr float fpsScale    = 0.45f;
        constexpr float metricScale = 0.30f;

        auto root = UI::CreateVBox(UI::Bounds(260_px, 160_px, UI::Anchor::TOP_LEFT))
            ->SetPadding(12.0f)
            ->SetSpacing(4.0f)
            ->SetThrottlePeriod(std::chrono::duration<double>(0.25));   // .throttle(0.25)

        // Text("FPS: \(window.averageFPS)")
        {
            UI::TextContent c = (window != nullptr)
                ? UI::TextContent("FPS: ", UI::Bind(window, &Window::GetAverageFPS))
                : UI::TextContent("FPS: --");
            root->AddChild(UI::CreateText(UI::Bounds{}, std::move(c))->SetTextScale(fpsScale));
        }

        // Text("Render: \(toMs(Profiler.averageTime("Render"))) ms")
        root->AddChild(UI::CreateText(UI::Bounds{},
            UI::TextContent("Render: ",
                            UI::Call(&toMs, Profiler::GetAverageTime("Render")),
                            " ms"))
            ->SetTextScale(metricScale));

        // ... idem pour RenderWorld, Upscale, UIUpscale, SwapBuffers

        return root;
    }

private:
    const Window *window = nullptr;

    // func toMs(d: nanoseconds) -> double = double(d.count) * 1e-6
    static double toMs(std::chrono::nanoseconds d) {
        return static_cast<double>(d.count()) * 1e-6;
    }
};

std::shared_ptr<PerformanceView> CreatePerformanceView(
    UI::Bounds bounds = UI::Bounds(), const Window *window = nullptr)
{
    return std::make_shared<PerformanceView>(bounds, window);
}

} // namespace ui::generated
```

Notes de mapping (DSL → C++) :

- `@Observed var window: Window?` → champ membre `const Window *window`, ajoute en parametre du constructeur apres `Bounds`
- `func toMs(d: nanoseconds) -> double = ...` → methode statique privee `static double toMs(std::chrono::nanoseconds d)`
- `let fpsScale = 0.45` → `constexpr float fpsScale = 0.45f`
- `VBox { ... }.frame(width: 260, height: 160, anchor: .topLeft).padding(12).spacing(4)` → `UI::CreateVBox(UI::Bounds(260_px, 160_px, UI::Anchor::TOP_LEFT))->SetPadding(12.0f)->SetSpacing(4.0f)`
- `"FPS: \(window.averageFPS)"` → `UI::TextContent("FPS: ", UI::Bind(window, &Window::GetAverageFPS))`
- `Profiler.averageTime("Render")` → `Profiler::GetAverageTime("Render")` (alias `Get` strip + `::`) ; le flag `volatile` issu de `@cui-volatile` empeche `BindFunc` de mettre en cache le dernier resultat
- `.scale(fpsScale)` → `->SetTextScale(fpsScale)` (alias `scale` declare via `@cui-modifier scale`)
- `.throttle(0.25)` → `->SetThrottlePeriod(std::chrono::duration<double>(0.25))` ; le runtime applique `effectivePeriod = max(parent_chain, self)` lors de la descente d'`Update()`
- `\(once expr)` → la valeur est evaluee une fois et stockee dans une `std::string` ou `T` local ; le fragment passe a `UI::TextContent` est un litteral (pas de `Bind`)
- `\(always expr)` → fragment passe via `UI::BindAlways(...)` (ou un wrapper qui retourne `IsDirty() == true` systematiquement)

### 25.4 Appel depuis le C++

```cpp
#include "Generated/UI/Views/PerformanceView.gen.h"

std::shared_ptr<UI::View> CreateOverlay(const Window *window)
{
    return ui::generated::CreatePerformanceView(UI::Bounds{}, window);
}
```

## 26. Recommandation de mise en oeuvre immediate

Pour commencer proprement sans trop de risque, l'ordre recommande est:

1. Implementer d'abord le scan C++ et la generation de registre, sans DSL.
2. Exposer `Text`, `Label`, `Container`, `VBox`, `HBox` et quelques methodes de lecture.
3. Generer un premier `.cui` simple vers `CreateText`, `CreateVBox`, `CreateContainer`.
4. Ajouter ensuite le mode `UI_CUI_GENERATE_COMPONENT(...)` pour les futurs composants.
5. Ne supporter les composants ecrits entierement en CUI qu'apres stabilisation du pipeline precedent.

Cela permet de demarrer avec le code existant, de valider la chaine de generation et de reduire tres vite le boilerplate, sans devoir concevoir tout un nouveau runtime.
