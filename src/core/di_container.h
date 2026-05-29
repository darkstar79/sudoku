// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace sudoku::core {

/// Base class for all DIContainer wiring errors.
///
/// These represent programmer errors in the dependency graph (a missing
/// registration or a cyclic factory), not recoverable runtime conditions —
/// hence an exception rather than std::expected. They fail loudly at the
/// resolution site instead of returning a silent nullptr that segfaults later.
class DIContainerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Thrown by resolve<T>() when no registration exists for the requested type.
class DIResolutionError : public DIContainerError {
public:
    using DIContainerError::DIContainerError;
};

/// Thrown by resolve<T>() / tryResolve<T>() when a factory transitively
/// resolves its own type (a dependency cycle). For a singleton this is detected
/// before the duplicate instance can be cached; for a transient it stops the
/// otherwise-unbounded recursion before it overflows the stack.
class DICycleError : public DIContainerError {
public:
    using DIContainerError::DIContainerError;
};

/// Simple dependency injection container for managing object lifetimes
class DIContainer {
public:
    /// Register a singleton factory function for type T
    template <typename T, typename Factory>
    void registerSingleton(Factory&& factory) {
        static_assert(std::is_abstract_v<T> || std::is_class_v<T>, "T must be a class or interface type");

        auto type_id = std::type_index(typeid(T));
        factories_[type_id] = [factory = std::forward<Factory>(factory)]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(std::shared_ptr<T>(factory().release()));
        };
    }

    /// Register a singleton instance directly
    template <typename T>
    void registerInstance(std::shared_ptr<T> instance) {
        static_assert(std::is_abstract_v<T> || std::is_class_v<T>, "T must be a class or interface type");

        auto type_id = std::type_index(typeid(T));
        instances_[type_id] = std::static_pointer_cast<void>(instance);
    }

    /// Register a transient factory (creates new instance each time)
    template <typename T, typename Factory>
    void registerTransient(Factory&& factory) {
        static_assert(std::is_abstract_v<T> || std::is_class_v<T>, "T must be a class or interface type");

        auto type_id = std::type_index(typeid(T));
        transient_factories_[type_id] = [factory = std::forward<Factory>(factory)]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(std::shared_ptr<T>(factory().release()));
        };
    }

    /// Resolve a dependency of type T.
    ///
    /// Contract: T must have been registered (singleton, transient, or
    /// instance) before resolution. A missing registration is a wiring bug, so
    /// this throws DIResolutionError rather than returning a silent nullptr
    /// that would segfault far from the omission site. A singleton factory that
    /// transitively resolves its own type throws DICycleError before the
    /// duplicate instance can be cached. Use tryResolve<T>() for genuinely
    /// optional dependencies where absence is expected.
    template <typename T>
    [[nodiscard]] std::shared_ptr<T> resolve() {
        auto resolution = resolveErased(std::type_index(typeid(T)));
        if (!resolution.registered) {
            throw DIResolutionError(std::string("DIContainer: no registration for type ") + typeid(T).name());
        }
        return std::static_pointer_cast<T>(std::move(resolution.instance));
    }

    /// Resolve a dependency of type T, or return nullptr if it was never
    /// registered. Unlike resolve<T>(), an absent registration is not an error.
    /// Cyclic factories still throw DICycleError.
    template <typename T>
    [[nodiscard]] std::shared_ptr<T> tryResolve() {
        return std::static_pointer_cast<T>(resolveErased(std::type_index(typeid(T))).instance);
    }

    /// Check if a type is registered
    template <typename T>
    bool isRegistered() const {
        auto type_id = std::type_index(typeid(T));
        return instances_.contains(type_id) || factories_.contains(type_id) || transient_factories_.contains(type_id);
    }

    /// Clear all registrations (useful for testing)
    void clear() {
        instances_.clear();
        factories_.clear();
        transient_factories_.clear();
    }

    /// Get count of registered types
    size_t getRegistrationCount() const {
        return instances_.size() + factories_.size() + transient_factories_.size();
    }

