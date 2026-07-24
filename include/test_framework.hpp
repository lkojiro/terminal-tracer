#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>

// A minimal, zero-dependency unit test harness. No external libraries
// to install — just include this header and write tests.
//
// Usage:
//   TEST(vec3_dot_product) {
//       Vec3 a(1, 0, 0), b(0, 1, 0);
//       CHECK_NEAR(a.dot(b), 0.0f, 1e-6f);
//   }
//
// Then in main(): return testing::runAllTests();

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, fn});
    }
};

// Thrown by CHECK_* macros on failure, caught by the runner so one
// failing assertion doesn't crash the whole test binary.
struct AssertionFailure {
    std::string message;
};

inline int runAllTests() {
    int passed = 0, failed = 0;
    for (auto& t : registry()) {
        try {
            t.fn();
            std::cout << "[PASS] " << t.name << "\n";
            passed++;
        } catch (const AssertionFailure& e) {
            std::cout << "[FAIL] " << t.name << " -- " << e.message << "\n";
            failed++;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << " -- uncaught exception: " << e.what() << "\n";
            failed++;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace testing

#define TEST(name)                                                          \
    void test_##name();                                                     \
    static ::testing::Registrar registrar_##name(#name, test_##name);       \
    void test_##name()

#define CHECK(expr)                                                         \
    do {                                                                    \
        if (!(expr)) {                                                     \
            throw ::testing::AssertionFailure{                             \
                std::string("CHECK failed: ") + #expr +                    \
                " (" + __FILE__ + ":" + std::to_string(__LINE__) + ")" };   \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                \
    do {                                                                    \
        float _a = (a), _b = (b);                                           \
        if (std::fabs(_a - _b) > (eps)) {                                   \
            throw ::testing::AssertionFailure{                              \
                std::string("CHECK_NEAR failed: ") + #a + " ~= " + #b +      \
                " (got " + std::to_string(_a) + " vs " + std::to_string(_b) +\
                ", " + __FILE__ + ":" + std::to_string(__LINE__) + ")" };    \
        }                                                                    \
    } while (0)
