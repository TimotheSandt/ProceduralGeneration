# UI DSL - Cahier des charges et plan d'implementation

Version cible: 0.1

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

## 4. Format cible

Le DSL doit pouvoir decrire:

- une `view`
- des composants enfants
- des valeurs statiques
- des valeurs dynamiques
- des appels de fonctions
- des appels de methodes sur objet
- des modificateurs chaines
- des blocs conditionnels
- des listes / boucles plus tard

Le format cible doit rester proche de SwiftUI / KotlinUI:

- `body { ... }`
- composants imbriques
- chaines de modificateurs
- donnees de l'exterieur marquees explicitement

## 5. Grammaire minimale

La grammaire 1.0 doit couvrir au minimum:

```txt
view_decl   := "view" IDENT "(" param_list? ")" block
param_list  := param ("," param)*
param       := IDENT ":" type [ "=" expr ] [ qualifier ]
qualifier   := "let" | "ref" | "state"
block       := "{" stmt* "}"
stmt        := component | let_decl | if_stmt | for_stmt
component   := IDENT "(" arg_list? ")" modifier_chain? block?
arg_list    := expr ("," expr)*
modifier_chain := ("." IDENT "(" arg_list? ")")*
expr        := string | number | identifier | call | member_call | concat | paren
```

Notes:

- `let` = valeur figee
- `ref` = valeur externe suivie
- `state` = etat possede par la vue
- les composants sont resolves par nom
- les modificateurs suivent les methodes chainables C++

## 6. Modeles de donnees

### 6.1 Valeur statique

Une valeur statique est capturee une seule fois.

Exemples:

- texte litteral
- nombre ecrit dans le DSL
- valeur C++ non reactive

Regle:

- elle ne devient jamais dirty
- elle est traduite vers une copie locale ou vers `BindStatic(...)`

### 6.2 Valeur dynamique

Une valeur dynamique est comparee a sa derniere valeur utilisee.

Regle:

- si `==` dit que la valeur a change, elle devient dirty
- apres `apply()` ou evaluation, la snapshot est mise a jour
- si le type ne sait pas se comparer ou se copier, il faut un mode externe / toujours dirty

Correspondance C++ actuelle:

- `UI::BindDynamic(...)`
- `UI::BindValue::IsDirty()`

### 6.3 Etat local de vue

Une valeur `state` appartient a la vue.

Regle:

- seule la vue la modifie
- la modification marque le noeud concerné dirty
- elle peut etre compilee vers `DeferredValue<T>` ou un wrapper equivalent

### 6.4 Fonction

Une fonction pure ou impure peut etre utilisee dans une expression.

Regle:

- si un argument est dirty, le resultat est dirty
- si aucun argument n'a change, le dernier resultat est re-utilise
- si la fonction n'a aucun argument, elle reste reevaluee a chaque fois tant que l'implementation C++ actuelle le demande

Correspondance C++ actuelle:

- `UI::Call(...)`
- `UI::BindFunc`

### 6.5 Methode sur objet

Une methode d'objet est une dependance a deux niveaux:

- l'objet lui-meme
- les arguments de la methode

Regle:

- si l'objet a change, le resultat est dirty
- si un argument a change, le resultat est dirty
- si l'objet est copiable et comparable, on garde une snapshot et on compare avec `==`
- si l'objet n'est pas snapshotable, on le traite comme dependance externe et on reevalue

Correspondance C++ actuelle:

- `UI::Bind(...)`
- `UI::BindMethod`

## 7. Regle de dirty tracking

Le DSL doit respecter la logique suivante:

1. Chaque expression compilee possede une derniere valeur ou un dernier resultat.
2. Chaque expression expose un etat dirty.
3. Si dirty est faux, la valeur cachee est re-utilisee.
4. Si dirty est vrai, l'expression est reevaluee.
5. Apres reevaluation, la snapshot est synchronisee.

Cas par cas:

- `static` -> jamais dirty
- `ref` -> dirty si la valeur actuelle differente de la snapshot
- `state` -> dirty quand la vue l'a modifie
- `function` -> dirty si un argument est dirty
- `method` -> dirty si un argument est dirty ou si l'objet a change
- `text` -> dirty si au moins une piece est dirty
- `container` -> dirty si un enfant structurel change ou si un modificateur change

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

Exemple de vue:

```txt
view PerformanceView(window: ref Window?) {
    let scale = 0.45

    body {
        VBox(spacing: 4) {
            Text("FPS: " + window.GetAverageFPS())
                .SetTextScale(scale)

            Text("Render: " + GetAverageRenderTimeMs())
                .SetTextScale(scale)
        }
    }
}
```

Exemple plus proche d'un ecran simple:

```txt
view ScoreView(score: ref int, title: let string) {
    body {
        Text(title + ": " + score)
            .SetTextScale(0.8)
    }
}
```

Regles d'ecriture:

- `view` definit une classe de vue
- `body` definit l'arbre UI principal
- `Text(...)` produit un composant texte
- les appels chaines apres un composant deviennent des modificateurs
- une reference externe utilisee sans `let` doit etre traitee comme dynamique

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

Livrable:

- parseur minimal
- messages d'erreur de base

### Etape 4 - Validation semantique

Objectif:

- verifier les types
- verifier les arguments
- verifier les composants connus
- verifier les qualifiants `let`, `ref`, `state`

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

Livrable:

