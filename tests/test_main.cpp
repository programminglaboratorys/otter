// Runs every case against every available backend.
// Usage: ini_tests [name-filter]   (substring match on the case name)

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "cases.hpp"
#include "cases_probe.hpp"
#include "win_backend.hpp"

int main(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";

    std::vector<std::unique_ptr<shim::Backend>> backends;
#ifdef _WIN32
    backends.push_back(std::make_unique<shim::WinBackend>());
#endif
    // otter backend goes here once it exists.

    if (backends.empty()) {
        std::puts("no backends available on this platform");
        return 0;
    }

    auto list = cases::all();                 // hand-written cases
    auto probed = cases::probe();
    list.insert(list.end(), std::make_move_iterator(probed.begin()), std::make_move_iterator(probed.end()));

    int pass = 0, fail = 0, skip = 0;

    for (auto& b : backends) {
        for (const auto& c : list) {
            if (!filter.empty() && std::string(c.name).find(filter) == std::string::npos)
                continue;

            std::string msg;
            switch (harness::run_case(*b, c, msg)) {
                case harness::Result::Pass:
                    ++pass;
                    std::printf("[PASS] %-12s %s\n", b->name(), c.name);
                    break;
                case harness::Result::Skip:
                    ++skip;
                    std::printf("[SKIP] %-12s %s  (%s)\n", b->name(), c.name, msg.c_str());
                    break;
                case harness::Result::Fail:
                    ++fail;
                    std::printf("[FAIL] %-12s %s\n%s\n", b->name(), c.name, msg.c_str());
                    break;
            }
        }
    }

    std::printf("\n%d passed, %d failed, %d skipped\n", pass, fail, skip);
    return fail ? 1 : 0;
}
