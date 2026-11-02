// minimal example of using otter
#include <iostream>


#include <iostream>
#include "otter/otter.hpp"

const std::string test_content = R"*([]
l = 1
[Hello]
World = Amazingo
; this is a random comment


[Audio]
volume = 80
mute
;[
[annoyingSection] ; bleh, this should get ignored

)*";

int main()
{
    using namespace otter;
    std::cout << "Hello World!\n";

    std::cout << test_content;

    const char* character = test_content.c_str();
    INI ini{};
    ini.parse_lines(test_content);

    std::cout << "--------------" << std::endl;
    std::cout << "ini.get(\"Audio\", \"volume\"): ";
    std::cout << ini.get("Audio", "volume") << std::endl;
    std::cout << "--------------" << std::endl;
    // print the lines
    for (const auto& line : ini.lines) {
        std::cout << "Line at offset " << std::setw(2) << std::setfill('0') << line.offset << ": ";
        switch (line.kind) {
        case LineKind::Section:
            std::cout << "Section [" << test_content.substr(line.name_off, line.name_len) << "]\n";
            break;
        case LineKind::KeyValue:
            std::cout << "KeyValue " << test_content.substr(line.name_off, line.name_len) << " = "
                << test_content.substr(line.value_off, line.value_len) << "\n";
            break;
        case LineKind::Bare:
            std::cout << "Bare " << test_content.substr(line.name_off, line.name_len) << "\n";
            break;
        case LineKind::Comment:
            std::cout << "Comment " << test_content.substr(line.name_off, line.name_len) << "\n";
            break;
        default:
            std::cout << "Invalid\n";
            break;
        }
    }
    std::cout << "--------------" << std::endl;
    // print the ini map
    for (const auto& section : ini.ini) {
        std::cout << "[" << section.first << "]\n";
        for (const auto& kv : section.second) {
            std::cout << "  " << kv.first << " = " << kv.second << "\n";
        }
    }
    return 0;
}