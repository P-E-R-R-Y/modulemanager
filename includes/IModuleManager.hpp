/**
 * @file IModuleManager.hpp
 *
 * @addtogroup modulemanager
 * @{
 */

#ifndef IMODULE_MANAGER_HPP
#define IMODULE_MANAGER_HPP

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "IModule.hpp"
#include "IModuleRegistry.hpp"
#include "SharedLibrary.hpp"

/**
 * @class IModuleManager
 * @brief La meme table que ModuleManager, mais dont les DEUX axes sont des
 *        chaines connues seulement au chargement.
 *
 * ModuleManager<Ts...> est plus agreable : ses colonnes sont des types,
 * donc Get<IGraphic2Module>() est verifie par le compilateur. Son prix est
 * qu'il ne charge que ce qu'on a su nommer. Une dll qui fournit un contrat
 * que l'hote ignore reste muette - aucun Discover<T> ne lui correspond,
 * donc rien n'est construit.
 *
 * Ici, la ligne vient de IModule::type() et la colonne est la cle de
 * chargement :
 *
 *                | raylib             | sfml              | asio
 *     -----------|--------------------|-------------------|-------------------
 *      graphic   | RayGraphicModule   | SfmlGraphicModule |
 *      audio     | RayAudioModule     | SfmlAudioModule   |
 *      network   |                    |                   | AsioNetworkModule
 *
 * MEME DISCIPLINE DE RANGEMENT QUE STRIDE, et donc le meme nom : le stockage
 * est colonne par colonne, chacune contigue et indexee par le numero de
 * ligne. Une case est deux resolutions de chaine puis un calcul d'indice.
 *
 *     Get(type, cle)   O(1)
 *     GetAllByKey(cle) la colonne entiere, deja contigue - rien a rebatir
 *     GetAllByType(type)  la ligne : un pas de colonne en colonne, a offset fixe
 *
 * Ce n'est PAS un Stride<Identity, Columns...> : celui-la range un
 * std::vector par TYPE de colonne, via std::get sur un tuple, resolu a la
 * compilation. Des colonnes decouvertes au runtime ne peuvent pas s'y
 * ranger - seule la disposition memoire est reprise.
 *
 * Une table de hachage imbriquee donnerait la meme reponse, mais parcourir
 * une ligne obligerait a visiter la table de chaque colonne. Ici c'est un
 * pas d'indice.
 */
class IModuleManager : public IModuleRegistry {

    public:
        /**
         * @brief Le seul symbole que la dll doit exporter.
         *
         * Un nom unique, le meme partout, connu sans connaitre le contrat -
         * c'est ce qui rend le chargement anonyme possible. Avec un symbole
         * par contrat, il faudrait deja savoir quoi chercher.
         *
         * @code
         * extern "C" IModule **getModules() {
         *     static SfmlGraphicModule graphic;
         *     static SfmlAudioModule audio;
         *     static IModule *list[] = { &graphic, &audio, nullptr };
         *     return list;
         * }
         * @endcode
         *
         * Un IModule ** termine par nullptr, et pas un std::vector : un
         * vecteur ne traverse pas un dlopen sans supposer que la dll et
         * l'hote partagent exactement la meme ABI de bibliotheque standard
         * et le meme allocateur. Deux pointeurs ne supposent rien.
         *
         * Le tableau appartient a la dll et vit aussi longtemps qu'elle.
         */
        static constexpr const char *entry = "getModules";

        ~IModuleManager() override {
            /* Les modules vivent dans les bibliotheques : on vide la table
             * avant de les fermer, sinon on garderait des pointeurs vers du
             * code demappe le temps de la destruction. */
            _cells.clear();
            _columns.clear();
        }

        /**
         * @brief Ouvre la dll, appelle getModules(), ajoute une colonne.
         *
         * @param path
         * @param key le nom de la colonne
         * @return false si la cle est prise, si getModules() manque, ou si
         *         la dll ne fournit rien
         */
        bool Load(const std::string &path, const std::string &key) {
            /* Une cle condamnee n'est pas reutilisable tant qu'elle n'a pas
             * ferme : la ressusciter obligerait a distinguer "condamnee mais
             * revenue" de "jamais condamnee", pour un cas qui n'arrive que si
             * on recharge dans le meme souffle qu'on decharge. */
            if (_columnOf.count(key))
                return false;

            auto library = std::make_unique<SharedLibrary>(path);
            auto get = library->symbol<IModule **(*)()>(entry);

            if (!get)
                return false;   // pas un fournisseur de modules

            IModule **modules = get();

            if (!modules || !*modules)
                return false;

            const size_t column = _cells.size();

            _cells.emplace_back(_rows.size(), nullptr);
            for (IModule **it = modules; *it; it++) {
                /* rowOf() peut agrandir toutes les colonnes : on resout la
                 * ligne AVANT d'indexer, plutot que de dependre de l'ordre
                 * d'evaluation d'un operator[]. */
                const size_t row = rowOf((*it)->type());

                /* Le registre AVANT la mise en table : le module doit
                 * pouvoir chercher ses voisins des l'instant ou il devient
                 * atteignable, pas un appel plus tard. */
                (*it)->bind(*this);

                /* A neuf, meme si l'objet a deja vecu : sur macOS dlclose ne
                 * decharge souvent pas, donc ce module peut etre exactement
                 * celui qu'on a condamne tout a l'heure. */
                (*it)->reset();
                _cells[column][row] = *it;
            }

            _columnOf.emplace(key, column);
            _columnNames.push_back(key);
            _columns.push_back(std::move(library));
            _condemned.push_back(false);
            return true;
        }

