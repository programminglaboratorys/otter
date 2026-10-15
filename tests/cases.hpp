#pragma once
// Dummy cases. Expected bytes are written from our notes on Windows behavior;
// running them against the windows-api backend confirms (or corrects) them.
// Real cases will later be generated from the probe results.

#include <vector>

#include "harness.hpp"

namespace cases {

using harness::Case;
using harness::expect_eq;
using shim::Backend;

inline std::vector<Case> all() {
    return {
        {"set_new_key_in_existing_section",
         "[a]\r\nx=1\r\n",
         [](Backend& b) { b.SetString("a", "y", "2"); },
         "[a]\r\nx=1\r\ny=2\r\n"},

        {"set_overwrites_existing_key",
         "[a]\r\nx=1\r\n",
         [](Backend& b) { b.SetString("a", "x", "2"); },
         "[a]\r\nx=2\r\n"},

        {"set_creates_missing_file",
         std::nullopt,
         [](Backend& b) { b.SetString("a", "k", "v"); },
         "[a]\r\nk=v\r\n"},

        {"set_with_current_group_and_item",
         std::nullopt,
         [](Backend& b) {
             b.SetCurrentGroup("a");
             b.SetCurrentItem("k");
             b.SetString("v");
         },
         "[a]\r\nk=v\r\n"},

        {"get_returns_what_was_set",
         std::nullopt,
         [](Backend& b) {
             b.SetString("a", "k", "hello");
             expect_eq(b.GetString("a", "k"), std::string("hello"), "GetString");
         },
         std::nullopt},

        {"value_roundtrip",
         std::nullopt,
         [](Backend& b) {
             b.SetValue("a", "n", 42);
             expect_eq(b.GetValue("a", "n"), 42, "GetValue");
         },
         "[a]\r\nn=42\r\n"},

        {"delete_key",
         "[a]\r\nx=1\r\ny=2\r\n",
         [](Backend& b) { b.DeleteItem("a", "x"); },
         "[a]\r\ny=2\r\n"},

        {"delete_group",
         "[a]\r\nx=1\r\n[b]\r\ny=2\r\n",
         [](Backend& b) { b.DeleteGroup("a"); },
         "[b]\r\ny=2\r\n"},

        // Shows how a case that isn't ready is switched off.
        {"quirk_not_ready_example",
         "[a]\r\nx=1\r\n",
         [](Backend& b) { b.SetString("a", "y", "2"); },
         "[a]\r\nx=1\r\ny=2\r\n",
         "waiting for probe data"},
    };
}

}  // namespace cases
