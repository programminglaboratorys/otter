#pragma once
// Test case type + runner. Each case gets a fresh file, runs against one
// backend, and optionally checks the exact bytes left on disk.

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

#include "shim.hpp"

namespace harness {

namespace fs = std::filesystem;

struct Failure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Case {
    const char* name;                       // unique, filename-safe
    std::optional<std::string> initial;     // file bytes before; nullopt = no file
    std::function<void(shim::Backend&)> run;
    std::optional<std::string> expected;    // file bytes after; nullopt = don't check
    const char* skip = nullptr;             // reason to skip (case not ready)
};

enum class Result { Pass, Fail, Skip };

inline std::string show(const std::string& s) {
    std::ostringstream o;
    for (unsigned char c : s) {
        if (c == '\r') o << "\\r";
        else if (c == '\n') o << "\\n\n";
        else if (c >= 32 && c < 127) o << c;
        else { char b[8]; std::snprintf(b, sizeof b, "\\x%02X", c); o << b; }
    }
    return o.str();
}

template <class A, class B>
void expect_eq(const A& got, const B& want, const char* what) {
    if (!(got == want)) {
        std::ostringstream o;
        o << what << ": got [" << got << "], want [" << want << "]";
        throw Failure(o.str());
    }
}

inline std::string read_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

inline void write_bytes(const fs::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

inline Result run_case(shim::Backend& b, const Case& c, std::string& msg) {
    if (c.skip) { msg = c.skip; return Result::Skip; }

    fs::path dir = fs::temp_directory_path() / "otter_tests" / b.name();
    fs::create_directories(dir);
    fs::path file = dir / (std::string(c.name) + ".ini");
    fs::remove(file);
    if (c.initial) write_bytes(file, *c.initial);

    try {
        b.open(file.string());
        c.run(b);
        b.close();
        if (c.expected) {
            std::string got = read_bytes(file);
            if (got != *c.expected) {
                msg = "file bytes differ\n--- expected:\n" + show(*c.expected) +
                      "\n--- actual:\n" + show(got);
                return Result::Fail;
            }
        }
    } catch (const shim::Unsupported& e) {
        msg = std::string("not implemented: ") + e.what();
        return Result::Skip;
    } catch (const std::exception& e) {
        msg = e.what();
        return Result::Fail;
    }
    return Result::Pass;
}

}  // namespace harness
