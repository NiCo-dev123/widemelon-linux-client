#include "display/UiConfig.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace widemelon::display
{

    UiConfig::UiConfig(std::filesystem::path path)
        : filePath(std::move(path))
    {
    }

    std::optional<std::string> UiConfig::readValue(std::string_view key) const
    {
        std::ifstream input(filePath);
        std::string line;
        const std::string prefix = std::string(key) + "=";
        while (std::getline(input, line))
        {
            if (line.compare(0, prefix.size(), prefix) == 0)
                return line.substr(prefix.size());
        }
        return std::nullopt;
    }

    int UiConfig::readInt(std::string_view key, int defaultValue, int minimum, int maximum) const
    {
        const std::optional<std::string> value = readValue(key);
        if (!value) return defaultValue;

        int parsed = 0;
        const auto result = std::from_chars(value->data(), value->data() + value->size(), parsed);
        if (result.ec != std::errc{} || result.ptr != value->data() + value->size()) return defaultValue;
        return std::clamp(parsed, minimum, maximum);
    }

    bool UiConfig::writeValue(std::string_view key, std::string_view value) const
    {
        std::ifstream input(filePath);
        if (!input) return false;

        const std::string prefix = std::string(key) + "=";
        const std::string replacement = prefix + std::string(value);
        std::vector<std::string> lines;
        std::string line;
        bool replaced = false;
        while (std::getline(input, line))
        {
            if (line.compare(0, prefix.size(), prefix) == 0)
            {
                lines.push_back(replacement);
                replaced = true;
            }
            else
                lines.push_back(line);
        }
        if (!replaced) lines.push_back(replacement);

        const std::filesystem::path temporary = filePath.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output) return false;
            for (const std::string &entry : lines) output << entry << "\n";
            if (!output) return false;
        }

        std::error_code error;
        std::filesystem::rename(temporary, filePath, error);
        if (!error) return true;
        std::filesystem::remove(temporary, error);
        return false;
    }

    bool UiConfig::writeInt(std::string_view key, int value, int minimum, int maximum) const
    {
        return writeValue(key, std::to_string(std::clamp(value, minimum, maximum)));
    }

}
