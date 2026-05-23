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
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace sudoku::core {

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

    /// Resolve a dependency of type T
    template <typename T>
    std::shared_ptr<T> resolve() {
        auto type_id = std::type_index(typeid(T));

        // Check for existing singleton instance
        auto instance_it = instances_.find(type_id);
        if (instance_it != instances_.end()) {
            return std::static_pointer_cast<T>(instance_it->second);
        }

        // Check for transient factory
        auto transient_it = transient_factories_.find(type_id);
        if (transient_it != transient_factories_.end()) {
            return std::static_pointer_cast<T>(transient_it->second());
        }

        // Check for singleton factory
        auto factory_it = factories_.find(type_id);
        if (factory_it != factories_.end()) {
            auto instance = factory_it->second();
            instances_[type_id] = instance;  // Cache for future use
            return std::static_pointer_cast<T>(instance);
        }

        return nullptr;
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
    std::unordered_map<std::type_index, std::shared_ptr<void>> instances_;
    std::unordered_map<std::type_index, std::function<std::shared_ptr<void>()>> factories_;
    std::unordered_map<std::type_index, std::function<std::shared_ptr<void>()>> transient_factories_;
};

/// Global container instance for easy access
/// In a real application, consider using a different pattern for DI
extern DIContainer& getContainer();

/// Convenience function to resolve dependencies
template <typename T>
std::shared_ptr<T> resolve() {
    return getContainer().resolve<T>();
}

}  // namespace sudoku::core