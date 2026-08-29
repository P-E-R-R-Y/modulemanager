/**
 * @file Stride.hpp
 *
 * @addtogroup modulemanager
 * @{
 */

#ifndef STRIDE_HPP
#define STRIDE_HPP

#include <optional>
#include <tuple>
#include <vector>


/**
 * @brief A two-axis table : rows are a dense index (0,1,2... reused, like
 *        ecs Entity), columns are Columns... (fixed, known at compile time,
 *        one vector<optional<T>> per type — no hashing). Each row also
 *        carries an Identity, whatever the caller wants a row to own or be
 *        keyed by — a SharedLibrary, a string, anything. Stride does not
 *        know or care what Identity is.
 * @class Stride
 * @tparam Identity
 * @tparam Columns
 *
 *     Stride<Identity, A, B, C, D> t;
 *     auto e = t.add(Identity{...});   // a new row, owning this identity
 *     t.set<B>(e, B{...});             // fill column B for this row
 *
 *     t.column<B>();                   // the WHOLE B column — no copy, no rebuild
 *     t.row(e, visitor);                // the WHOLE row e — a fold over Columns..., no copy
 *     t.identity(e);                    // the Identity of this row
 *
 * The index is hidden behind Entity : private construction, Stride is the
 * only friend. No way to fabricate an Entity by hand, and an Entity from
 * Stride<X,A,B> is NOT the same type as one from Stride<Y,C,D> — passing it
 * to the wrong Stride is a COMPILE error, not a runtime bug.
 */
template <typename Identity, typename... Columns>
class Stride {
    public:
        /**
         * @brief A row handle. Only Stride can create one.
         * @class Entity
         */
        class Entity {
            public:
                friend class Stride;
                bool operator==(const Entity &o) const { return _idx == o._idx; }

            private:
                explicit Entity(size_t idx) : _idx(idx) {}
                size_t _idx;
        };

        /**
         * @brief A new row, owning identity. Every column grows (or a freed
         *        index is reused).
         * @param identity
         * @return Entity
         */
        Entity add(Identity identity) {
            size_t idx;
            if (!_free.empty()) {
                idx = _free.back();
                _free.pop_back();
            } else {
                idx = _size++;
                (std::get<std::vector<std::optional<Columns>>>(_columns).emplace_back(), ...);
                _identities.emplace_back();
            }
            _identities[idx] = std::move(identity);
            return Entity(idx);
        }

        /**
         * @brief Clears every column at this row and its identity. The
         *        index becomes reusable.
         * @param e
         */
        void remove(Entity e) {
            (std::get<std::vector<std::optional<Columns>>>(_columns)[e._idx].reset(), ...);
            _identities[e._idx].reset();
            _free.push_back(e._idx);
        }

        /**
         * @brief The Identity owning this row.
         * @param e
         * @return std::optional<Identity>&
         */
        std::optional<Identity> &identity(Entity e) {
            return _identities[e._idx];
        }

        /**
         * @brief Writes the T value of this row.
         * @tparam T
         * @param e
         * @param value
         */
        template <typename T>
        void set(Entity e, T value) {
            std::get<std::vector<std::optional<T>>>(_columns)[e._idx] = std::move(value);
        }

        /**
         * @brief The T value of this row, or nullopt.
         * @tparam T
         * @param e
         * @return std::optional<T>&
         */
        template <typename T>
        std::optional<T> &at(Entity e) {
            return std::get<std::vector<std::optional<T>>>(_columns)[e._idx];
        }

        /**
         * @brief The WHOLE T column — a vector already there, nothing to rebuild.
         * @tparam T
         * @return std::vector<std::optional<T>>&
         */
        template <typename T>
        std::vector<std::optional<T>> &column() {
            return std::get<std::vector<std::optional<T>>>(_columns);
        }

        /**
         * @brief Visits the WHOLE row, one column at a time. No copy.
         * @tparam F
         * @param e
         * @param f
         */
        template <typename F>
        void row(Entity e, F &&f) {
            (f(std::get<std::vector<std::optional<Columns>>>(_columns)[e._idx]), ...);
        }

        /** @brief The number of live rows. */
        size_t size() const { return _size; }

    private:
        std::tuple<std::vector<std::optional<Columns>>...> _columns;
        std::vector<std::optional<Identity>> _identities;
        std::vector<size_t> _free;
        size_t _size = 0;
};

/** @} */

#endif /* !STRIDE_HPP */
