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

#include "core/di_container.h"

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// Mock interface for testing
class ITestService {
public:
    virtual ~ITestService() = default;
    virtual int getValue() const = 0;
};

// Mock implementation
class TestService : public ITestService {
public:
    explicit TestService(int value) : value_(value) {
    }
    int getValue() const override {
        return value_;
    }

private:
    int value_;
};

// Mock dependency
class IDependency {
public:
    virtual ~IDependency() = default;
    virtual std::string getName() const = 0;
};

class Dependency : public IDependency {
public:
    explicit Dependency(const std::string& name) : name_(name) {
    }
    std::string getName() const override {
        return name_;
    }

private:
    std::string name_;
};

// Service with dependency
class ServiceWithDependency : public ITestService {
public:
    explicit ServiceWithDependency(std::shared_ptr<IDependency> dep) : dependency_(dep) {
    }
    int getValue() const override {
        return static_cast<int>(dependency_->getName().length());
    }

private:
    std::shared_ptr<IDependency> dependency_;
};

TEST_CASE("DIContainer - Basic Service Registration and Resolution", "[DIContainer]") {
    DIContainer container;

    SECTION("Register and resolve singleton") {
        // Register a singleton service
        container.registerSingleton<ITestService>([]() { return std::make_unique<TestService>(42); });

        // Resolve the service
        auto service1 = container.resolve<ITestService>();
        auto service2 = container.resolve<ITestService>();

        REQUIRE(service1 != nullptr);
        REQUIRE(service2 != nullptr);
        REQUIRE(service1.get() == service2.get());  // Same instance
        REQUIRE(service1->getValue() == 42);
    }

    SECTION("Register and resolve transient") {
        // Register a transient service
        container.registerTransient<ITestService>([]() { return std::make_unique<TestService>(100); });

        // Resolve the service
        auto service1 = container.resolve<ITestService>();
        auto service2 = container.resolve<ITestService>();

        REQUIRE(service1 != nullptr);
        REQUIRE(service2 != nullptr);
        REQUIRE(service1.get() != service2.get());  // Different instances
        REQUIRE(service1->getValue() == 100);
        REQUIRE(service2->getValue() == 100);
    }
}

TEST_CASE("DIContainer - Service Dependencies", "[DIContainer]") {
    DIContainer container;

    SECTION("Resolve service with dependencies") {
        // Register dependency first
        container.registerSingleton<IDependency>([]() { return std::make_unique<Dependency>("test"); });

        // Register service that depends on the dependency
        container.registerTransient<ITestService>([&container]() {
            auto dep = container.resolve<IDependency>();
            return std::make_unique<ServiceWithDependency>(dep);
        });

        // Resolve the service
        auto service = container.resolve<ITestService>();

        REQUIRE(service != nullptr);
        REQUIRE(service->getValue() == 4);  // Length of "test"
    }
}

TEST_CASE("DIContainer - Error Handling", "[DIContainer]") {
    DIContainer container;

    SECTION("Resolve unregistered service throws DIResolutionError") {
        REQUIRE_THROWS_AS(container.resolve<ITestService>(), DIResolutionError);
    }

    SECTION("Resolution error names the missing type") {
        try {
            static_cast<void>(container.resolve<ITestService>());
            FAIL("expected DIResolutionError");
        } catch (const DIResolutionError& ex) {
            // Diagnostic must reference the unresolved type so wiring omissions
            // are debuggable at the omission site, not at a far-away segfault.
            const std::string message = ex.what();
            REQUIRE(message.find(typeid(ITestService).name()) != std::string::npos);
        }
    }

    SECTION("tryResolve returns nullptr for unregistered service") {
        auto service = container.tryResolve<ITestService>();
        REQUIRE(service == nullptr);
    }

    SECTION("tryResolve returns the instance for a registered service") {
        container.registerSingleton<ITestService>([]() { return std::make_unique<TestService>(7); });

        auto via_try = container.tryResolve<ITestService>();
        auto via_resolve = container.resolve<ITestService>();

        REQUIRE(via_try != nullptr);
        REQUIRE(via_try.get() == via_resolve.get());
        REQUIRE(via_try->getValue() == 7);
    }

    SECTION("Check if service is registered") {
        REQUIRE_FALSE(container.isRegistered<ITestService>());

        container.registerSingleton<ITestService>([]() { return std::make_unique<TestService>(1); });

        REQUIRE(container.isRegistered<ITestService>());
    }
}

