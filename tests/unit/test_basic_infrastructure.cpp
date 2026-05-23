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
#include "core/observable.h"

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// Simple test service for verification
class ITestService {
public:
    virtual ~ITestService() = default;
    virtual std::string getName() const = 0;
};

class BasicTestService : public ITestService {
public:
    explicit BasicTestService(const std::string& name) : name_(name) {
    }
    std::string getName() const override {
        return name_;
    }

private:
    std::string name_;
};

TEST_CASE("Basic Infrastructure - DI Container Works", "[Infrastructure]") {
    DIContainer container;

    SECTION("Can register and resolve services") {
        container.registerSingleton<ITestService>([]() { return std::make_unique<BasicTestService>("test-service"); });

        auto service = container.resolve<ITestService>();
        REQUIRE(service != nullptr);
        REQUIRE(service->getName() == "test-service");
    }
}

TEST_CASE("Basic Infrastructure - Observable Pattern Works", "[Infrastructure]") {
    Observable<std::string> observable("initial");

    SECTION("Basic observable functionality") {
        REQUIRE(observable.get() == "initial");

        bool notified = false;
        std::string received_value;

        auto unsubscribe = observable.subscribe([&](const std::string& value) {
            notified = true;
            received_value = value;
        });

        observable.set("changed");

        REQUIRE(notified);
        REQUIRE(received_value == "changed");
        REQUIRE(observable.get() == "changed");

        unsubscribe();
    }
}

TEST_CASE("Basic Infrastructure - Integration Test", "[Infrastructure]") {
    SECTION("DI Container with Observable") {
        DIContainer container;

        // Register an observable as a service
        container.registerSingleton<Observable<int>>([]() { return std::make_unique<Observable<int>>(42); });

        auto observable = container.resolve<Observable<int>>();
        REQUIRE(observable != nullptr);
        REQUIRE(observable->get() == 42);

        // Test observer functionality
        int received_value = 0;
        auto unsubscribe = observable->subscribe([&](const int& value) { received_value = value; });

        observable->set(100);
        REQUIRE(received_value == 100);

        unsubscribe();
    }
}