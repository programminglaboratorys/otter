#pragma once
// Test-only shim. Mirrors the function set of the runtime's INI extension
// (current group/item + 1/2/3-argument overloads) so the same test cases can
// drive the Windows API and otter. This is NOT otter's public interface.
//
// A backend implements only what it supports. Anything it doesn't override
// throws Unsupported, and the runner reports that case as SKIP, not FAIL.

#include <stdexcept>
#include <string>
#include <utility>

namespace shim {

struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual const char* name() const = 0;
    virtual void open(const std::string& absolute_path) = 0;
    virtual void close() {}  // flush pending writes (if the backend defers them)

    // ---- the primitives a backend overrides --------------------------------
    virtual void setString(const std::string&, const std::string&, const std::string&) { no("setString"); }
    virtual void setValue(const std::string&, const std::string&, int)                 { no("setValue"); }
    virtual std::string getString(const std::string&, const std::string&)              { no("getString"); }
    virtual int getValue(const std::string&, const std::string&)                       { no("getValue"); }
    virtual void deleteItem(const std::string&, const std::string&)                    { no("deleteItem"); }
    virtual void deleteGroup(const std::string&)                                       { no("deleteGroup"); }

    // ---- extension-style API (same names/overloads as the runtime) ---------
    void SetCurrentGroup(std::string g) { group_ = std::move(g); }
    void SetCurrentItem(std::string i)  { item_ = std::move(i); }

    void SetString(const std::string& v)                                              { setString(group_, item_, v); }
    void SetString(const std::string& i, const std::string& v)                        { setString(group_, i, v); }
    void SetString(const std::string& g, const std::string& i, const std::string& v)  { setString(g, i, v); }

    void SetValue(int v)                                                 { setValue(group_, item_, v); }
    void SetValue(const std::string& i, int v)                           { setValue(group_, i, v); }
    void SetValue(const std::string& g, const std::string& i, int v)     { setValue(g, i, v); }

    std::string GetString()                                              { return getString(group_, item_); }
    std::string GetString(const std::string& i)                          { return getString(group_, i); }
    std::string GetString(const std::string& g, const std::string& i)    { return getString(g, i); }

    int GetValue()                                                       { return getValue(group_, item_); }
    int GetValue(const std::string& i)                                   { return getValue(group_, i); }
    int GetValue(const std::string& g, const std::string& i)             { return getValue(g, i); }

    void DeleteGroup(const std::string& g)                               { deleteGroup(g); }
    void DeleteItem(const std::string& i)                                { deleteItem(group_, i); }
    void DeleteItem(const std::string& g, const std::string& i)          { deleteItem(g, i); }

private:
    [[noreturn]] static void no(const char* what) { throw Unsupported(what); }

    std::string group_, item_;
};

}  // namespace shim
