#include "core/key_value_file.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mmo::core {
namespace {

std::string_view trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

}

std::string KeyValueDocument::where(int line) const
{
    return source + ":" + std::to_string(line) + ": ";
}

KeyValueDocument parse_key_value(std::string_view text, std::string source)
{
    KeyValueDocument document;
    document.source = std::move(source);
    bool header_read = false;
    int line_number = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string_view line = trim(text.substr(start, end - start));
        start = end + 1;
        ++line_number;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (!header_read) {
            const auto space = line.find(' ');
            if (space == std::string_view::npos) {
                throw std::runtime_error(document.where(line_number) +
                    "expected '<FORMAT> <version>' header");
            }
            document.format = std::string(line.substr(0, space));
            const std::string_view version = trim(line.substr(space + 1));
            const auto result = std::from_chars(
                version.data(), version.data() + version.size(), document.version);
            if (result.ec != std::errc{} || result.ptr != version.data() + version.size() ||
                document.version <= 0) {
                throw std::runtime_error(document.where(line_number) + "invalid format version");
            }
            header_read = true;
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) {
            throw std::runtime_error(document.where(line_number) + "expected key=value");
        }
        const std::string_view key = trim(line.substr(0, equals));
        if (key.empty()) {
            throw std::runtime_error(document.where(line_number) + "empty key");
        }
        document.entries.push_back(
            {std::string(key), std::string(trim(line.substr(equals + 1))), line_number});
    }
    if (!header_read) {
        throw std::runtime_error(document.source + ": file is empty");
    }
    return document;
}

KeyValueDocument read_key_value_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open " + path.generic_string());
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return parse_key_value(contents.str(), path.generic_string());
}

float parse_float(const KeyValueDocument& document, const KeyValueEntry& entry)
{
    // std::from_chars for float is not available on every toolchain we target.
    std::istringstream stream(entry.value);
    stream.imbue(std::locale::classic());
    float value = 0.0f;
    if (!(stream >> value) || !(stream >> std::ws).eof() || !std::isfinite(value)) {
        throw std::runtime_error(document.where(entry.line) + entry.key + " must be a number");
    }
    return value;
}

unsigned long long parse_unsigned(const KeyValueDocument& document, const KeyValueEntry& entry)
{
    unsigned long long value = 0;
    const char* const end = entry.value.data() + entry.value.size();
    const auto result = std::from_chars(entry.value.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error(
            document.where(entry.line) + entry.key + " must be a non-negative integer");
    }
    return value;
}

std::vector<std::string> split_words(std::string_view value)
{
    std::vector<std::string> words;
    std::size_t start = 0;
    while (start < value.size()) {
        start = value.find_first_not_of(" \t", start);
        if (start == std::string_view::npos) {
            break;
        }
        const std::size_t end = std::min(value.find_first_of(" \t", start), value.size());
        words.emplace_back(value.substr(start, end - start));
        start = end;
    }
    return words;
}

}
