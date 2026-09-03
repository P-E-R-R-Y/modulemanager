# modulemanager — changelog

Markers: 🟢 added · 🔴 breaking · 🔵 fix · ⚪ internal or docs · 🟡 proposed
in the plan, no code written yet.

## v0.1.0

- 🟢 `Load`, `Unload`, `Reconcile` — condemned is not closed
- 🟢 `SharedLibrary` under `RTLD_NOW | RTLD_LOCAL`: this flag is what lets
  two vendors with same-named classes coexist in one process
- 🟢 Stride table: one column per dll, one row per contract, O(1)
- 🟢 typed helpers `Get<T>`, `GetAllByType<T>`, `Current<T>`, `Select<T>`
- 🟢 23 tests

## v0.2.0 — proposed, none of this is written

- 🟡 `Adopt(IModule*, key)` — a statically linked module would enter the
  table without `dlopen`. Same body as `Load` minus the `SharedLibrary`;
  `Reconcile()` would not change a line
- 🟡 `Binding<T>` — follow a contract, hold the lock, number the
  generations, pin or follow the default
- 🟡 token registry: `acquire()` would refuse if a `claims()` resource is
  held elsewhere
