#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mmo::core {

struct KeyValueEntry {
    std::string key;
    std::string value;
    int line = 0;
};

// Versioned text format shared by content files:
//   <FORMAT> <version>
//   key=value          (blank lines and lines starting with '#' are ignored)
// Keys may repeat; consumers decide which keys they accept.
struct KeyValueDocument {
    std::string source;
    std::string format;
    int version = 0;
    std::vector<KeyValueEntry> entries;

    // "source:line: message", used by every content error.
    [[nodiscard]] std::string where(int line) const;
};

// Throws std::runtime_error with the source name and line on malformed input.
[[nodiscard]] KeyValueDocument parse_key_value(std::string_view text, std::string source);
[[nodiscard]] KeyValueDocument read_key_value_file(const std::filesystem::path& path);

// Strict number parsing; throws std::runtime_error naming `document.where(entry.line)`.
[[nodiscard]] float parse_float(const KeyValueDocument& document, const KeyValueEntry& entry);
[[nodiscard]] unsigned long long parse_unsigned(
    const KeyValueDocument& document, const KeyValueEntry& entry);
[[nodiscard]] std::vector<std::string> split_words(std::string_view value);

}
