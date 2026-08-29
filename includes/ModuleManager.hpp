/**
 * @file ModuleManager.hpp
 *
 * @addtogroup modulemanager
 * @{
 */

#ifndef MODULE_MANAGER_HPP
#define MODULE_MANAGER_HPP

#include <concepts>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "IModule.hpp"
#include "IModuleRegistry.hpp"
#include "SharedLibrary.hpp"
#include "Stride.hpp"


/**
 * @class ModuleManager
 * @brief A Stride<SharedLibrary, Ts...> : a loaded dll IS a row, a contract
 *        (Ts...) IS a column, a module is the cell where they meet.
 *
 * Two identifiers, one per axis, and nothing else :
 *
 * - the KEY you passed to Load() names a row,
 * - the static type T names a column.
 *
 * Both are yours. The manager never reads IModule::name() or type() — a
 * module's identity is its position in the table, not something it declares
 * about itself. IModule is only required so GetAllByKey(key) has a common base to
 * hand a whole row back through.
 *
 *     ModuleManager<IGraphicModule, IWindowModule> modules;
 *
 *     modules.Load("./ray.so", "ray");
 *     modules.Get<IGraphicModule>("ray"); // one, by contract and key
 *     modules.GetAll<IGraphicModule>();   // every library providing it
 *     modules.GetAllByKey("ray");              // everything "ray" provides
 *     modules.Get<IGraphicModule>(e);     // one, by row
 *     modules.Get(e);                     // the SharedLibrary of this row
 *     modules.Unload("ray");              // clears the row, closes the dll
 */
