#pragma once
// Reference backend: the real Windows profile API. Windows only.
// Always pass an absolute path: a bare file name makes Windows look in the
// Windows directory instead of the current one.

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "shim.hpp"

namespace shim {

class WinBackend : public Backend {
public:
    const char* name() const override { return "windows-api"; }

    void open(const std::string& p) override { path_ = p; }

    void setString(const std::string& g, const std::string& i, const std::string& v) override {
        if (!WritePrivateProfileStringA(g.c_str(), i.c_str(), v.c_str(), path_.c_str()))
            throw std::runtime_error("WritePrivateProfileStringA failed");
    }

    // TODO: confirm how the original game formats numbers (int only for now).
    void setValue(const std::string& g, const std::string& i, int v) override {
        setString(g, i, std::to_string(v));
    }

    std::string getString(const std::string& g, const std::string& i,
                          const std::string& def) override {
        std::string buf(65536, '\0');
        DWORD n = GetPrivateProfileStringA(g.c_str(), i.c_str(), def.c_str(), buf.data(),
                                           static_cast<DWORD>(buf.size()), path_.c_str());
        buf.resize(n);
        return buf;
    }

    int getValue(const std::string& g, const std::string& i) override {
        return static_cast<int>(GetPrivateProfileIntA(g.c_str(), i.c_str(), 0, path_.c_str()));
    }

    void deleteItem(const std::string& g, const std::string& i) override {
        if (!WritePrivateProfileStringA(g.c_str(), i.c_str(), nullptr, path_.c_str()))
            throw std::runtime_error("delete item failed");
    }

    void deleteGroup(const std::string& g) override {
        if (!WritePrivateProfileStringA(g.c_str(), nullptr, nullptr, path_.c_str()))
            throw std::runtime_error("delete group failed");
    }

private:
    std::string path_;
};

}  // namespace shim

#endif  // _WIN32
