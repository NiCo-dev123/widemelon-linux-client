#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace widemelon::display
{
    enum class PageId : std::uint8_t
    {
        Home,
        Settings,
        About,
        LeftStickMod,
        RightStickMod,
        LeftStickCalibration,
        Numpad
    };

    enum class FooterIcon : std::uint8_t
    {
        A,
        B,
        X,
        Start,
        L,
        R
    };

    enum class FooterItemType : std::uint8_t
    {
        Icon,
        Text
    };

    struct FooterItem
    {
        FooterItemType type{FooterItemType::Text};
        FooterIcon icon{FooterIcon::A};
        std::string_view text;
    };

    enum class FieldType : std::uint8_t
    {
        TextInput,
        NavigationButton,
        ActionButton,
        Choice,
        Range
    };

    enum class ValueId : std::uint8_t
    {
        None,
        Host,
        Port,
        PairingCode,
        ActiveTheme,
        LeftStickMode,
        RightStickMode,
        Sm64AutoDelayFrames,
        Sm64AutoReleaseDelayMs,
        Sm64DpadDeadzonePercent,
        CursorSpeedLimit,
        LeftStickScaleUpPercent,
        LeftStickScaleDownPercent,
        LeftStickScaleLeftPercent,
        LeftStickScaleRightPercent
    };

    enum class ActionId : std::uint8_t
    {
        None,
        Connect,
        Quit
    };

    struct FieldDefinition
    {
        FieldType type{FieldType::ActionButton};
        std::string_view label;
        ValueId value{ValueId::None};
        PageId destination{PageId::Home};
        ActionId action{ActionId::None};
        int minimum{0};
        int maximum{0};
        int step{1};
    };

    struct SectionDefinition
    {
        std::string_view title;
        std::vector<std::string_view> hintLines;
        std::vector<FieldDefinition> fields;
    };

    struct PageDefinition
    {
        PageId id{PageId::Home};
        std::string_view title;
        std::vector<FooterItem> footer;
        std::vector<SectionDefinition> sections;
    };
}
