# modulemanager

version: 1.0.0

> Finds, loads and keeps track of modules at runtime.

Split out of [imodule](https://github.com/P-E-R-R-Y/imodule): that repo holds
the interface a module answers to, this one holds the machinery that loads
them. An interface and a manager are two different jobs.

## What is in here

- `ModuleManager` — the table of loaded modules. One row per shared library,
  one column per contract.
- `SharedLibrary` — a `dlopen`/`dlsym` wrapper, RAII, cross-platform.
- `Stride` — the two-axis table `ModuleManager` is built on. Generic: rows
  carry an identity, columns carry optional values.

## Loading

A loaded library IS a row, a contract IS a column, a module is the cell
where they meet. Two identifiers, one per axis :

- the **key** you passed to `Load()` names a row,
- the **static type** `T` names a column.

Both are yours. The manager never reads `IModule::name()` or `type()` — a
module's address is its position in the table, not something it declares
about itself. Those two stay on the module for whoever needs them, typically
a plugin host that has no static types to work with.

```cpp
ModuleManager<IGraphic2Module, IGraphic3Module, IAudioModule> modules;

modules.Load("./raylib_impl.dylib", "ray");
modules.Load("./sfml_impl.dylib",   "sfml");

modules.Get<IGraphic2Module>("ray");    // the cell : contract + key
modules.Get<IGraphic3Module>("sfml");   // nullptr, sfml has no 3D
modules.GetAll<IGraphic2Module>();      // every library providing it
modules.GetAll("ray");                  // everything "ray" provides
modules.Unload("ray");                  // clears the row, closes the dll
```

Partial coverage is not an error : a vendor that only does 2D simply leaves
the 3D column empty, and `Get` returns `nullptr`.

`Find(key)` hands back the row itself, so `Get<T>(entity)` and `Get(entity)`
can work on it without looking the key up again.