TEST_CASE("DIContainer - Cyclic singleton resolution", "[DIContainer]") {
    DIContainer container;

    SECTION("Direct self-cycle is detected and throws") {
        // Factory for ITestService transitively resolves its own type before
        // returning. Without re-entry detection this re-runs the factory and
        // caches a duplicate instance; the guard must turn it into a clear throw.
        container.registerSingleton<ITestService>([&container]() {
            auto self = container.resolve<ITestService>();  // re-entry
            return std::make_unique<TestService>(self ? self->getValue() : -1);
        });

        REQUIRE_THROWS_AS(container.resolve<ITestService>(), DICycleError);
    }

    SECTION("Mutual two-type cycle is detected and throws") {
        container.registerSingleton<ITestService>([&container]() {
            auto dep = container.resolve<IDependency>();
            return std::make_unique<ServiceWithDependency>(dep);
        });
        container.registerSingleton<IDependency>([&container]() {
            auto svc = container.resolve<ITestService>();
            return std::make_unique<Dependency>(svc ? "x" : "y");
        });

        REQUIRE_THROWS_AS(container.resolve<ITestService>(), DICycleError);
    }

    SECTION("In-progress marker is cleared after a cycle throws") {
        container.registerSingleton<ITestService>([&container]() {
            auto self = container.resolve<ITestService>();
            return std::make_unique<TestService>(1);
        });
        container.registerSingleton<IDependency>([]() { return std::make_unique<Dependency>("ok"); });

        REQUIRE_THROWS_AS(container.resolve<ITestService>(), DICycleError);

        // A subsequent well-formed resolution of a different type must still
        // succeed: the failed type must not be left stuck in the in-progress set.
        auto dep = container.resolve<IDependency>();
        REQUIRE(dep != nullptr);
        REQUIRE(dep->getName() == "ok");
    }

    SECTION("Transient self-cycle is detected and throws") {
        // A transient factory that re-enters its own type would otherwise
        // recurse until the stack overflows; the guard must turn it into a
        // clear throw, just like the singleton path.
        container.registerTransient<ITestService>([&container]() {
            auto self = container.resolve<ITestService>();  // re-entry
            return std::make_unique<TestService>(self ? self->getValue() : -1);
        });

        REQUIRE_THROWS_AS(container.resolve<ITestService>(), DICycleError);
    }

    SECTION("In-progress marker is cleared after a transient cycle throws") {
        container.registerTransient<ITestService>([&container]() {
            auto self = container.resolve<ITestService>();
            return std::make_unique<TestService>(1);
        });
        container.registerSingleton<IDependency>([]() { return std::make_unique<Dependency>("ok"); });

        REQUIRE_THROWS_AS(container.resolve<ITestService>(), DICycleError);

        auto dep = container.resolve<IDependency>();
        REQUIRE(dep != nullptr);
        REQUIRE(dep->getName() == "ok");
    }
}

TEST_CASE("DIContainer - Free-function resolve helpers", "[DIContainer]") {
    auto& container = getContainer();
    container.clear();

    SECTION("Free tryResolve returns nullptr for unregistered service") {
        REQUIRE(tryResolve<ITestService>() == nullptr);
    }

    SECTION("Free tryResolve returns the instance for a registered service") {
        container.registerSingleton<ITestService>([]() { return std::make_unique<TestService>(7); });

        auto service = tryResolve<ITestService>();
        REQUIRE(service != nullptr);
        REQUIRE(service->getValue() == 7);
    }

    container.clear();
}

TEST_CASE("DIContainer - Clear Services", "[DIContainer]") {
    DIContainer container;

    SECTION("Clear all services") {
        container.registerSingleton<ITestService>([]() { return std::make_unique<TestService>(1); });

        REQUIRE(container.isRegistered<ITestService>());

        container.clear();

        REQUIRE_FALSE(container.isRegistered<ITestService>());
        REQUIRE_THROWS_AS(container.resolve<ITestService>(), DIResolutionError);
    }
}

TEST_CASE("DIContainer - Global Container Access", "[DIContainer]") {
    // Clean up any previous state
    getContainer().clear();

    SECTION("getContainer returns valid container") {
        auto& container = getContainer();

        // Register a test service
        container.registerSingleton<ITestService>([]() { return std::make_unique<TestService>(123); });

        auto service = container.resolve<ITestService>();
        REQUIRE(service != nullptr);
        REQUIRE(service->getValue() == 123);

        // Clean up
        container.clear();
    }

    SECTION("Global container is shared across calls") {
        auto& container1 = getContainer();
        auto& container2 = getContainer();

        // Should be the same instance
        REQUIRE(&container1 == &container2);

        // Register in one, should be available in the other
        container1.registerSingleton<ITestService>([]() { return std::make_unique<TestService>(456); });

        REQUIRE(container2.isRegistered<ITestService>());

        // Clean up
        container1.clear();
    }
}