#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace widemelon::display
{

    class UiConfig
    {
    public:
        explicit UiConfig(std::filesystem::path path);

        std::optional<std::string> readValue(std::string_view key) const;
        int readInt(std::string_view key, int defaultValue, int minimum, int maximum) const;
        bool writeValue(std::string_view key, std::string_view value) const;
        bool writeInt(std::string_view key, int value, int minimum, int maximum) const;

    private:
        std::filesystem::path filePath;
    };

}
