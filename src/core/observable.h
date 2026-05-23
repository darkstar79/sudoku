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

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace sudoku::core {

/// Observer interface for the Observer pattern
template <typename T>
class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void onNotify(const T& value) = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    IObserver() = default;
    IObserver(const IObserver&) = default;
    IObserver& operator=(const IObserver&) = default;
    IObserver(IObserver&&) = default;
    IObserver& operator=(IObserver&&) = default;
};

/// Observable wrapper for any value type
template <typename T>
class Observable {
public:
    using ObserverPtr = std::shared_ptr<IObserver<T>>;
    using CallbackFn = std::function<void(const T&)>;
    using CallbackId = size_t;

    /// Construct with initial value
    explicit Observable(T initial_value = T{}) : value_(std::move(initial_value)) {
    }

    /// Get current value (const)
    const T& get() const {
        return value_;
    }

    /// Set new value and notify observers
    void set(T new_value) {
        if (value_ != new_value) {
            value_ = std::move(new_value);
            notifyObservers();
        }
    }

    /// Update value in-place and notify observers
    template <typename Func>
    void update(Func&& updater) {
        T old_value = value_;
        std::forward<Func>(updater)(value_);
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4702)  // MSVC false positive: unreachable code in template instantiation
#endif
        if (value_ != old_value) {
            notifyObservers();
        }
#ifdef _MSC_VER
#    pragma warning(pop)
#endif
    }

    /// Subscribe with observer interface
    void subscribe(ObserverPtr observer) {
        if (observer) {
            observers_.push_back(observer);
        }
    }

    /// Subscribe with callback function
    std::function<void()> subscribe(const CallbackFn& callback) {
        if (callback) {
            CallbackId id = next_callback_id_++;
            callbacks_[id] = callback;

            // Return unsubscribe function
            return [this, id]() { callbacks_.erase(id); };
        }
        return []() {};  // Empty function if callback is null
    }

    /// Unsubscribe observer
    void unsubscribe(ObserverPtr observer) {
        std::erase_if(observers_,
                      [&observer](const std::weak_ptr<IObserver<T>>& weak_obs) { return weak_obs.lock() == observer; });
    }

    /// Clear all subscriptions
    void clearSubscriptions() {
        observers_.clear();
        callbacks_.clear();
    }

    /// Get number of active subscriptions
    [[nodiscard]] size_t getSubscriptionCount() const {
        return observers_.size() + callbacks_.size();
    }

    /// Explicit conversion to T
    /// Use .get() for accessing value instead of implicit conversion
    explicit operator T() const {
        return value_;
    }

    /// Assignment operator with notification
    Observable& operator=(const T& new_value) {
        set(new_value);
        return *this;
    }

    /// Assignment operator with notification
    Observable& operator=(T&& new_value) {
        set(std::move(new_value));
        return *this;
    }

private:
    T value_;
    std::vector<std::weak_ptr<IObserver<T>>> observers_;
    std::unordered_map<CallbackId, CallbackFn> callbacks_;
    CallbackId next_callback_id_{0};

    void notifyObservers() {
        // Notify interface-based observers
        auto it = observers_.begin();
        while (it != observers_.end()) {
            if (auto observer = it->lock()) {
                observer->onNotify(value_);
                ++it;
            } else {
                // Remove expired weak_ptr
                it = observers_.erase(it);
            }
        }

        // Notify callback-based observers
        // Make a copy to avoid iterator invalidation if callbacks modify the map
        auto callbacks_copy = callbacks_;
        for (const auto& [id, callback] : callbacks_copy) {
            // Check if callback still exists (might have been removed during notification)
            if (callbacks_.count(id) > 0) {
                callback(value_);
            }
        }
    }
};

/// Convenience function to create observable
template <typename T>
Observable<T> makeObservable(T initial_value = T{}) {
    return Observable<T>(std::move(initial_value));
}

/// Helper class for observing multiple observables
class CompositeObserver {
public:
    template <typename T, typename Callable>
    void observe(Observable<T>& observable, Callable&& callback) {
        observable.subscribe(std::function<void(const T&)>(std::forward<Callable>(callback)));
        // Intentional: in single-view MVVM, MainWindow is the sole subscriber per
        // observable, so clearSubscriptions() is equivalent to per-callback unsubscribe.
        // If multiple views ever subscribe to the same observable, replace with the
        // per-callback unsubscribe token returned by subscribe().
        observables_.push_back([&observable]() { observable.clearSubscriptions(); });
    }

    /// Unsubscribe from all observables
    void unsubscribeAll() {
        for (auto& unsubscribe : observables_) {
            unsubscribe();
        }
        observables_.clear();
    }

    CompositeObserver() = default;
    ~CompositeObserver() {
        unsubscribeAll();
    }
    CompositeObserver(const CompositeObserver&) = delete;
    CompositeObserver& operator=(const CompositeObserver&) = delete;
    CompositeObserver(CompositeObserver&&) = delete;
    CompositeObserver& operator=(CompositeObserver&&) = delete;

private:
    std::vector<std::function<void()>> observables_;
};

}  // namespace sudoku::core