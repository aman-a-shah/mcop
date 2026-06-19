// test_framework.hpp — a minimal, dependency-free unit-test harness.
//
// Usage:
//   #include "test_framework.hpp"
//   TEST(my_case) { CHECK(1 + 1 == 2); CHECK_CLOSE(x, 3.0, 1e-9); }
//   int main() { return mcop::test::run_all(); }
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace mcop {
namespace test {

struct Case {
    std::string name;
    std::function<void(int&)> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void(int&)> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int run_all() {
    int failures = 0;
    for (auto& c : registry()) {
        int local = 0;
        c.fn(local);
        if (local == 0) {
            std::printf("[ PASS ] %s\n", c.name.c_str());
        } else {
            std::printf("[ FAIL ] %s (%d assertion(s))\n", c.name.c_str(), local);
            failures += local;
        }
    }
    std::printf("----\n%s (%zu cases)\n", failures == 0 ? "ALL PASSED" : "FAILURES",
                registry().size());
    return failures == 0 ? 0 : 1;
}

}  // namespace test
}  // namespace mcop

// --- assertion macros (require an int& named `_fail` in scope) ---
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("    CHECK failed: %s (%s:%d)\n", #cond,          \
                        __FILE__, __LINE__);                             \
            ++_fail;                                                      \
        }                                                                 \
    } while (0)

#define CHECK_CLOSE(a, b, tol)                                            \
    do {                                                                  \
        double _va = (a), _vb = (b), _t = (tol);                          \
        if (std::fabs(_va - _vb) > _t) {                                  \
            std::printf("    CHECK_CLOSE failed: |%.10g - %.10g| > %.3g " \
                        "(%s:%d)\n", _va, _vb, _t, __FILE__, __LINE__);   \
            ++_fail;                                                      \
        }                                                                 \
    } while (0)

#define TEST(name)                                                        \
    static void name(int& _fail);                                         \
    static ::mcop::test::Registrar _reg_##name(#name, name);             \
    static void name(int& _fail)
