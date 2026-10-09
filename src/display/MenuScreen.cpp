#include "display/MenuScreen.h"
#include "display/ConfigScreen.h"
#include "display/MenuNavigation.h"
#include "display/NumpadScreen.h"
#include "display/PageRenderer.h"
#include "display/UiConfig.h"
#include "display/UiTheme.h"
#include "display/pages/HomePage.h"
#include "display/pages/LeftStickCalibrationPage.h"
#include "display/pages/LeftStickModPage.h"
#include "display/pages/SettingsPage.h"
#include "common/Logger.h"
#include "input/CursorStickMod.h"
#include "input/LeftStickCalibration.h"
#include "input/Sm64DpadMod.h"
#include "input/Sm64StickMod.h"
#include "input/EvdevInput.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace widemelon::display
{
namespace
{
constexpr std::string_view UiConfigThemeKey{"active-theme"};
constexpr std::string_view UiConfigLeftStickModeKey{"left-stick-mod"};
constexpr std::string_view UiConfigSm64AutoFramesKey{"sm64-auto-center-hold-frames"};
constexpr std::string_view UiConfigSm64AutoReleaseDelayKey{"sm64-auto-release-delay-ms"};
constexpr std::string_view UiConfigCursorSpeedKey{"cursor-speed-limit"};
constexpr std::string_view UiConfigSm64DpadDeadzoneKey{"sm64-dpad-deadzone-percent"};
constexpr std::string_view UiConfigLeftStickScaleLeftKey{"left-stick-scale-left-percent"};
constexpr std::string_view UiConfigLeftStickScaleRightKey{"left-stick-scale-right-percent"};
constexpr std::string_view UiConfigLeftStickScaleUpKey{"left-stick-scale-up-percent"};
constexpr std::string_view UiConfigLeftStickScaleDownKey{"left-stick-scale-down-percent"};

UiConfig uiConfigFor(const std::string &directory) { return UiConfig(std::filesystem::path(directory) / "widemelon-client-ui.conf"); }
auto &palette = uiPalette();
auto &themeText = uiTextSettings();
auto &uiTextures = widemelon::display::uiTextures();
using widemelon::display::drawControlHint;
using widemelon::display::drawGradientBackground;
using widemelon::display::drawPill;
using widemelon::display::drawTextColored;
using widemelon::display::textWidth;

enum class MenuPage { Home, Settings, LeftStickMod, LeftStickCalibration };
enum class MenuItem {
    HomeHostLabel, HomeHost, HomePortLabel, HomePort, HomeSessionCodeLabel, HomeSessionCode,
    SectionConnect, HomeConnect, SectionSettings, HomeSettings,
    SectionTheme, Theme, SectionAnalogSticks, LeftStickMod, LeftStickCalibration,
    LeftStickModeHint, LeftStickMode, LeftStickDescription,
    Sm64AutoFramesLabel, Sm64AutoFrames, Sm64AutoReleaseDelayLabel, Sm64AutoReleaseDelay,
    Sm64DpadDeadzoneLabel, Sm64DpadDeadzone, CursorSpeedLabel, CursorSpeed,
    LeftStickCalibrationHint, LeftStickScaleUpLabel, LeftStickScaleUp,
    LeftStickScaleDownLabel, LeftStickScaleDown, LeftStickScaleLeftLabel, LeftStickScaleLeft,
    LeftStickScaleRightLabel, LeftStickScaleRight,
    SectionNavigation, Back, Quit
};

    enum class SettingsResult { Home, Exit };

    std::vector<std::string> availableThemes(const std::string &directory)
    {
        std::vector<std::string> themes;
        std::error_code error;
        for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path(directory) / "assets" / "themes", error))
            if (entry.is_directory(error)) themes.push_back(entry.path().filename().string());
        std::sort(themes.begin(), themes.end());
        return themes;
    }


    int menuItemHeight(MenuItem item, std::string_view leftStickMode = {})
    {
        switch (item)
        {
        case MenuItem::SectionConnect:
        case MenuItem::SectionSettings:
        case MenuItem::SectionTheme:
        case MenuItem::SectionAnalogSticks:
        case MenuItem::SectionNavigation:
        case MenuItem::HomeHostLabel:
        case MenuItem::HomePortLabel:
        case MenuItem::HomeSessionCodeLabel:
        case MenuItem::Sm64AutoFramesLabel:
        case MenuItem::Sm64AutoReleaseDelayLabel:
        case MenuItem::Sm64DpadDeadzoneLabel:
        case MenuItem::CursorSpeedLabel:
        case MenuItem::LeftStickScaleUpLabel:
        case MenuItem::LeftStickScaleDownLabel:
        case MenuItem::LeftStickScaleLeftLabel:
        case MenuItem::LeftStickScaleRightLabel:
            return themeText.formFontSize;
        case MenuItem::LeftStickModeHint:
            return themeText.gameplayStatusFontSize * 2 + 8;
        case MenuItem::LeftStickCalibrationHint:
            return themeText.gameplayStatusFontSize * 3 + 16;
        case MenuItem::LeftStickDescription:
        {
            const std::string_view description = widemelon::display::pages::leftStickModeDescription(leftStickMode);
            const int lineCount = 1 + static_cast<int>(std::count(description.begin(), description.end(), '\n'));
            return lineCount * themeText.gameplayStatusFontSize + std::max(0, lineCount - 1) * 8;
        }
        default:
            return widemelon::UiFormFieldHeight;
        }
    }

    int menuItemTop(const std::vector<MenuItem> &items, std::size_t index, std::string_view leftStickMode = {})
    {
        int top = 0;
        for (std::size_t itemIndex = 0; itemIndex < index; ++itemIndex)
            top += menuItemHeight(items[itemIndex], leftStickMode) + widemelon::UiMenuItemGap;
        return top;
    }

    int menuListTop()
    {
        widemelon::display::PageRenderContext context;
        context.itemGap = widemelon::UiMenuItemGap;
        context.titleFontSize = themeText.titleFontSize;
        return widemelon::display::PageRenderer::firstItemY(context);
    }

    int menuFooterTop(int height)
    {
        widemelon::display::PageRenderContext context;
        context.height = height;
        return widemelon::display::PageRenderer::footerTop(context);
    }

    int menuListBottom(int height)
    {
        return menuFooterTop(height) - widemelon::UiMenuItemGap;
    }

    bool menuItemSelectable(MenuItem item)
    {
        switch (item)
        {
        case MenuItem::HomeHostLabel:
        case MenuItem::HomePortLabel:
        case MenuItem::HomeSessionCodeLabel:
        case MenuItem::SectionConnect:
        case MenuItem::SectionSettings:
        case MenuItem::SectionTheme:
        case MenuItem::SectionAnalogSticks:
        case MenuItem::LeftStickModeHint:
        case MenuItem::LeftStickDescription:
        case MenuItem::LeftStickCalibrationHint:
        case MenuItem::Sm64AutoFramesLabel:
        case MenuItem::Sm64AutoReleaseDelayLabel:
        case MenuItem::Sm64DpadDeadzoneLabel:
        case MenuItem::CursorSpeedLabel:
        case MenuItem::LeftStickScaleUpLabel:
        case MenuItem::LeftStickScaleDownLabel:
        case MenuItem::LeftStickScaleLeftLabel:
        case MenuItem::LeftStickScaleRightLabel:
        case MenuItem::SectionNavigation:
            return false;
        default:
            return true;
        }
    }

    std::vector<widemelon::display::MenuNavigationItem> menuNavigationItems(const std::vector<MenuItem> &items,
                                                                              std::string_view leftStickMode = {})
    {
        std::vector<widemelon::display::MenuNavigationItem> navigationItems;
        navigationItems.reserve(items.size());
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            navigationItems.push_back({menuItemSelectable(items[index]), menuItemTop(items, index, leftStickMode),
                                       menuItemHeight(items[index], leftStickMode)});
        }
        return navigationItems;
    }

    int menuScrollOffset(const std::vector<MenuItem> &items, std::string_view leftStickMode, int selected,
                         int currentOffset, int height)
    {
        const int viewportHeight = menuListBottom(height) - menuListTop();
        return widemelon::display::MenuNavigation::scrollOffsetForSelection(
            menuNavigationItems(items, leftStickMode), selected, currentOffset, viewportHeight);
    }

    std::vector<MenuItem> menuItems(MenuPage page, const std::string &leftStickMode)
    {
        if (page == MenuPage::Home)
        {
            std::vector<MenuItem> items;
            for (const widemelon::display::SectionDefinition &section : widemelon::display::pages::homePage().sections)
            {
                for (const widemelon::display::FieldDefinition &field : section.fields)
                {
                    switch (field.value)
                    {
                    case widemelon::display::ValueId::Host:
                        items.push_back(MenuItem::HomeHostLabel);
                        items.push_back(MenuItem::HomeHost);
                        break;
                    case widemelon::display::ValueId::Port:
                        items.push_back(MenuItem::HomePortLabel);
                        items.push_back(MenuItem::HomePort);
                        break;
                    case widemelon::display::ValueId::PairingCode:
                        items.push_back(MenuItem::HomeSessionCodeLabel);
                        items.push_back(MenuItem::HomeSessionCode);
                        break;
                    default:
                        if (field.action == widemelon::display::ActionId::Connect)
                        {
                            items.push_back(MenuItem::SectionConnect);
                            items.push_back(MenuItem::HomeConnect);
                        }
                        else if (field.destination == widemelon::display::PageId::Settings)
                        {
                            items.push_back(MenuItem::SectionSettings);
                            items.push_back(MenuItem::HomeSettings);
                        }
                        break;
                    }
                }
            }
            return items;
        }
        if (page == MenuPage::Settings)
        {
            std::vector<MenuItem> items;
            for (const widemelon::display::SectionDefinition &section : widemelon::display::pages::settingsPage().sections)
            {
                if (section.title == "Theme") items.push_back(MenuItem::SectionTheme);
                else if (section.title == "Analog sticks") items.push_back(MenuItem::SectionAnalogSticks);
                else if (section.title == "Navigation") items.push_back(MenuItem::SectionNavigation);

                for (const widemelon::display::FieldDefinition &field : section.fields)
                {
                    if (field.value == widemelon::display::ValueId::ActiveTheme)
                        items.push_back(MenuItem::Theme);
                    else if (field.destination == widemelon::display::PageId::LeftStickMod)
                        items.push_back(MenuItem::LeftStickMod);
                    else if (field.destination == widemelon::display::PageId::LeftStickCalibration)
                        items.push_back(MenuItem::LeftStickCalibration);
                    else if (field.action == widemelon::display::ActionId::Quit)
                        items.push_back(MenuItem::Quit);
                    else if (field.destination == widemelon::display::PageId::Home)
                        items.push_back(MenuItem::Back);
                }
            }
            return items;
        }
        if (page == MenuPage::LeftStickCalibration)
        {
            std::vector<MenuItem> items;
            for (const widemelon::display::SectionDefinition &section : widemelon::display::pages::leftStickCalibrationPage().sections)
            {
                if (section.title == "Navigation") items.push_back(MenuItem::SectionNavigation);
                if (!section.hintLines.empty()) items.push_back(MenuItem::LeftStickCalibrationHint);
                for (const widemelon::display::FieldDefinition &field : section.fields)
                {
                    switch (field.value)
                    {
                    case widemelon::display::ValueId::LeftStickScaleUpPercent:
                        items.push_back(MenuItem::LeftStickScaleUpLabel);
                        items.push_back(MenuItem::LeftStickScaleUp);
                        break;
                    case widemelon::display::ValueId::LeftStickScaleDownPercent:
                        items.push_back(MenuItem::LeftStickScaleDownLabel);
                        items.push_back(MenuItem::LeftStickScaleDown);
                        break;
                    case widemelon::display::ValueId::LeftStickScaleLeftPercent:
                        items.push_back(MenuItem::LeftStickScaleLeftLabel);
                        items.push_back(MenuItem::LeftStickScaleLeft);
                        break;
                    case widemelon::display::ValueId::LeftStickScaleRightPercent:
                        items.push_back(MenuItem::LeftStickScaleRightLabel);
                        items.push_back(MenuItem::LeftStickScaleRight);
                        break;
                    default:
                        if (field.destination == widemelon::display::PageId::Settings) items.push_back(MenuItem::Back);
                        break;
                    }
                }
            }
            return items;
        }

        const widemelon::display::PageDefinition definition = widemelon::display::pages::leftStickModPage(leftStickMode);
        std::vector<MenuItem> items;
        for (const widemelon::display::SectionDefinition &section : definition.sections)
        {
            if (section.title == "Navigation") items.push_back(MenuItem::SectionNavigation);
            if (!section.hintLines.empty())
            {
                items.push_back(section.title == "Mode hint" ? MenuItem::LeftStickModeHint : MenuItem::LeftStickDescription);
            }
            for (const widemelon::display::FieldDefinition &field : section.fields)
            {
                switch (field.value)
                {
                case widemelon::display::ValueId::LeftStickMode: items.push_back(MenuItem::LeftStickMode); break;
                case widemelon::display::ValueId::Sm64AutoDelayFrames:
                    items.push_back(MenuItem::Sm64AutoFramesLabel);
                    items.push_back(MenuItem::Sm64AutoFrames);
                    break;
                case widemelon::display::ValueId::Sm64AutoReleaseDelayMs:
                    items.push_back(MenuItem::Sm64AutoReleaseDelayLabel);
                    items.push_back(MenuItem::Sm64AutoReleaseDelay);
                    break;
                case widemelon::display::ValueId::Sm64DpadDeadzonePercent:
                    items.push_back(MenuItem::Sm64DpadDeadzoneLabel);
                    items.push_back(MenuItem::Sm64DpadDeadzone);
                    break;
                case widemelon::display::ValueId::CursorSpeedLimit:
                    items.push_back(MenuItem::CursorSpeedLabel);
                    items.push_back(MenuItem::CursorSpeed);
                    break;
                default:
                    if (field.destination == widemelon::display::PageId::Settings) items.push_back(MenuItem::Back);
                    break;
                }
            }
        }
        return items;
    }

    const char *menuTitle(MenuPage page)
    {
        switch (page)
        {
        case MenuPage::Home: return widemelon::display::pages::homePage().title.data();
        case MenuPage::Settings: return widemelon::display::pages::settingsPage().title.data();
        case MenuPage::LeftStickMod: return widemelon::display::pages::leftStickModPage({}).title.data();
        case MenuPage::LeftStickCalibration: return widemelon::display::pages::leftStickCalibrationPage().title.data();
        }
        return "Settings";
    }

    void drawAdjustableValue(SDL_Renderer *renderer, const SDL_Rect &field, const std::string &value, bool selected)
    {
        drawPill(renderer, field, selected);
        const int textY = field.y + (field.h - themeText.formFontSize) / 2;
        drawTextColored(renderer, "<", field.x + 12, textY, themeText.formFontSize, palette.primary);
        drawTextColored(renderer, value, field.x + (field.w - textWidth(value, themeText.formFontSize)) / 2,
                        textY, themeText.formFontSize, palette.primary);
        drawTextColored(renderer, ">", field.x + field.w - textWidth(">", themeText.formFontSize) - 12,
                        textY, themeText.formFontSize, palette.primary);
    }

    void drawMenuText(SDL_Renderer *renderer, int centerX, int rowY, std::string_view text, SDL_Color color, int fontSize)
    {
        drawTextColored(renderer, text, centerX - textWidth(text, fontSize) / 2, rowY, fontSize, color);
    }

    void drawMenuMultilineText(SDL_Renderer *renderer, int centerX, int rowY, std::string_view text,
                               SDL_Color color, int fontSize)
    {
        constexpr int lineGap = 8;
        std::size_t start = 0;
        int line = 0;
        while (start <= text.size())
        {
            const std::size_t end = text.find('\n', start);
            const std::string_view current = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
            if (!current.empty()) drawMenuText(renderer, centerX, rowY + line * (fontSize + lineGap), current, color, fontSize);
            if (end == std::string_view::npos) break;
            start = end + 1;
            ++line;
        }
    }

    void renderMenu(SDL_Renderer *renderer, int width, int height, MenuPage page, const widemelon::Config &config, const std::string &theme, const std::string &leftStickMode,
                    std::uint8_t sm64AutoFrames, std::uint16_t sm64AutoReleaseDelayMs, std::uint8_t sm64DpadDeadzone, std::uint16_t cursorSpeedLimit,
                    const widemelon::LeftStickCalibration &leftStickCalibration,
                    const std::vector<MenuItem> &items, int selected, int scrollOffset)
    {
        const int centerX = width / 2;
        const int fieldWidth = widemelon::UiFormFieldWidth;
        const int fieldHeight = widemelon::UiFormFieldHeight;
        const int fieldX = centerX - fieldWidth / 2;
        const std::string title = menuTitle(page);
        const int listTop = menuListTop();
        const int listBottom = menuListBottom(height);
        const std::string_view description = widemelon::display::pages::leftStickModeDescription(leftStickMode);
        widemelon::display::MenuRenderContext renderContext;
        renderContext.page.width = width;
        renderContext.page.height = height;
        renderContext.page.drawBackground = [=]() { drawGradientBackground(renderer, width, height); };
        renderContext.renderer = renderer;
        renderContext.listTop = listTop;
        renderContext.listBottom = listBottom;
        renderContext.scrollOffset = scrollOffset;
        for (std::size_t index = 0; index < items.size(); ++index)
            renderContext.items.push_back({menuItemTop(items, index, leftStickMode), menuItemHeight(items[index], leftStickMode)});
        renderContext.renderHeader = [&]()
        {
            drawMenuText(renderer, centerX, 36, title, palette.primary, themeText.titleFontSize);
        };
        renderContext.renderItem = [&](std::size_t index, int rowY)
        {

            switch (items[index])
            {
            case MenuItem::SectionConnect: drawMenuText(renderer, centerX, rowY, "Connect", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionSettings: drawMenuText(renderer, centerX, rowY, "Settings", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionTheme: drawMenuText(renderer, centerX, rowY, "Theme", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionAnalogSticks: drawMenuText(renderer, centerX, rowY, "Analog sticks", palette.primary, themeText.formFontSize); return;
            case MenuItem::SectionNavigation: drawMenuText(renderer, centerX, rowY, "Navigation", palette.primary, themeText.formFontSize); return;
            case MenuItem::HomeHostLabel: drawMenuText(renderer, centerX, rowY, "Server Address", palette.primary, themeText.formFontSize); return;
            case MenuItem::HomePortLabel: drawMenuText(renderer, centerX, rowY, "Port", palette.primary, themeText.formFontSize); return;
            case MenuItem::HomeSessionCodeLabel: drawMenuText(renderer, centerX, rowY, "Session code", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickModeHint:
                drawMenuMultilineText(renderer, centerX, rowY, "Select the desired behaviour\nfor the left stick while in-game.", palette.hint, themeText.gameplayStatusFontSize);
                return;
            case MenuItem::LeftStickDescription:
                drawMenuMultilineText(renderer, centerX, rowY, description, palette.hint, themeText.gameplayStatusFontSize);
                return;
            case MenuItem::Sm64AutoFramesLabel: drawMenuText(renderer, centerX, rowY, "SM64 Auto delay", palette.primary, themeText.formFontSize); return;
            case MenuItem::Sm64AutoReleaseDelayLabel: drawMenuText(renderer, centerX, rowY, "SM64 Auto release delay", palette.primary, themeText.formFontSize); return;
            case MenuItem::Sm64DpadDeadzoneLabel: drawMenuText(renderer, centerX, rowY, "SM64 D-pad deadzone", palette.primary, themeText.formFontSize); return;
            case MenuItem::CursorSpeedLabel: drawMenuText(renderer, centerX, rowY, "Cursor speed limit", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickCalibrationHint:
                drawMenuMultilineText(renderer, centerX, rowY, "Adjust the range of the left stick\nto correct asymmetrical inputs.\n(experimental feature)", palette.hint, themeText.gameplayStatusFontSize);
                return;
            case MenuItem::LeftStickScaleUpLabel: drawMenuText(renderer, centerX, rowY, "Top multiplier", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickScaleDownLabel: drawMenuText(renderer, centerX, rowY, "Bottom multiplier", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickScaleLeftLabel: drawMenuText(renderer, centerX, rowY, "Left multiplier", palette.primary, themeText.formFontSize); return;
            case MenuItem::LeftStickScaleRightLabel: drawMenuText(renderer, centerX, rowY, "Right multiplier", palette.primary, themeText.formFontSize); return;
            default: break;
            }

            std::string value;
            bool adjustable = false;
            switch (items[index])
            {
            case MenuItem::HomeHost: value = config.host; break;
            case MenuItem::HomePort: value = std::to_string(config.port); break;
            case MenuItem::HomeSessionCode: value = config.pairingCode; break;
            case MenuItem::HomeConnect: value = "Connect"; break;
            case MenuItem::HomeSettings: value = "Settings"; break;
            case MenuItem::Theme: value = theme; adjustable = true; break;
            case MenuItem::LeftStickMod: value = "Left stick mod"; break;
            case MenuItem::LeftStickCalibration: value = "Left stick calibration"; break;
            case MenuItem::LeftStickMode: value = leftStickMode; adjustable = true; break;
            case MenuItem::Sm64AutoFrames: value = std::to_string(sm64AutoFrames) + " frames"; adjustable = true; break;
            case MenuItem::Sm64AutoReleaseDelay: value = std::to_string(sm64AutoReleaseDelayMs) + " ms"; adjustable = true; break;
            case MenuItem::Sm64DpadDeadzone: value = std::to_string(sm64DpadDeadzone) + " %"; adjustable = true; break;
            case MenuItem::CursorSpeed: value = std::to_string(cursorSpeedLimit) + " px/s"; adjustable = true; break;
            case MenuItem::LeftStickScaleUp: value = std::to_string(leftStickCalibration.up) + " %"; adjustable = true; break;
            case MenuItem::LeftStickScaleDown: value = std::to_string(leftStickCalibration.down) + " %"; adjustable = true; break;
            case MenuItem::LeftStickScaleLeft: value = std::to_string(leftStickCalibration.left) + " %"; adjustable = true; break;
            case MenuItem::LeftStickScaleRight: value = std::to_string(leftStickCalibration.right) + " %"; adjustable = true; break;
            case MenuItem::Back: value = "Back"; break;
            case MenuItem::Quit: value = "Quit"; break;
            default: break;
            }

            const SDL_Rect field{fieldX, rowY, fieldWidth, fieldHeight};
            if (adjustable)
                drawAdjustableValue(renderer, field, value, static_cast<int>(index) == selected);
            else
            {
                drawPill(renderer, field, static_cast<int>(index) == selected);
                drawMenuText(renderer, centerX, field.y + (field.h - themeText.formFontSize) / 2, value, palette.primary, themeText.formFontSize);
            }
        };

        renderContext.renderFooter = [&]()
        {
            const int hintY = menuFooterTop(height);
            if (page == MenuPage::Home)
            {
                const int formHintWidth = controlHintWidth(uiTextures.hintA, "A", "EDIT", themeText.hintFontSize)
                    + controlHintWidth(uiTextures.hintStart, "START", "CONNECT", themeText.hintFontSize);
                int hintX = width - formHintWidth - 20;
                hintX = drawControlHint(renderer, uiTextures.hintA, "A", "EDIT", hintX, hintY, themeText.hintFontSize);
                drawControlHint(renderer, uiTextures.hintStart, "START", "CONNECT", hintX, hintY, themeText.hintFontSize);
            }
            else if (page == MenuPage::Settings)
            {
                const int footerWidth = controlHintWidth(uiTextures.hintA, "A", "SELECT", themeText.hintFontSize)
                    + controlHintWidth(uiTextures.hintB, "B", "BACK", themeText.hintFontSize);
                int hintX = width - footerWidth - 20;
                hintX = drawControlHint(renderer, uiTextures.hintA, "A", "SELECT", hintX, hintY, themeText.hintFontSize);
                drawControlHint(renderer, uiTextures.hintB, "B", "BACK", hintX, hintY, themeText.hintFontSize);
            }
            else
            {
                const int footerWidth = (page == MenuPage::LeftStickMod ? controlHintWidth(uiTextures.hintA, "A", "SELECT", themeText.hintFontSize) : 0)
                    + controlHintWidth(uiTextures.hintB, "B", "BACK", themeText.hintFontSize);
                int hintX = width - footerWidth - 20;
                if (page == MenuPage::LeftStickMod)
                    hintX = drawControlHint(renderer, uiTextures.hintA, "A", "SELECT", hintX, hintY, themeText.hintFontSize);
                drawControlHint(renderer, uiTextures.hintB, "B", "BACK", hintX, hintY, themeText.hintFontSize);
            }
        };
        renderContext.present = [=]() { SDL_RenderPresent(renderer); };
        widemelon::display::PageRenderer::renderMenu(renderContext);
    }

    SettingsResult editSettings(SDL_Renderer *renderer, int width, int height, const widemelon::Config &config, widemelon::EvdevInput &input)
    {
        std::array<char, 4096> executable{};
        const ssize_t length = readlink("/proc/self/exe", executable.data(), executable.size() - 1);
        if (length <= 0) return SettingsResult::Home;
        executable[static_cast<std::size_t>(length)] = 0;
        const std::string directory = std::filesystem::path(executable.data()).parent_path().string();
        const widemelon::display::UiConfig uiConfig = uiConfigFor(directory);
        std::vector<std::string> themes = availableThemes(directory);
        if (themes.empty()) return SettingsResult::Home;
        std::size_t themeIndex = 0;
        const std::string currentTheme = uiConfig.readValue(UiConfigThemeKey).value_or("DukuSlice");
        const auto current = std::find(themes.begin(), themes.end(), currentTheme);
        if (current != themes.end()) themeIndex = static_cast<std::size_t>(std::distance(themes.begin(), current));

        const std::array<std::string, 6> leftStickModes{"Disabled", "D-pad", "SM64 Auto", "SM64 Manual", "SM64 D-pad", "Cursor"};
        std::size_t leftStickModeIndex = 0;
        const std::string currentLeftStickMode = uiConfig.readValue(UiConfigLeftStickModeKey).value_or("D-pad");
        const auto currentLeftStick = std::find(leftStickModes.begin(), leftStickModes.end(), currentLeftStickMode);
        if (currentLeftStick != leftStickModes.end())
            leftStickModeIndex = static_cast<std::size_t>(std::distance(leftStickModes.begin(), currentLeftStick));
        else if (currentLeftStickMode == "SM64")
            leftStickModeIndex = 2;

        std::uint8_t sm64AutoFrames = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64AutoFramesKey, widemelon::Sm64TouchCenterHoldFramesDefault,
                                                                                  widemelon::Sm64TouchCenterHoldFramesMinimum, widemelon::Sm64TouchCenterHoldFramesMaximum));
        std::uint16_t sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(uiConfig.readInt(UiConfigSm64AutoReleaseDelayKey, widemelon::Sm64TouchReleaseDelayDefaultMs,
                                                                                            widemelon::Sm64TouchReleaseDelayMinimumMs, widemelon::Sm64TouchReleaseDelayMaximumMs));
        std::uint16_t cursorSpeedLimit = static_cast<std::uint16_t>(uiConfig.readInt(UiConfigCursorSpeedKey, widemelon::CursorSpeedDefault,
                                                                                      widemelon::CursorSpeedMinimum, widemelon::CursorSpeedMaximum));
        std::uint8_t sm64DpadDeadzone = static_cast<std::uint8_t>(uiConfig.readInt(UiConfigSm64DpadDeadzoneKey, widemelon::Sm64DpadDeadzoneDefaultPercent,
                                                                                    widemelon::Sm64DpadDeadzoneMinimumPercent, widemelon::Sm64DpadDeadzoneMaximumPercent));
        widemelon::LeftStickCalibration leftStickCalibration{
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleLeftKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleRightKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleUpKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
            static_cast<std::uint8_t>(uiConfig.readInt(UiConfigLeftStickScaleDownKey, widemelon::LeftStickScaleDefaultPercent, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)),
        };
        MenuPage page = MenuPage::Settings;
        int selected = 1;
        int scrollOffset = 0;
        widemelon::display::MenuNavigation navigation;
        while (true)
        {
            const std::string &leftStickMode = leftStickModes[leftStickModeIndex];
            const std::vector<MenuItem> items = menuItems(page, leftStickMode);
            if (!items.empty())
            {
                selected = std::min(selected, static_cast<int>(items.size()) - 1);
                scrollOffset = menuScrollOffset(items, leftStickMode, selected, scrollOffset, height);
            }
            renderMenu(renderer, width, height, page, config, themes[themeIndex], leftStickMode, sm64AutoFrames, sm64AutoReleaseDelayMs,
                           sm64DpadDeadzone, cursorSpeedLimit, leftStickCalibration, items, selected, scrollOffset);

            input.pollEvent();
            if (input.exitComboPressed()) return SettingsResult::Exit;
            const auto action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
            if (action == widemelon::UiAction::Back || action == widemelon::UiAction::Delete)
            {
                if (page == MenuPage::Settings) return SettingsResult::Home;
                page = MenuPage::Settings;
                selected = 1;
                scrollOffset = 0;
                continue;
            }
            if (items.empty())
            {
                SDL_Delay(10);
                continue;
            }

            const std::vector<widemelon::display::MenuNavigationItem> navigationItems = menuNavigationItems(items, leftStickMode);
            if (action == widemelon::UiAction::Up) selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, -1);
            else if (action == widemelon::UiAction::Down) selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, 1);
            else if (action == widemelon::UiAction::Left || action == widemelon::UiAction::Right)
            {
                const int direction = action == widemelon::UiAction::Left ? -1 : 1;
                switch (items[static_cast<std::size_t>(selected)])
                {
                case MenuItem::Theme:
                {
                    const std::size_t nextTheme = static_cast<std::size_t>((static_cast<int>(themeIndex) + direction + static_cast<int>(themes.size())) % static_cast<int>(themes.size()));
                    if (uiConfig.writeValue(UiConfigThemeKey, themes[nextTheme]))
                    {
                        themeIndex = nextTheme;
                        reloadUiResources(renderer);
                    }
                    break;
                }
                case MenuItem::LeftStickMode:
                {
                    const std::size_t nextMode = static_cast<std::size_t>((static_cast<int>(leftStickModeIndex) + direction + static_cast<int>(leftStickModes.size())) % static_cast<int>(leftStickModes.size()));
                    if (uiConfig.writeValue(UiConfigLeftStickModeKey, leftStickModes[nextMode])) leftStickModeIndex = nextMode;
                    break;
                }
                case MenuItem::Sm64AutoFrames:
                {
                    const int next = std::clamp(static_cast<int>(sm64AutoFrames) + direction, static_cast<int>(widemelon::Sm64TouchCenterHoldFramesMinimum), static_cast<int>(widemelon::Sm64TouchCenterHoldFramesMaximum));
                    if (uiConfig.writeInt(UiConfigSm64AutoFramesKey, next, widemelon::Sm64TouchCenterHoldFramesMinimum, widemelon::Sm64TouchCenterHoldFramesMaximum)) sm64AutoFrames = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::Sm64AutoReleaseDelay:
                {
                    const int next = std::clamp(static_cast<int>(sm64AutoReleaseDelayMs) + direction * static_cast<int>(widemelon::Sm64TouchReleaseDelayStepMs), static_cast<int>(widemelon::Sm64TouchReleaseDelayMinimumMs), static_cast<int>(widemelon::Sm64TouchReleaseDelayMaximumMs));
                    if (uiConfig.writeInt(UiConfigSm64AutoReleaseDelayKey, next, widemelon::Sm64TouchReleaseDelayMinimumMs, widemelon::Sm64TouchReleaseDelayMaximumMs)) sm64AutoReleaseDelayMs = static_cast<std::uint16_t>(next);
                    break;
                }
                case MenuItem::Sm64DpadDeadzone:
                {
                    const int next = std::clamp(static_cast<int>(sm64DpadDeadzone) + direction, static_cast<int>(widemelon::Sm64DpadDeadzoneMinimumPercent), static_cast<int>(widemelon::Sm64DpadDeadzoneMaximumPercent));
                    if (uiConfig.writeInt(UiConfigSm64DpadDeadzoneKey, next, widemelon::Sm64DpadDeadzoneMinimumPercent, widemelon::Sm64DpadDeadzoneMaximumPercent)) sm64DpadDeadzone = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::CursorSpeed:
                {
                    const int next = std::clamp(static_cast<int>(cursorSpeedLimit) + direction * static_cast<int>(widemelon::CursorSpeedStep), static_cast<int>(widemelon::CursorSpeedMinimum), static_cast<int>(widemelon::CursorSpeedMaximum));
                    if (uiConfig.writeInt(UiConfigCursorSpeedKey, next, widemelon::CursorSpeedMinimum, widemelon::CursorSpeedMaximum)) cursorSpeedLimit = static_cast<std::uint16_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleUp:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.up) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleUpKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.up = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleDown:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.down) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleDownKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.down = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleLeft:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.left) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleLeftKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.left = static_cast<std::uint8_t>(next);
                    break;
                }
                case MenuItem::LeftStickScaleRight:
                {
                    const int next = std::clamp(static_cast<int>(leftStickCalibration.right) + direction, static_cast<int>(widemelon::LeftStickScaleMinimumPercent), static_cast<int>(widemelon::LeftStickScaleMaximumPercent));
                    if (uiConfig.writeInt(UiConfigLeftStickScaleRightKey, next, widemelon::LeftStickScaleMinimumPercent, widemelon::LeftStickScaleMaximumPercent)) leftStickCalibration.right = static_cast<std::uint8_t>(next);
                    break;
                }
                default: break;
                }
            }
            else if (action == widemelon::UiAction::Confirm)
            {
                switch (items[static_cast<std::size_t>(selected)])
                {
                case MenuItem::LeftStickMod:
                    page = MenuPage::LeftStickMod;
                    selected = 2;
                    scrollOffset = 0;
                    break;
                case MenuItem::LeftStickCalibration:
                    page = MenuPage::LeftStickCalibration;
                    selected = 2;
                    scrollOffset = 0;
                    break;
                case MenuItem::Back:
                    if (page == MenuPage::Settings) return SettingsResult::Home;
                    page = MenuPage::Settings;
                    selected = 1;
                    scrollOffset = 0;
                    break;
                case MenuItem::Quit: return SettingsResult::Exit;
                default: break;
                }
            }
            SDL_Delay(10);
        }
    }

}
bool MenuScreen::editConfiguration(SDL_Renderer *renderer, int width, int height, Config &config, EvdevInput &input)
    {
        const std::vector<MenuItem> items = menuItems(MenuPage::Home, "");
        int selected = 0;
        int scrollOffset = 0;
        widemelon::display::MenuNavigation navigation;
        while (true)
        {
            const std::vector<widemelon::display::MenuNavigationItem> navigationItems = menuNavigationItems(items);
            scrollOffset = menuScrollOffset(items, {}, selected, scrollOffset, height);
            renderMenu(renderer, width, height, MenuPage::Home, config, "", "", 0, 0, 0, 0, widemelon::LeftStickCalibration{}, items, selected, scrollOffset);

            input.pollEvent();
            if (input.exitComboPressed()) return false;
            const widemelon::UiAction action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
            if (action == widemelon::UiAction::Up)
                selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, -1);
            else if (action == widemelon::UiAction::Down)
                selected = widemelon::display::MenuNavigation::nextSelection(navigationItems, selected, 1);
            else if (action == widemelon::UiAction::Confirm || action == widemelon::UiAction::Start)
            {
                if (action == widemelon::UiAction::Start)
                    selected = 7;
                switch (items[static_cast<std::size_t>(selected)])
                {
                case MenuItem::HomeHost:
                    widemelon::display::NumpadScreen::edit(renderer, width, height, input, "HOST ADDRESS", config.host, 15, true);
                    break;
                case MenuItem::HomePort:
                {
                    std::string port = std::to_string(config.port);
                    if (widemelon::display::NumpadScreen::edit(renderer, width, height, input, "PORT", port, 5, false))
                    {
                        unsigned int parsed = 0;
                        const auto result = std::from_chars(port.data(), port.data() + port.size(), parsed);
                        if (result.ec == std::errc{} && result.ptr == port.data() + port.size() && parsed <= 65535)
                            config.port = static_cast<std::uint16_t>(parsed);
                        else
                            widemelon::Logger::error("Configuration form error: invalid port");
                    }
                    break;
                }
                case MenuItem::HomeSessionCode:
                    widemelon::display::NumpadScreen::edit(renderer, width, height, input, "SESSION CODE", config.pairingCode, 16, false);
                    break;
                case MenuItem::HomeConnect:
                {
                    const widemelon::ConfigLoadResult checked = widemelon::ConfigLoader::validate(config);
                    if (!checked.ok)
                    {
                        widemelon::Logger::error("Configuration form error: " + checked.error);
                        break;
                    }
                    std::string error;
                    if (widemelon::ConfigLoader::saveConfiguration(config, error))
                    {
                        widemelon::Logger::info("Configuration saved from setup form");
                        return true;
                    }
                    // Persistence is optional: the values entered in this
                    // session remain valid and must not prevent connecting.
                    widemelon::Logger::error("Configuration save warning: " + error);
                    return true;
                }
                case MenuItem::HomeSettings:
                    if (editSettings(renderer, width, height, config, input) == SettingsResult::Exit)
                        return false;
                    break;
                default:
                    break;
                }
            }
            SDL_Delay(10);
        }
    }



void MenuScreen::renderConnecting(SDL_Renderer *renderer, int width, int height, const Config &config)
{
    const std::vector<MenuItem> items = menuItems(MenuPage::Home, "");
    renderMenu(renderer, width, height, MenuPage::Home, config, "", "", 0, 0, 0, 0, LeftStickCalibration{}, items, 4, 0);
}
}
