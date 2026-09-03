# modulemanager — journal

Marqueurs : 🟢 ajout · 🔴 rupture · 🔵 correctif · ⚪ interne ou doc · 🟡 propose
dans le plan, code non ecrit.

## v0.1.0

- 🟢 `Load`, `Unload`, `Reconcile` — condamne n'est pas ferme
- 🟢 `SharedLibrary` en `RTLD_NOW | RTLD_LOCAL` : c'est ce flag qui fait
  tenir deux vendors aux classes homonymes dans un meme processus
- 🟢 table Stride : une colonne par dll, une ligne par contrat, O(1)
- 🟢 aides typees `Get<T>`, `GetAllByType<T>`, `Current<T>`, `Select<T>`
- 🟢 23 tests

## v0.2.0 — propose, rien de tout ceci n'est ecrit

- 🟡 `Adopt(IModule*, key)` — un module lie en statique entrerait dans la
  table sans `dlopen`. Meme corps que `Load` sans le `SharedLibrary` ;
  `Reconcile()` ne changerait pas d'une ligne
- 🟡 `Binding<T>` — suivre un contrat, tenir le verrou, numeroter les
  generations, epingler ou suivre le defaut
- 🟡 registre de jetons : `acquire()` refuserait si une ressource de
  `claims()` est detenue ailleurs
