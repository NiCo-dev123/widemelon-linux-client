#include "display/MenuScreen.h"

#include "common/Logger.h"
#include "display/ConfigScreen.h"
#include "display/MenuEntry.h"
#include "display/MenuNavigation.h"
#include "display/NumpadScreen.h"
#include "display/PageRenderer.h"
#include "display/UiConfig.h"
#include "display/UiTheme.h"
#include "display/pages/HomePage.h"
#include "display/pages/AboutPage.h"
#include "display/pages/LeftStickCalibrationPage.h"
#include "display/pages/LeftStickModPage.h"
#include "display/pages/RightStickModPage.h"
#include "display/pages/SettingsPage.h"
#include "input/CursorStickMod.h"
#include "input/EvdevInput.h"
#include "input/LeftStickCalibration.h"
#include "input/MphManualStickMod.h"
#include "input/Sm64DpadMod.h"
#include "input/Sm64StickMod.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <unistd.h>

namespace widemelon::display
{
namespace
{
constexpr std::string_view ThemeKey{"active-theme"}, ModeKey{"left-stick-mod"}, RightModeKey{"right-stick-mod"}, MphSpeedKey{"mph-manual-speed"};
constexpr std::string_view AutoFramesKey{"sm64-auto-center-hold-frames"}, AutoReleaseKey{"sm64-auto-release-delay-ms"};
constexpr std::string_view CursorSpeedKey{"cursor-speed-limit"}, DpadDeadzoneKey{"sm64-dpad-deadzone-percent"};
constexpr std::array<std::string_view, 4> CalibrationKeys{
    "left-stick-scale-left-percent", "left-stick-scale-right-percent", "left-stick-scale-up-percent", "left-stick-scale-down-percent"};

struct MenuState
{
    std::string theme;
    std::string mode;
    std::string rightMode{"Disabled"};
    std::uint16_t mphSpeed{MphManualSpeedDefault};
    std::uint8_t autoFrames{Sm64TouchCenterHoldFramesDefault};
    std::uint16_t autoRelease{Sm64TouchReleaseDelayDefaultMs};
    std::uint8_t dpadDeadzone{Sm64DpadDeadzoneDefaultPercent};
    std::uint16_t cursorSpeed{CursorSpeedDefault};
    LeftStickCalibration calibration{};
};

const PageDefinition &pageDefinition(PageId id, std::string_view leftMode, std::string_view rightMode)
{
    switch (id)
    {
    case PageId::Home: return pages::homePage();
    case PageId::Settings: return pages::settingsPage();
    case PageId::About: return pages::aboutPage();
    case PageId::LeftStickCalibration: return pages::leftStickCalibrationPage();
    case PageId::LeftStickMod: { static PageDefinition page; page = pages::leftStickModPage(leftMode); return page; }
    case PageId::RightStickMod: { static PageDefinition page; page = pages::rightStickModPage(rightMode); return page; }
    default: return pages::settingsPage();
    }
}

std::vector<MenuEntry> entriesFor(const PageDefinition &page)
{
    std::vector<MenuEntry> entries;
    for (const SectionDefinition &section : page.sections)
    {
        if (!section.title.empty()) entries.push_back({MenuEntryType::SectionTitle, &section, nullptr, section.title});
        for (const std::string_view hint : section.hintLines) entries.push_back({MenuEntryType::Hint, &section, nullptr, hint});
        for (const FieldDefinition &field : section.fields) entries.push_back({MenuEntryType::Field, &section, &field, {}});
    }
    return entries;
}

int lineCount(std::string_view text) { return 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n')); }
int entryHeight(const MenuEntry &entry)
{
    const ThemeTextSettings &text = uiTextSettings();
    return entry.type == MenuEntryType::Hint ? lineCount(entry.text) * text.gameplayStatusFontSize + (lineCount(entry.text) - 1) * 8
                                              : entry.type == MenuEntryType::Field ? UiFormFieldHeight : text.formFontSize;
}
int entryTop(const std::vector<MenuEntry> &entries, std::size_t index)
{
    int top = 0;
    for (std::size_t i = 0; i < index; ++i) top += entryHeight(entries[i]) + UiMenuItemGap;
    return top;
}
int footerTop(int height) { PageRenderContext c; c.height = height; return PageRenderer::footerTop(c); }
int listTop() { PageRenderContext c; c.itemGap = UiMenuItemGap; c.titleFontSize = uiTextSettings().titleFontSize; return PageRenderer::firstItemY(c); }

std::vector<MenuNavigationItem> navigationItems(const std::vector<MenuEntry> &entries)
{
    std::vector<MenuNavigationItem> items;
    for (std::size_t i = 0; i < entries.size(); ++i) items.push_back({entries[i].selectable(), entryTop(entries, i), entryHeight(entries[i])});
    return items;
}

int firstSelectable(const std::vector<MenuNavigationItem> &items)
{
    for (std::size_t index = 0; index < items.size(); ++index)
        if (items[index].selectable) return static_cast<int>(index);
    return 0;
}

std::string fieldValue(const FieldDefinition &field, const Config &config, const MenuState &state)
{
    switch (field.value)
    {
    case ValueId::Host: return config.host;
    case ValueId::Port: return std::to_string(config.port);
    case ValueId::PairingCode: return config.pairingCode;
    case ValueId::ActiveTheme: return state.theme;
    case ValueId::LeftStickMode: return state.mode;
    case ValueId::RightStickMode: return state.rightMode;
    case ValueId::MphManualSpeed: return std::to_string(state.mphSpeed) + " px/s";
    case ValueId::Sm64AutoDelayFrames: return std::to_string(state.autoFrames) + " frames";
    case ValueId::Sm64AutoReleaseDelayMs: return std::to_string(state.autoRelease) + " ms";
    case ValueId::Sm64DpadDeadzonePercent: return std::to_string(state.dpadDeadzone) + " %";
    case ValueId::CursorSpeedLimit: return std::to_string(state.cursorSpeed) + " px/s";
    case ValueId::LeftStickScaleLeftPercent: return std::to_string(state.calibration.left) + " %";
    case ValueId::LeftStickScaleRightPercent: return std::to_string(state.calibration.right) + " %";
    case ValueId::LeftStickScaleUpPercent: return std::to_string(state.calibration.up) + " %";
    case ValueId::LeftStickScaleDownPercent: return std::to_string(state.calibration.down) + " %";
    default: return std::string(field.label);
    }
}

bool isAdjustable(const FieldDefinition &field) { return field.type == FieldType::Choice || field.type == FieldType::Range; }
void text(SDL_Renderer *r, int x, int y, std::string_view value, SDL_Color color, int size)
{ drawTextColored(r, value, x - textWidth(value, size) / 2, y, size, color); }
void multiline(SDL_Renderer *r, int x, int y, std::string_view value)
{
    std::size_t start = 0; int line = 0;
    while (start <= value.size()) { const std::size_t end = value.find('\n', start); const std::string_view current = value.substr(start, end == std::string_view::npos ? value.size() - start : end - start); if (!current.empty()) text(r, x, y + line * (uiTextSettings().gameplayStatusFontSize + 8), current, uiPalette().hint, uiTextSettings().gameplayStatusFontSize); if (end == std::string_view::npos) break; start = end + 1; ++line; }
}

SDL_Texture *icon(FooterIcon value)
{
    const UiTextures &t = uiTextures();
    switch (value) { case FooterIcon::A: return t.hintA; case FooterIcon::B: return t.hintB; case FooterIcon::X: return t.hintX; case FooterIcon::Start: return t.hintStart; case FooterIcon::L: return t.hintL; case FooterIcon::R: return t.hintR; }
    return nullptr;
}
const char *iconFallback(FooterIcon value)
{ switch (value) { case FooterIcon::A: return "A"; case FooterIcon::B: return "B"; case FooterIcon::X: return "X"; case FooterIcon::Start: return "START"; case FooterIcon::L: return "L"; case FooterIcon::R: return "R"; } return ""; }

void render(SDL_Renderer *renderer, int width, int height, const PageDefinition &page, const Config &config,
            const MenuState &state, const std::vector<MenuEntry> &entries, int selected, int scroll)
{
    const ThemeTextSettings &fonts = uiTextSettings();
    const int center = width / 2, fieldX = center - UiFormFieldWidth / 2;
    MenuRenderContext context;
    context.page.width = width; context.page.height = height;
    context.page.drawBackground = [=] { drawGradientBackground(renderer, width, height); };
    context.renderer = renderer; context.listTop = listTop(); context.listBottom = footerTop(height) - UiMenuItemGap; context.scrollOffset = scroll;
    for (std::size_t i = 0; i < entries.size(); ++i) context.items.push_back({entryTop(entries, i), entryHeight(entries[i])});
    context.renderHeader = [&] { text(renderer, center, 36, page.title, uiPalette().primary, fonts.titleFontSize); };
    context.renderItem = [&](std::size_t index, int y) {
        const MenuEntry &entry = entries[index];
        if (entry.type == MenuEntryType::SectionTitle) { text(renderer, center, y, entry.text, uiPalette().primary, fonts.formFontSize); return; }
        if (entry.type == MenuEntryType::Hint) { multiline(renderer, center, y, entry.text); return; }
        const FieldDefinition &field = *entry.field; const std::string value = fieldValue(field, config, state);
        const SDL_Rect rect{fieldX, y, UiFormFieldWidth, UiFormFieldHeight}; drawPill(renderer, rect, static_cast<int>(index) == selected);
        const int yText = y + (rect.h - fonts.formFontSize) / 2;
        if (isAdjustable(field)) { drawTextColored(renderer, "<", rect.x + 12, yText, fonts.formFontSize, uiPalette().primary); drawTextColored(renderer, ">", rect.x + rect.w - textWidth(">", fonts.formFontSize) - 12, yText, fonts.formFontSize, uiPalette().primary); }
        text(renderer, center, yText, value, uiPalette().primary, fonts.formFontSize);
    };
    context.renderFooter = [&] { int total = 0; for (const FooterItem &item : page.footer) total += controlHintWidth(icon(item.icon), iconFallback(item.icon), item.text, fonts.hintFontSize); int x = width - total - 20; for (const FooterItem &item : page.footer) x = drawControlHint(renderer, icon(item.icon), iconFallback(item.icon), item.text, x, footerTop(height), fonts.hintFontSize); };
    context.present = [=] { SDL_RenderPresent(renderer); }; PageRenderer::renderMenu(context);
}

std::vector<std::string> themes(const std::string &directory)
{ std::vector<std::string> result; std::error_code error; for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path(directory) / "assets/themes", error)) if (entry.is_directory(error)) result.push_back(entry.path().filename().string()); std::sort(result.begin(), result.end()); return result; }
UiConfig configFor(const std::string &directory) { return UiConfig(std::filesystem::path(directory) / "widemelon-client-ui.conf"); }
std::string executableDirectory() { std::array<char, 4096> path{}; const ssize_t length = readlink("/proc/self/exe", path.data(), path.size() - 1); return length > 0 ? std::filesystem::path(std::string(path.data(), static_cast<std::size_t>(length))).parent_path().string() : std::string{}; }

void updateRange(const FieldDefinition &field, int direction, MenuState &state, const UiConfig &config)
{
    const int minimum = field.minimum, maximum = field.maximum, step = field.step * direction;
    auto update = [&](std::string_view key, auto &target) { const int value = std::clamp(static_cast<int>(target) + step, minimum, maximum); if (config.writeInt(key, value, minimum, maximum)) target = static_cast<std::decay_t<decltype(target)>>(value); };
    switch (field.value) {
    case ValueId::Sm64AutoDelayFrames: update(AutoFramesKey, state.autoFrames); break;
    case ValueId::Sm64AutoReleaseDelayMs: update(AutoReleaseKey, state.autoRelease); break;
    case ValueId::Sm64DpadDeadzonePercent: update(DpadDeadzoneKey, state.dpadDeadzone); break;
    case ValueId::CursorSpeedLimit: update(CursorSpeedKey, state.cursorSpeed); break;
    case ValueId::MphManualSpeed: update(MphSpeedKey, state.mphSpeed); break;
    case ValueId::LeftStickScaleLeftPercent: update(CalibrationKeys[0], state.calibration.left); break;
    case ValueId::LeftStickScaleRightPercent: update(CalibrationKeys[1], state.calibration.right); break;
    case ValueId::LeftStickScaleUpPercent: update(CalibrationKeys[2], state.calibration.up); break;
    case ValueId::LeftStickScaleDownPercent: update(CalibrationKeys[3], state.calibration.down); break;
    default: break; }
}

enum class Result { Home, Exit };
Result editSettings(SDL_Renderer *renderer, int width, int height, const Config &config, EvdevInput &input)
{
    const std::string directory = executableDirectory(); if (directory.empty()) return Result::Home;
    const UiConfig uiConfig = configFor(directory); std::vector<std::string> availableThemes = themes(directory); if (availableThemes.empty()) return Result::Home;
    MenuState state; state.theme = uiConfig.readValue(ThemeKey).value_or("DukuSlice"); state.mode = uiConfig.readValue(ModeKey).value_or("D-pad"); state.rightMode = uiConfig.readValue(RightModeKey).value_or("Disabled"); state.mphSpeed = static_cast<std::uint16_t>(uiConfig.readInt(MphSpeedKey, MphManualSpeedDefault, MphManualSpeedMinimum, MphManualSpeedMaximum));
    state.autoFrames = static_cast<std::uint8_t>(uiConfig.readInt(AutoFramesKey, Sm64TouchCenterHoldFramesDefault, Sm64TouchCenterHoldFramesMinimum, Sm64TouchCenterHoldFramesMaximum));
    state.autoRelease = static_cast<std::uint16_t>(uiConfig.readInt(AutoReleaseKey, Sm64TouchReleaseDelayDefaultMs, Sm64TouchReleaseDelayMinimumMs, Sm64TouchReleaseDelayMaximumMs));
    state.dpadDeadzone = static_cast<std::uint8_t>(uiConfig.readInt(DpadDeadzoneKey, Sm64DpadDeadzoneDefaultPercent, Sm64DpadDeadzoneMinimumPercent, Sm64DpadDeadzoneMaximumPercent));
    state.cursorSpeed = static_cast<std::uint16_t>(uiConfig.readInt(CursorSpeedKey, CursorSpeedDefault, CursorSpeedMinimum, CursorSpeedMaximum));
    state.calibration = {static_cast<std::uint8_t>(uiConfig.readInt(CalibrationKeys[0], LeftStickScaleDefaultPercent, LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent)), static_cast<std::uint8_t>(uiConfig.readInt(CalibrationKeys[1], LeftStickScaleDefaultPercent, LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent)), static_cast<std::uint8_t>(uiConfig.readInt(CalibrationKeys[2], LeftStickScaleDefaultPercent, LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent)), static_cast<std::uint8_t>(uiConfig.readInt(CalibrationKeys[3], LeftStickScaleDefaultPercent, LeftStickScaleMinimumPercent, LeftStickScaleMaximumPercent))};
    const std::array<std::string, 6> modes{"Disabled", "D-pad", "SM64 Auto", "SM64 Manual", "SM64 D-pad", "Cursor"};
    PageId pageId = PageId::Settings; int selected = 0, scroll = 0; MenuNavigation navigation;
    while (true) {
        const PageDefinition &page = pageDefinition(pageId, state.mode, state.rightMode); const std::vector<MenuEntry> entries = entriesFor(page); const auto nav = navigationItems(entries);
        if (selected < 0 || selected >= static_cast<int>(nav.size()) || !nav[static_cast<std::size_t>(selected)].selectable) selected = firstSelectable(nav);
        scroll = MenuNavigation::scrollOffsetForSelection(nav, selected, scroll, footerTop(height) - UiMenuItemGap - listTop()); render(renderer, width, height, page, config, state, entries, selected, scroll);
        input.pollEvent(); if (input.exitComboPressed()) return Result::Exit; const UiAction action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
        if (action == UiAction::Back || action == UiAction::Delete) { if (pageId == PageId::Settings) return Result::Home; pageId = PageId::Settings; selected = 0; scroll = 0; continue; }
        if (action == UiAction::Up || action == UiAction::Down) { selected = MenuNavigation::nextSelection(nav, selected, action == UiAction::Up ? -1 : 1); SDL_Delay(10); continue; }
        if (selected < 0 || selected >= static_cast<int>(entries.size())) { SDL_Delay(10); continue; }
        const FieldDefinition &field = *entries[static_cast<std::size_t>(selected)].field;
        if ((action == UiAction::Left || action == UiAction::Right) && isAdjustable(field)) {
            const int direction = action == UiAction::Left ? -1 : 1;
            if (field.value == ValueId::ActiveTheme) { auto it = std::find(availableThemes.begin(), availableThemes.end(), state.theme); const int index = it == availableThemes.end() ? 0 : static_cast<int>(std::distance(availableThemes.begin(), it)); state.theme = availableThemes[static_cast<std::size_t>((index + direction + static_cast<int>(availableThemes.size())) % static_cast<int>(availableThemes.size()))]; if (uiConfig.writeValue(ThemeKey, state.theme)) reloadUiResources(renderer); }
            else if (field.value == ValueId::LeftStickMode) { auto it = std::find(modes.begin(), modes.end(), state.mode); const int index = it == modes.end() ? 0 : static_cast<int>(std::distance(modes.begin(), it)); state.mode = modes[static_cast<std::size_t>((index + direction + static_cast<int>(modes.size())) % static_cast<int>(modes.size()))]; uiConfig.writeValue(ModeKey, state.mode); }
            else if (field.value == ValueId::RightStickMode) { static const std::array<std::string, 3> rightModes{"Disabled", "Cursor", "MPH Manual"}; auto it = std::find(rightModes.begin(), rightModes.end(), state.rightMode); const int index = it == rightModes.end() ? 0 : static_cast<int>(std::distance(rightModes.begin(), it)); state.rightMode = rightModes[static_cast<std::size_t>((index + direction + static_cast<int>(rightModes.size())) % static_cast<int>(rightModes.size()))]; uiConfig.writeValue(RightModeKey, state.rightMode); }
            else updateRange(field, direction, state, uiConfig);
        } else if (action == UiAction::Confirm) {
            if (field.action == ActionId::Quit) return Result::Exit;
            if (field.type == FieldType::NavigationButton)
            {
                if (field.destination == PageId::Home) { if (pageId == PageId::Settings) return Result::Home; pageId = PageId::Settings; }
                else if (field.destination != pageId) { pageId = field.destination; selected = 0; scroll = 0; }
            }
        }
        SDL_Delay(10);
    }
}
}