- tests unitaires
- tests d'integration UI

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
- nom DSL infere: retire le prefixe `DoSet`, met en camelCase (`DoSetTextScale` → `textScale`)
- le nom peut etre overridde: `@cui-modifier myName`
- si `SetFoo(T arg)` chainable existe deja → enregistre seulement dans le registre
- si `SetFoo` n'existe pas → genere le wrapper chainable retournant `std::shared_ptr<Derived>`

Deux modes doivent coexister.

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

## 20.3 Blocs `@cui-cpp` dans les fichiers `.cui`

Un fichier `.cui` peut contenir des blocs de code C++ brut via `@cui-cpp { ... }`. Ces blocs sont emis verbatim dans la section `private` de la classe generee.

Cela evite la sous-classe manuelle quand la vue a besoin de methodes utilitaires.

Syntaxe:

```cui
view MyView(ref window: Window?) {

    @cui-cpp {
        static double ToMs(std::chrono::nanoseconds d) {
            return static_cast<double>(d.count()) * 1e-6;
        }
    }

    body { ... }
}
```

Regles:

- le contenu est copie tel quel dans le `.gen.cpp` / `.gen.h`, dans la section `private` de la classe
- les methodes declarees dans `@cui-cpp` sont appelables depuis le corps de la vue comme n'importe quel symbole expose
- les includes necessaires doivent etre ajoutes via `@cui-cpp-include "file.h"` en tete de fichier `.cui`
- le compilateur CUI ne valide pas la syntaxe C++ du bloc : c'est le compilateur C++ qui detectera les erreurs (les messages C++ pointeront vers la ligne du `.gen.cpp`, pas du `.cui` - limitation connue)

Syntaxe `@cui-cpp-include`:

```cui
@cui-cpp-include "Profiler.h"
@cui-cpp-include <chrono>

view PerformanceView(ref window: Window?) { ... }
```

### 20.4 Appels de fonctions avec `::` dans le DSL

Le DSL supporte la notation `Namespace::Function(args)` pour appeler des fonctions enregistrees `@cui-expose` dans un namespace C++.

Regle de resolution:

- `Profiler::GetAverageTime("Render")` → le precompilateur doit avoir vu `@cui-expose` sur `Profiler::GetAverageTime`
- la resolution utilise le nom qualifie complet comme cle dans le registre
- version courte via `@cui-dsl-name`: `@cui-dsl-name Profiler.GetAverageTime` permet d'ecrire `Profiler.GetAverageTime(...)` avec un point

Exemple sur `Profiler`:

```cpp
namespace Profiler {
    /** @cui-expose */
    std::chrono::nanoseconds GetAverageTime(const char* name);
}
```

Dans le DSL, accessible via `Profiler::GetAverageTime("Render")` ou si renomme `Profiler.GetAverageTime("Render")`.

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
Text("FPS: " + window.GetAverageFPS())
    .SetTextScale(0.45)
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
VBox()
    .SetPadding(8)
    .SetSpacing(4)
{
    Text("FPS")
    Label("Profiler")
}
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
Container()
    .SetPadding(12)
    .SetOverflowMode(OverflowMode::SCROLL)
{
    Text("Logs")
}
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

Exemple DSL:

```txt
Text("FPS: " + window.GetAverageFPS())
Text("Render: " + GetAverageRenderTimeMs())
Text("Usage: " + Math.FormatPercent(used, total))
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
    /** @cui-expose */
    std::chrono::nanoseconds GetAverageTime(const char* name);
}
```

### 25.2 Fichier CUI

```cui
@cui-cpp-include "Profiler.h"
@cui-cpp-include <chrono>

view PerformanceView(ref window: Window?) {

    @cui-cpp {
        static double ToMs(std::chrono::nanoseconds d) {
            return static_cast<double>(d.count()) * 1e-6;
        }
    }

    let fpsScale    = 0.45
    let metricScale = 0.30

    body {
        VBox()
            .frame(width: 260.px, height: 160.px, anchor: Anchor.topLeft)
            .padding(12)
            .spacing(4)
        {
            Text("FPS: " + window.GetAverageFPS())
                .textScale(fpsScale)

            Text("Render: "       + ToMs(Profiler::GetAverageTime("Render"))      + " ms").textScale(metricScale)
            Text("Render World: " + ToMs(Profiler::GetAverageTime("RenderWorld")) + " ms").textScale(metricScale)
            Text("Upscale: "      + ToMs(Profiler::GetAverageTime("Upscale"))     + " ms").textScale(metricScale)
            Text("UI Upscale: "   + ToMs(Profiler::GetAverageTime("UIUpscale"))   + " ms").textScale(metricScale)
            Text("Swap Buffers: " + ToMs(Profiler::GetAverageTime("SwapBuffers")) + " ms").textScale(metricScale)
        }
    }
}
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
            ->SetSpacing(4.0f);

        // Text("FPS: " + window.GetAverageFPS())
        {
            UI::TextContent c = (window != nullptr)
                ? UI::TextContent("FPS: ", UI::Bind(window, &Window::GetAverageFPS))
                : UI::TextContent("FPS: --");
            root->AddChild(UI::CreateText(UI::Bounds{}, std::move(c))->SetTextScale(fpsScale));
        }

        // Text("Render: " + ToMs(...) + " ms")
        root->AddChild(UI::CreateText(UI::Bounds{},
            UI::TextContent("Render: ",
                            UI::Call(&ToMs, Profiler::GetAverageTime("Render")),
                            " ms"))
            ->SetTextScale(metricScale));

        // ... idem pour RenderWorld, Upscale, UIUpscale, SwapBuffers

        return root;
    }

private:
    const Window *window = nullptr;

    // @cui-cpp block
    static double ToMs(std::chrono::nanoseconds d) {
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