private:
    /// Outcome of a type-erased lookup. `registered` distinguishes "no
    /// registration" (resolve throws, tryResolve returns null) from a
    /// registered factory that legitimately produced a null instance.
    struct ResolveResult {
        bool registered;
        std::shared_ptr<void> instance;
    };

    /// RAII marker that records a type as "resolution in progress". It inserts
    /// on construction and exposes whether the insert was fresh via inserted();
    /// a false result means the type was already being resolved — i.e. a cycle.
    /// The marker is cleared on scope exit (including during exception
    /// unwinding), but only by the guard that actually inserted it, so a thrown
    /// cycle never erases the outer in-progress entry it shares.
    class ResolvingGuard {
    public:
        ResolvingGuard(std::unordered_set<std::type_index>& set, std::type_index id)
            : set_(set), id_(id), inserted_(set.insert(id).second) {
        }
        ResolvingGuard(const ResolvingGuard&) = delete;
        ResolvingGuard& operator=(const ResolvingGuard&) = delete;
        ResolvingGuard(ResolvingGuard&&) = delete;
        ResolvingGuard& operator=(ResolvingGuard&&) = delete;
        ~ResolvingGuard() {
            if (inserted_) {
                set_.erase(id_);
            }
        }

        [[nodiscard]] bool inserted() const {
            return inserted_;
        }

    private:
        std::unordered_set<std::type_index>& set_;
        std::type_index id_;
        bool inserted_;
    };

    /// Shared resolution logic for resolve<T>() and tryResolve<T>(), operating
    /// on the erased type_index. Throws DICycleError on re-entrant factory
    /// resolution (singleton or transient).
    ResolveResult resolveErased(std::type_index type_id) {
        // Check for existing singleton instance
        auto instance_it = instances_.find(type_id);
        if (instance_it != instances_.end()) {
            return {.registered = true, .instance = instance_it->second};
        }

        // Check for transient factory
        auto transient_it = transient_factories_.find(type_id);
        if (transient_it != transient_factories_.end()) {
            // A transient factory that re-enters its own type would recurse
            // until the stack overflows; the in-progress marker turns that into
            // a clear throw instead.
            ResolvingGuard guard(resolving_, type_id);
            if (!guard.inserted()) {
                throw DICycleError(std::string("DIContainer: cyclic transient resolution for type ") + type_id.name());
            }
            return {.registered = true, .instance = transient_it->second()};
        }

        // Check for singleton factory
        auto factory_it = factories_.find(type_id);
        if (factory_it != factories_.end()) {
            // Mark in-progress before invoking the factory. A re-entrant resolve
            // of the same type (a cycle) would otherwise re-run the factory and
            // overwrite the cached instance, yielding two divergent "singletons".
            ResolvingGuard guard(resolving_, type_id);
            if (!guard.inserted()) {
                throw DICycleError(std::string("DIContainer: cyclic singleton resolution for type ") + type_id.name());
            }

            auto instance = factory_it->second();
            instances_[type_id] = instance;  // Cache for future use
            return {.registered = true, .instance = instance};
        }

        return {.registered = false, .instance = nullptr};
    }

    std::unordered_map<std::type_index, std::shared_ptr<void>> instances_;
    std::unordered_map<std::type_index, std::function<std::shared_ptr<void>()>> factories_;
    std::unordered_map<std::type_index, std::function<std::shared_ptr<void>()>> transient_factories_;
    std::unordered_set<std::type_index> resolving_;
};

/// Global container instance for easy access
/// In a real application, consider using a different pattern for DI
extern DIContainer& getContainer();

/// Convenience function to resolve dependencies. Throws DIResolutionError if
/// the type was never registered (see DIContainer::resolve).
template <typename T>
[[nodiscard]] std::shared_ptr<T> resolve() {
    return getContainer().resolve<T>();
}

/// Convenience function for optional dependencies: returns nullptr if the type
/// was never registered instead of throwing (see DIContainer::tryResolve).
template <typename T>
[[nodiscard]] std::shared_ptr<T> tryResolve() {
    return getContainer().tryResolve<T>();
}

}  // namespace sudoku::core