bool MenuScreen::editConfiguration(SDL_Renderer *renderer, int width, int height, Config &config, EvdevInput &input)
{
    int selected = 0, scroll = 0; MenuNavigation navigation;
    while (true) {
        const PageDefinition &page = pages::homePage(); const std::vector<MenuEntry> entries = entriesFor(page); const auto nav = navigationItems(entries);
        if (selected < 0 || selected >= static_cast<int>(nav.size()) || !nav[static_cast<std::size_t>(selected)].selectable) selected = firstSelectable(nav);
        scroll = MenuNavigation::scrollOffsetForSelection(nav, selected, scroll, footerTop(height) - UiMenuItemGap - listTop()); MenuState state; render(renderer, width, height, page, config, state, entries, selected, scroll);
        input.pollEvent(); if (input.exitComboPressed()) return false; const UiAction action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
        if (action == UiAction::Up || action == UiAction::Down) { selected = MenuNavigation::nextSelection(nav, selected, action == UiAction::Up ? -1 : 1); SDL_Delay(10); continue; }
        if (action == UiAction::Start) { for (std::size_t i = 0; i < entries.size(); ++i) if (entries[i].field && entries[i].field->action == ActionId::Connect) { selected = static_cast<int>(i); break; } }
        if (action == UiAction::Confirm || action == UiAction::Start) { const FieldDefinition &field = *entries[static_cast<std::size_t>(selected)].field; if (field.value == ValueId::Host) NumpadScreen::edit(renderer, width, height, input, "HOST ADDRESS", config.host, 15, true); else if (field.value == ValueId::Port) { std::string port = std::to_string(config.port); if (NumpadScreen::edit(renderer, width, height, input, "PORT", port, 5, false)) { unsigned int parsed = 0; const auto result = std::from_chars(port.data(), port.data() + port.size(), parsed); if (result.ec == std::errc{} && result.ptr == port.data() + port.size() && parsed <= 65535) config.port = static_cast<std::uint16_t>(parsed); else Logger::error("Configuration form error: invalid port"); } } else if (field.value == ValueId::PairingCode) NumpadScreen::edit(renderer, width, height, input, "SESSION CODE", config.pairingCode, 16, false); else if (field.destination == PageId::Settings) { if (editSettings(renderer, width, height, config, input) == Result::Exit) return false; } else if (field.action == ActionId::Connect) { const ConfigLoadResult checked = ConfigLoader::validate(config); if (!checked.ok) { Logger::error("Configuration form error: " + checked.error); } else { std::string error; if (!ConfigLoader::saveConfiguration(config, error)) Logger::error("Configuration save warning: " + error); else Logger::info("Configuration saved from setup form"); return true; } } }
        SDL_Delay(10);
    }
}

void MenuScreen::renderConnecting(SDL_Renderer *renderer, int width, int height, const Config &config)
{
    MenuState state; const PageDefinition &page = pages::homePage(); const std::vector<MenuEntry> entries = entriesFor(page); render(renderer, width, height, page, config, state, entries, 0, 0);
}
}