        /**
         * @brief La case : ce type, chez ce fournisseur. O(1).
         *
         * @param type
         * @param key
         * @return IModule* nullptr si la case est vide
         */
        IModule *Get(const std::string &type, const std::string &key) override {
            const auto row = _rowOf.find(type);
            const auto column = _columnOf.find(key);

            if (row == _rowOf.end() || column == _columnOf.end())
                return nullptr;
            return _cells[column->second][row->second];
        }

        /**
         * @brief La LIGNE : tous ceux qui declarent ce type.
         *
         *     GetAllByType("audio")  ->  RayAudioModule, SfmlAudioModule
         *
         * Un pas de colonne en colonne, a offset fixe. C'est la lecture qui
         * donne son nom au motif.
         *
         * @param type
         * @return std::vector<IModule *>
         */
        std::vector<IModule *> GetAllByType(const std::string &type) override {
            std::vector<IModule *> found;
            const auto row = _rowOf.find(type);

            if (row == _rowOf.end())
                return found;
            for (const std::vector<IModule *> &column : _cells)
                if (IModule *module = column[row->second])
                    found.push_back(module);
            return found;
        }

        /**
         * @brief La COLONNE : tout ce que ce fournisseur apporte.
         *
         *     GetAllByKey("raylib")  ->  RayGraphicModule, RayAudioModule
         *
         * Deja contigue en memoire, rien a rebatir.
         *
         * @param key
         * @return std::vector<IModule *>
         */
        std::vector<IModule *> GetAllByKey(const std::string &key) override {
            std::vector<IModule *> found;
            const auto column = _columnOf.find(key);

            if (column == _columnOf.end())
                return found;
            for (IModule *module : _cells[column->second])
                if (module)
                    found.push_back(module);
            return found;
        }

        /**
         * @brief Toute la table, colonne par colonne.
         *
         * @return std::vector<IModule *>
         */
        std::vector<IModule *> GetAll() override {
            std::vector<IModule *> found;

            for (const std::vector<IModule *> &column : _cells)
                for (IModule *module : column)
                    if (module)
                        found.push_back(module);
            return found;
        }

        /**
         * @brief Condamne une colonne. NE FERME RIEN.
         *
         * Chaque module de la dll passe a mustClose(). Ses detenteurs le
         * verront a leur prochain tick, detruiront ce qu'ils tiennent et
         * appelleront release(). La fermeture, elle, revient a Reconcile().
         *
         * Entre les deux, la dll reste ouverte et tout ce qu'elle a fabrique
         * reste valide - c'est precisement ce qui laisse le temps de lacher
         * proprement.
         *
         * @param key
         */
        void Unload(const std::string &key) {
            const auto column = _columnOf.find(key);

            if (column == _columnOf.end())
                return;

            _condemned[column->second] = true;
            for (IModule *module : _cells[column->second])
                if (module)
                    module->condemn();
        }

        /**
         * @brief Ferme ce qui est condamne et que plus personne ne tient.
         *
         * A appeler a chaque tick. Une colonne dont un seul module a encore
         * un detenteur est laissee telle quelle : on repassera au tick
         * suivant. C'est toute l'attente du mecanisme - il n'y a ni blocage
         * ni thread, seulement une fermeture qui n'a pas lieu ce tour-ci.
         *
         * @return le nombre de dll effectivement fermees
         */
        size_t Reconcile() {
            size_t closed = 0;

            for (size_t column = 0; column < _cells.size(); ) {
                if (!_condemned[column] || !isFree(column)) {
                    column++;
                    continue;
                }

                /* Les cellules d'abord, la bibliotheque ensuite : elles
                 * pointent dedans, et detruire l'unique_ptr appelle dlclose. */
                /* Une selection qui pointe dans cette colonne devient
                 * caduque : elle designerait du code demappe. */
                for (auto it = _current.begin(); it != _current.end(); )
                    it = contains(column, it->second) ? _current.erase(it) : std::next(it);

                _cells.erase(_cells.begin() + column);
                _columnNames.erase(_columnNames.begin() + column);
                _condemned.erase(_condemned.begin() + column);
                _columns.erase(_columns.begin() + column);

                reindex();
                closed++;
            }
            return closed;
        }