template <std::derived_from<IModule>... Ts>
class ModuleManager : public IModuleRegistry,
                      private Stride<std::unique_ptr<SharedLibrary>, Ts *...> {
        using Base = Stride<std::unique_ptr<SharedLibrary>, Ts *...>;

    public:
        /* Inheritance is PRIVATE : Stride is the substrate, not the API.
         * Public it would let a caller add() a row with no dll, remove() one
         * without clearing its columns, or set() a cell by hand - breaking
         * the invariant Load/Unload exist to hold. Re-export anything that
         * turns out to be genuinely needed, one `using` at a time. */
        using typename Base::Entity;

        /**
         * @brief Opens the dll, adds its row, fills the columns it provides.
         *
         * A key already in use is refused, so a row is never ambiguous.
         *
         * @param path
         * @param key
         * @return bool
         */
        bool Load(const std::string &path, const std::string &key) {
            if (_byKey.count(key))
                return false;
            auto lib = std::make_unique<SharedLibrary>(path);
            SharedLibrary &ref = *lib;
            Entity e = this->add(std::move(lib));
            (Discover<Ts>(ref, e), ...);
            _byKey.emplace(key, e);
            return true;
        }

        /**
         * @brief Clears this row's columns AND its identity, which closes
         *        the dll. One call, nothing left to tidy up.
         *
         * @param key
         */
        void Unload(const std::string &key) {
            auto it = _byKey.find(key);

            if (it == _byKey.end())
                return;
            this->remove(it->second); /* clears columns AND identity -> closes the dll */
            _byKey.erase(it);
        }

        /**
         * @brief The row named by this key, or nothing.
         *
         * @param key
         * @return std::optional<Entity>
         */
        std::optional<Entity> Find(const std::string &key) const {
            auto it = _byKey.find(key);

            return it == _byKey.end() ? std::nullopt : std::optional<Entity>(it->second);
        }

        /**
         * @brief The T module of the library named by this key, or nullptr.
         *
         * nullptr covers both "no such key" and "this library does not
         * provide T" — neither is an error.
         *
         * @tparam T
         * @param key
         * @return T*
         */
        template <typename T>
        T *Get(const std::string &key) {
            auto e = Find(key);
            return e ? Get<T>(*e) : nullptr;
        }

        /**
         * @brief The T module of this exact row, or nullptr.
         *
         * @tparam T
         * @param e
         * @return T*
         */
        template <typename T>
        T *Get(Entity e) {
            auto &slot = this->template at<T *>(e);
            return slot ? *slot : nullptr;
        }

        /**
         * @brief The SharedLibrary of this exact row, or nullptr.
         *
         * @param e
         * @return SharedLibrary*
         */
        SharedLibrary *Get(Entity e) {
            auto &id = this->identity(e);
            return id ? id->get() : nullptr;
        }

        /**
         * @brief Every library providing T.
         *
         * @tparam T
         * @return std::vector<T *>
         */
        template <typename T>
        std::vector<T *> GetAll() {
            std::vector<T *> out;

            for (auto &slot : this->template column<T *>())
                if (slot)
                    out.push_back(*slot);
            return out;
        }

        /**
         * @brief Every module the library named by this key provides.
         *
         * Type-erased : you get the cells of that row without naming their
         * columns. Useful to list what a library offers ; to actually use
         * one, Get<T>(key) hands it back with its real type.
         *
         * @param key
         * @return std::vector<IModule *>
         */
        std::vector<IModule *> GetAllByKey(const std::string &key) override {
            std::vector<IModule *> out;
            auto e = Find(key);

            if (!e)
                return out;
            this->row(*e, [&](auto &slot) {
                if (slot)
                    out.push_back(static_cast<IModule *>(*slot));
            });
            return out;
        }

        /**
         * @brief Every module of every loaded library.
         *
         * The list you hand to something compiled apart - a game in a dll -
         * which cannot name a contract by template parameter. It sorts them
         * out with IModule::type() and name().
         *
         * @return std::vector<IModule *>
         */
        std::vector<IModule *> GetAll() override {
            std::vector<IModule *> out;

            for (const auto &[key, e] : _byKey)
                this->row(e, [&](auto &slot) {
                    if (slot)
                        out.push_back(static_cast<IModule *>(*slot));
                });
            return out;
        }

        /**
         * @brief La case, par deux chaines. Impose par IModuleRegistry.
         *
         * SEUL endroit ou ce manager lit type(). Il ne s'en sert jamais pour
         * son propre rangement - ses colonnes sont des types statiques. Il ne
         * le lit que pour repondre a une question posee en chaines, par
         * quelqu'un qui n'a pas les types sous la main.
         *
         * O(colonnes) au lieu de O(1) : la voie rapide de ce manager reste
         * Get<T>(cle).
         *
         * @param type
         * @param key
         * @return IModule*
         */
        IModule *Get(const std::string &type, const std::string &key) override {
            for (IModule *module : GetAllByKey(key))
                if (type == module->type())
                    return module;
            return nullptr;
        }

        /** @brief Le module en service pour ce contrat, ou nullptr. */
        IModule *Current(const std::string &type) override {
            const auto found = _current.find(type);

            return found == _current.end() ? nullptr : found->second;
        }

        /** @brief L'hote declare son choix. Un seul par contrat. */
        void Select(const std::string &type, IModule *module) override {
            if (module)
                _current[type] = module;
            else
                _current.erase(type);
        }

        /**
         * @brief La ligne, par chaine. Impose par IModuleRegistry.
         *
         * @param type
         * @return std::vector<IModule *>
         */
        std::vector<IModule *> GetAllByType(const std::string &type) override {
            std::vector<IModule *> found;

            for (IModule *module : GetAll())
                if (type == module->type())
                    found.push_back(module);
            return found;
        }

    private:
        /** @brief Fills one column, if the dll exports that contract's entry. */
        template <typename T>
        void Discover(SharedLibrary &lib, Entity e) {
            auto entry = lib.symbol<T *(*)()>(T::entry);

            if (!entry)
                return;

            T *module = entry();

            // le registre avant la mise en table, comme IModuleManager
            module->bind(*this);
            this->template set<T *>(e, module);
        }

        /** @brief key -> row. The only thing Stride cannot answer itself. */
        std::unordered_map<std::string, Entity> _byKey;

        /** @brief contrat -> module en service. */
        std::map<std::string, IModule *> _current;
};

/** @} */

#endif /* !MODULE_MANAGER_HPP */
