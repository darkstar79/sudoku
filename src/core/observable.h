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
#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
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

/// Observable wrapper for any value type.
///
/// Subscriber lifetime contract: callback-based subscribers (closures registered via
/// `subscribe(CallbackFn)`) must outlive any captured references for the lifetime of the
/// subscription. The returned unsubscribe lambda — or `clearSubscriptions()` — must be
/// invoked from the subscriber's owner before that owner is destroyed; otherwise a later
/// notification can call the closure with a dangling capture. Interface-based subscribers
/// (`subscribe(ObserverPtr)`) hold a weak_ptr internally, so a destroyed observer is
/// detected and skipped automatically.
///
/// Reentrancy: subscribers may call `set()` / `update()` / `subscribe()` / `unsubscribe()`
/// / `clearSubscriptions()` from inside a notification callback. Reentrant value changes
/// are queued and drained after the outer notification completes, so every subscriber in a
/// given batch observes the same value.
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

    /// Set new value and notify observers.
    ///
    /// Reentrancy: if called from inside a subscriber's callback (during a notification),
    /// the new value is queued in `pending_value_` and the outer notification continues with
    /// the unchanged `value_`. The queued value is applied and re-notified after the outer
    /// notification drains. Repeated reentrant sets coalesce — only the latest value is kept.
    void set(T new_value) {
        if (notifying_) {
            pending_value_ = std::move(new_value);
            return;
        }
        if (value_ != new_value) {
            value_ = std::move(new_value);
            notifyAndDrain();
        }
    }

    /// Update value in-place and notify observers.
    ///
    /// Reentrancy: see `set()`. During a notification, the updater is applied to a working
    /// copy (built from any pending value if present, otherwise from `value_`) and queued.
    template <typename Func>
    void update(Func&& updater) {
        if (notifying_) {
            T working = pending_value_.has_value() ? *pending_value_ : value_;
            std::forward<Func>(updater)(working);
            pending_value_ = std::move(working);
            return;
        }
        T old_value = value_;
        std::forward<Func>(updater)(value_);
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4702)  // MSVC false positive: unreachable code in template instantiation
#endif
        if (value_ != old_value) {
            notifyAndDrain();
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
    bool notifying_{false};
    std::optional<T> pending_value_;

    /// Run one notification pass, then drain any reentrant set()/update() calls that
    /// arrived during that pass. Chains of reentrant updates are handled by looping until
    /// no more pending value remains.
    ///
    /// Exception safety: if any subscriber callback throws, the RAII guard resets
    /// `notifying_` and discards `pending_value_` before rethrowing, so the Observable is
    /// not wedged into a permanent "notifying" state in which all future writes would be
    /// silently queued. Reentrant intent queued before the throw is discarded — only
    /// completed operations contribute to subsequent state.
    void notifyAndDrain() {
        struct ResetOnExit {
            Observable* self;
            explicit ResetOnExit(Observable* s) : self(s) {
            }
            ~ResetOnExit() {
                self->notifying_ = false;
                self->pending_value_.reset();
            }
            ResetOnExit(const ResetOnExit&) = delete;
            ResetOnExit& operator=(const ResetOnExit&) = delete;
            ResetOnExit(ResetOnExit&&) = delete;
            ResetOnExit& operator=(ResetOnExit&&) = delete;
        } guard(this);

        notifying_ = true;
        notifyObservers();
        // Defensive cap on drain iterations. A well-behaved subscriber chain terminates in
        // a small number of steps (typically 1); a runaway loop here means a subscriber is
        // unconditionally writing a fresh value on every notification — a logic bug we
        // surface as an assertion in debug and a graceful bail-out in release.
        constexpr int kMaxDrainDepth = 64;
        int depth = 0;
        while (pending_value_.has_value()) {
            assert(++depth <= kMaxDrainDepth &&
                   "Observable drain loop exceeded max depth (pathological reentrant subscriber)");
            if (depth > kMaxDrainDepth) {
                return;  // guard discards remaining pending_value_
            }
            T queued = std::move(*pending_value_);
            pending_value_.reset();
            if (value_ != queued) {
                value_ = std::move(queued);
                notifyObservers();
            }
        }
    }

    void notifyObservers() {
        // Bind a reference rather than copying — reentrant set()/update() are deferred by
        // notifyAndDrain() (notifying_ gate), so value_ is provably stable across this
        // call. Avoiding the copy matters for large T (e.g., GameState, UIState) on hot
        // notification paths.
        const T& snapshot = value_;

        // Snapshot interface observers (weak_ptrs) so we can safely iterate even if a
        // callback triggers unsubscribe()/clearSubscriptions() — both mutate observers_ via
        // std::erase_if/clear(), invalidating any live iterator into observers_.
        auto observers_copy = observers_;
        for (auto& weak_obs : observers_copy) {
            auto observer = weak_obs.lock();
            if (!observer) {
                continue;
            }
            // Gate on the live observers_: if the callback unsubscribed this observer
            // earlier in the batch (or destroyed its parent View), don't invoke it.
            // Mirrors the callbacks_.count(id) > 0 guard on the callback path. This is
            // O(N) per observer = O(N²) per batch; acceptable for the small subscriber
            // counts seen in single-View MVVM. If N ever grows past a couple dozen, switch
            // to a side-table of raw pointers.
            const bool still_subscribed =
                std::any_of(observers_.begin(), observers_.end(),
                            [&](const std::weak_ptr<IObserver<T>>& w) { return w.lock() == observer; });
            if (still_subscribed) {
                observer->onNotify(snapshot);
            }
        }
        // Prune expired weak_ptrs (was previously done inline in the iteration loop).
        std::erase_if(observers_, [](const std::weak_ptr<IObserver<T>>& w) { return w.expired(); });

        // Notify callback-based observers. Snapshot the map so callbacks that mutate
        // callbacks_ (subscribe/unsubscribe/clearSubscriptions) don't invalidate iteration.
        auto callbacks_copy = callbacks_;
        for (const auto& [id, callback] : callbacks_copy) {
            if (callbacks_.count(id) > 0) {
                callback(snapshot);
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