        /** @brief Le module en service pour ce contrat, ou nullptr. */
        IModule *Current(const std::string &type) override {
            const auto found = _current.find(type);

            return found == _current.end() ? nullptr : found->second;
        }

        /** @brief L'hote declare son choix. Un seul par type. */
        void Select(const std::string &type, IModule *module) override {
            if (module)
                _current[type] = module;
            else
                _current.erase(type);
        }

        /* ---- les memes questions, posees avec un type ---------------- *
         *
         * C'est tout ce que ModuleManager<Ts...> apportait de plus, sans sa
         * contrainte : lui devait connaitre ses colonnes a la compilation
         * et faisait un dlsym par contrat. Ici la table reste decouverte au
         * chargement, et le type ne sert qu'a nommer la ligne et a rendre
         * le cast.
         *
         * static_cast et pas dynamic_cast : sous RTLD_LOCAL le typeinfo
         * n'est pas partage entre la dll et l'hote, donc dynamic_cast
         * repondrait faux. Le static_cast est sur parce que les deux cotes
         * ont compile les memes en-tetes - la disposition des classes et
         * l'ajustement vers la base sont resolus a la compilation. Ce a
         * quoi il fait confiance, c'est la chaine rendue par type().
         */

        /**
         * @brief Ce contrat chez ce fournisseur, ou tout ce qui le satisfait.
         *
         * Parcourt T::accepts : demander un IGraphic2Module trouve aussi un
         * vendor qui ne declare que "graphic3", puisqu'il en est un.
         */
        template <typename T>
        T *Get(const std::string &key) {
            for (const char *const *type = T::accepts; *type; type++)
                if (IModule *module = Get(*type, key))
                    return static_cast<T *>(module);
            return nullptr;
        }

        /** @brief Tous ceux qui remplissent ce contrat, ou mieux. */
        template <typename T>
        std::vector<T *> GetAllByType() {
            std::vector<T *> found;

            for (const char *const *type = T::accepts; *type; type++)
                for (IModule *module : GetAllByType(*type))
                    found.push_back(static_cast<T *>(module));
            return found;
        }

        /** @brief Le module en service pour ce contrat, vu comme un T. */
        template <typename T>
        T *Current() {
            return static_cast<T *>(Current(T::contract));
        }

        /** @brief L'hote declare son choix pour le contrat de T. */
        template <typename T>
        void Select(T *module) {
            Select(T::contract, module);
        }

        /** @brief Lignes, dans l'ordre de decouverte. */
        const std::vector<std::string> &GetTypes() const { return _rows; }

        /** @brief Les colonnes, dans l'ordre de chargement. */
        const std::vector<std::string> &GetKeys() const { return _columnNames; }

    private:
        /** @brief Ce module est-il dans cette colonne ? */
        bool contains(size_t column, IModule *module) const {
            for (IModule *cell : _cells[column])
                if (cell && cell == module)
                    return true;
            return false;
        }

        /** @brief Plus aucun module de cette colonne n'est tenu. */
        bool isFree(size_t column) const {
            for (IModule *module : _cells[column])
                if (module && !module->isClosed())
                    return false;
            return true;
        }

        /** @brief Les colonnes ont bouge : on refait la correspondance. */
        void reindex() {
            _columnOf.clear();
            for (size_t column = 0; column < _columnNames.size(); column++)
                _columnOf.emplace(_columnNames[column], column);
        }

        /**
         * @brief L'indice de cette ligne, creee si elle n'existe pas encore.
         *
         * Ajouter une ligne fait grandir TOUTES les colonnes : c'est le prix
         * d'une table dense, et il se paie une fois par famille de modules,
         * pas une fois par module.
         */
        size_t rowOf(const std::string &type) {
            const auto found = _rowOf.find(type);

            if (found != _rowOf.end())
                return found->second;

            const size_t row = _rows.size();

            _rowOf.emplace(type, row);
            _rows.push_back(type);
            for (std::vector<IModule *> &column : _cells)
                column.resize(row + 1, nullptr);
            return row;
        }

        /* Rangement colonne par colonne, comme Stride : _cells[colonne][ligne].
         * Une colonne est contigue, une ligne se parcourt a offset fixe. */
        std::vector<std::vector<IModule *>> _cells;
        std::vector<std::unique_ptr<SharedLibrary>> _columns;
        std::vector<bool> _condemned;   ///< colonne -> Unload demande
        std::map<std::string, IModule *> _current;   ///< contrat -> en service

        std::vector<std::string> _rows;         ///< ligne  -> type
        std::vector<std::string> _columnNames;  ///< colonne -> cle
        std::unordered_map<std::string, size_t> _rowOf;
        std::unordered_map<std::string, size_t> _columnOf;
};

/** @} */

#endif /* !IMODULE_MANAGER_HPP */
