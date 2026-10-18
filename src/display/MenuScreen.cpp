#include "display/MenuScreen.h"

#include "common/Logger.h"
#include "display/ConfigScreen.h"
#include "display/special/KeyboardScreen.h"
#include "display/MenuEntry.h"
#include "display/MenuNavigation.h"
#include "display/special/NumpadScreen.h"
#include "display/special/PopupScreen.h"
#include "display/PageRenderer.h"
#include "display/UiConfig.h"
#include "display/UiTheme.h"
#include "display/pages/HomePage.h"
#include "display/pages/AboutPage.h"
#include "display/pages/InputPresetManagerPage.h"
#include "display/pages/InputPresetEditorPage.h"
#include "display/pages/LeftStickCalibrationPage.h"
#include "display/pages/LeftStickModPage.h"
#include "display/pages/RightStickModPage.h"
#include "display/pages/SettingsPage.h"
#include "input/CursorStickMod.h"
#include "input/EvdevInput.h"
#include "input/LeftStickCalibration.h"
#include "input/InputPreset.h"
#include "input/TspsStickConfig.h"
#include "input/MphAutoStickMod.h"
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
constexpr std::string_view AutoFramesKey{"sm64-auto-center-hold-frames"}, AutoReleaseKey{"sm64-auto-release-delay-ms"}, MphAutoReleaseKey{"mph-auto-release-delay-ms"};
constexpr std::string_view CursorSpeedKey{"cursor-speed-limit"}, DpadDeadzoneKey{"sm64-dpad-deadzone-percent"};

struct MenuState
{
    std::string theme;
    std::string activePreset;
    std::vector<std::string> presetNames;
    std::string mode;
    std::string rightMode{"Disabled"};
    std::uint16_t mphSpeed{MphManualSpeedDefault};
    std::uint16_t mphAutoRelease{MphAutoReleaseDelayMs};
    std::uint8_t autoFrames{Sm64TouchCenterHoldFramesDefault};
    std::uint16_t autoRelease{Sm64TouchReleaseDelayDefaultMs};
    std::uint8_t dpadDeadzone{Sm64DpadDeadzoneDefaultPercent};
    std::uint16_t leftCursorSpeed{CursorSpeedDefault};
    std::uint16_t rightCursorSpeed{CursorSpeedDefault};
    std::uint8_t leftCursorDeadzone{CursorDeadzoneDefaultPercent};
    std::uint8_t rightCursorDeadzone{CursorDeadzoneDefaultPercent};
    LeftStickCalibration calibration{};
    std::array<InputMapping, PhysicalInputCount> mappings{};
};

std::optional<std::size_t> mappingIndex(ValueId value)
{
    const int index = static_cast<int>(value) - static_cast<int>(ValueId::MappingA);
    return index >= 0 && index < static_cast<int>(PhysicalInputCount) ? std::optional<std::size_t>(static_cast<std::size_t>(index)) : std::nullopt;
}

std::string mappingLabel(PhysicalInput input, const InputMapping &mapping)
{
    const std::string output = mapping.output == MappingOutput::KeyPress ? std::string(dsButtonName(mapping.button))
        : mapping.output == MappingOutput::KeyHold ? std::string(dsButtonName(mapping.button)) + " (hold)"
        : std::string(mappingOutputName(mapping.output));
    return std::string(physicalInputName(input)) + " : " + output;
}

const PageDefinition &pageDefinition(PageId id, std::string_view presetName, std::string_view leftMode, std::string_view rightMode)
{
    switch (id)
    {
    case PageId::Home: return pages::homePage();
    case PageId::Settings: return pages::settingsPage();
    case PageId::InputPresetManager: return pages::inputPresetManagerPage();
    case PageId::InputPresetEditor: { static PageDefinition page; page = pages::inputPresetEditorPage(presetName, leftMode, rightMode); return page; }
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
    if (const std::optional<std::size_t> index = mappingIndex(field.value))
        return mappingLabel(static_cast<PhysicalInput>(*index), state.mappings[*index]);
    switch (field.value)
    {
    case ValueId::Host: return config.host;
    case ValueId::Port: return std::to_string(config.port);
    case ValueId::PairingCode: return config.pairingCode;
    case ValueId::ActiveTheme: return state.theme;
    case ValueId::ActiveInputPreset: return state.activePreset.empty() ? "No preset" : state.activePreset;
    case ValueId::LeftStickMode: return state.mode;
    case ValueId::RightStickMode: return state.rightMode;
    case ValueId::MphManualSpeed: return std::to_string(state.mphSpeed) + " px/s";
    case ValueId::MphAutoReleaseDelayMs: return std::to_string(state.mphAutoRelease) + " ms";
    case ValueId::Sm64AutoDelayFrames: return std::to_string(state.autoFrames) + " frames";
    case ValueId::Sm64AutoReleaseDelayMs: return std::to_string(state.autoRelease) + " ms";
    case ValueId::Sm64DpadDeadzonePercent: return std::to_string(state.dpadDeadzone) + " %";
    case ValueId::LeftCursorSpeedLimit: return std::to_string(state.leftCursorSpeed) + " px/s";
    case ValueId::RightCursorSpeedLimit: return std::to_string(state.rightCursorSpeed) + " px/s";
    case ValueId::LeftCursorDeadzonePercent: return "Deadzone: " + std::to_string(state.leftCursorDeadzone) + " %";
    case ValueId::RightCursorDeadzonePercent: return "Deadzone: " + std::to_string(state.rightCursorDeadzone) + " %";
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
    switch (value) { case FooterIcon::A: return t.hintA; case FooterIcon::B: return t.hintB; case FooterIcon::X: return t.hintX; case FooterIcon::Y: return t.hintY; case FooterIcon::Start: return t.hintStart; case FooterIcon::L: return t.hintL; case FooterIcon::R: return t.hintR; }
    return nullptr;
}
const char *iconFallback(FooterIcon value)
{ switch (value) { case FooterIcon::A: return "A"; case FooterIcon::B: return "B"; case FooterIcon::X: return "X"; case FooterIcon::Y: return "Y"; case FooterIcon::Start: return "START"; case FooterIcon::L: return "L"; case FooterIcon::R: return "R"; } return ""; }

void render(SDL_Renderer *renderer, int width, int height, const PageDefinition &page, const Config &config,
            const MenuState &state, const std::vector<MenuEntry> &entries, int selected, int scroll, bool present = true)
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
    context.present = [=] { if (present) SDL_RenderPresent(renderer); }; PageRenderer::renderMenu(context);
}

std::vector<std::string> themes(const std::string &directory)
{ std::vector<std::string> result; std::error_code error; for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path(directory) / "assets/themes", error)) if (entry.is_directory(error)) result.push_back(entry.path().filename().string()); std::sort(result.begin(), result.end()); return result; }
UiConfig configFor(const std::string &directory) { return UiConfig(std::filesystem::path(directory) / "widemelon-client-ui.conf"); }
std::string executableDirectory() { std::array<char, 4096> path{}; const ssize_t length = readlink("/proc/self/exe", path.data(), path.size() - 1); return length > 0 ? std::filesystem::path(std::string(path.data(), static_cast<std::size_t>(length))).parent_path().string() : std::string{}; }

void updateRange(const FieldDefinition &field, int direction, MenuState &state, const UiConfig &config, bool persistLegacyConfig)
{
    const int minimum = field.minimum, maximum = field.maximum, step = field.step * direction;
    auto update = [&](std::string_view key, auto &target)
    {
        const int value = std::clamp(static_cast<int>(target) + step, minimum, maximum);
        if (!persistLegacyConfig || config.writeInt(key, value, minimum, maximum)) target = static_cast<std::decay_t<decltype(target)>>(value);
    };
    auto updateLocal = [&](auto &target)
    {
        target = static_cast<std::decay_t<decltype(target)>>(std::clamp(static_cast<int>(target) + step, minimum, maximum));
    };
    switch (field.value) {
    case ValueId::Sm64AutoDelayFrames: update(AutoFramesKey, state.autoFrames); break;
    case ValueId::Sm64AutoReleaseDelayMs: update(AutoReleaseKey, state.autoRelease); break;
    case ValueId::Sm64DpadDeadzonePercent: update(DpadDeadzoneKey, state.dpadDeadzone); break;
    case ValueId::LeftCursorSpeedLimit: update(CursorSpeedKey, state.leftCursorSpeed); break;
    case ValueId::RightCursorSpeedLimit: update(CursorSpeedKey, state.rightCursorSpeed); break;
    case ValueId::LeftCursorDeadzonePercent: updateLocal(state.leftCursorDeadzone); break;
    case ValueId::RightCursorDeadzonePercent: updateLocal(state.rightCursorDeadzone); break;
    case ValueId::MphManualSpeed: update(MphSpeedKey, state.mphSpeed); break;
    case ValueId::MphAutoReleaseDelayMs: update(MphAutoReleaseKey, state.mphAutoRelease); break;
    case ValueId::LeftStickScaleLeftPercent: updateLocal(state.calibration.left); break;
    case ValueId::LeftStickScaleRightPercent: updateLocal(state.calibration.right); break;
    case ValueId::LeftStickScaleUpPercent: updateLocal(state.calibration.up); break;
    case ValueId::LeftStickScaleDownPercent: updateLocal(state.calibration.down); break;
    default: break; }
}

enum class Result { Home, Game, Exit };
Result editSettings(SDL_Renderer *renderer, int width, int height, const Config &config, EvdevInput &input, bool returnToGame)
{
    const Result backResult = returnToGame ? Result::Game : Result::Home;
    const std::string directory = executableDirectory(); if (directory.empty()) return backResult;
    const UiConfig uiConfig = configFor(directory); std::vector<std::string> availableThemes = themes(directory); if (availableThemes.empty()) return backResult;
    MenuState state; state.theme = uiConfig.readValue(ThemeKey).value_or("DukuSlice"); state.mode = normalizeStickMode(uiConfig.readValue(ModeKey).value_or("D-pad")); state.rightMode = normalizeStickMode(uiConfig.readValue(RightModeKey).value_or("Disabled")); state.mphSpeed = static_cast<std::uint16_t>(uiConfig.readInt(MphSpeedKey, MphManualSpeedDefault, MphManualSpeedMinimum, MphManualSpeedMaximum)); state.mphAutoRelease = static_cast<std::uint16_t>(uiConfig.readInt(MphAutoReleaseKey, MphAutoReleaseDelayMs, MphAutoReleaseDelayMinimumMs, MphAutoReleaseDelayMaximumMs));
    state.autoFrames = static_cast<std::uint8_t>(uiConfig.readInt(AutoFramesKey, Sm64TouchCenterHoldFramesDefault, Sm64TouchCenterHoldFramesMinimum, Sm64TouchCenterHoldFramesMaximum));
    state.autoRelease = static_cast<std::uint16_t>(uiConfig.readInt(AutoReleaseKey, Sm64TouchReleaseDelayDefaultMs, Sm64TouchReleaseDelayMinimumMs, Sm64TouchReleaseDelayMaximumMs));
    state.dpadDeadzone = static_cast<std::uint8_t>(uiConfig.readInt(DpadDeadzoneKey, Sm64DpadDeadzoneDefaultPercent, Sm64DpadDeadzoneMinimumPercent, Sm64DpadDeadzoneMaximumPercent));
    state.leftCursorSpeed = static_cast<std::uint16_t>(uiConfig.readInt(CursorSpeedKey, CursorSpeedDefault, CursorSpeedMinimum, CursorSpeedMaximum));
    state.rightCursorSpeed = state.leftCursorSpeed;
    state.presetNames = InputPresetStore::names();
    if (!InputPresetStore::activeName())
    {
        InputPreset preset = InputPresetStore::defaultPreset();
        preset.name = "Config 1";
        preset.leftStickMode = state.mode; preset.rightStickMode = state.rightMode; preset.mphCameraSpeed = state.mphSpeed; preset.mphAutoReleaseDelayMs = state.mphAutoRelease;
        preset.sm64AutoCenterHoldFrames = state.autoFrames; preset.sm64AutoReleaseDelayMs = state.autoRelease; preset.sm64DpadDeadzonePercent = state.dpadDeadzone; preset.leftCursorSpeedLimit = state.leftCursorSpeed; preset.rightCursorSpeedLimit = state.rightCursorSpeed;
        std::string migrationError;
        if (!InputPresetStore::save(preset, true, migrationError)) Logger::error("Input preset migration warning: " + migrationError);
        state.presetNames = InputPresetStore::names();
    }
    state.activePreset = InputPresetStore::activeName().value_or(state.presetNames.empty() ? std::string{} : state.presetNames.front());
    std::optional<InputPreset> activePreset = InputPresetStore::load(state.activePreset);
    if (!activePreset)
    {
        activePreset = InputPresetStore::defaultPreset();
        std::string fallbackError;
        if (!InputPresetStore::save(*activePreset, true, fallbackError))
            Logger::error("Cannot restore Default input preset; continuing with built-in defaults: " + fallbackError);
        state.activePreset = activePreset->name;
        state.presetNames = InputPresetStore::names();
    }
    if (activePreset)
    {
        state.mappings = activePreset->mappings;
        state.leftCursorSpeed = activePreset->leftCursorSpeedLimit;
        state.rightCursorSpeed = activePreset->rightCursorSpeedLimit;
        state.leftCursorDeadzone = activePreset->leftCursorDeadzonePercent;
        state.rightCursorDeadzone = activePreset->rightCursorDeadzonePercent;
        std::optional<LeftStickCalibration> legacyCalibration = InputPresetStore::legacyLeftStickCalibration(state.activePreset);
        if (!legacyCalibration) legacyCalibration = InputPresetStore::legacyActiveStickCalibration();
        state.calibration = TspsStickConfig::loadOrMigrate(legacyCalibration.value_or(state.calibration));
    }
    const std::array<std::string, 5> modes{"Disabled", "D-pad", "Analog centered", "Cursor", "Cursor wrapped"};
    InputPreset editingPreset;
    bool editing = false;
    auto saveEditingPreset = [&]()
    {
        editingPreset.leftStickMode = state.mode;
        editingPreset.rightStickMode = state.rightMode;
        editingPreset.mphCameraSpeed = state.mphSpeed;
        editingPreset.mphAutoReleaseDelayMs = state.mphAutoRelease;
        editingPreset.sm64AutoCenterHoldFrames = state.autoFrames;
        editingPreset.sm64AutoReleaseDelayMs = state.autoRelease;
        editingPreset.sm64DpadDeadzonePercent = state.dpadDeadzone;
        editingPreset.leftCursorSpeedLimit = state.leftCursorSpeed;
        editingPreset.rightCursorSpeedLimit = state.rightCursorSpeed;
        editingPreset.leftCursorDeadzonePercent = state.leftCursorDeadzone;
        editingPreset.rightCursorDeadzonePercent = state.rightCursorDeadzone;
        editingPreset.mappings = state.mappings;
        std::string presetError;
        if (!InputPresetStore::save(editingPreset, editingPreset.name == state.activePreset, presetError))
        {
            Logger::error("Cannot save input preset: " + presetError);
            return false;
        }
        return true;
    };
    const auto hasUnsavedChanges = [&]()
    {
        return editingPreset.leftStickMode != state.mode
            || editingPreset.rightStickMode != state.rightMode
            || editingPreset.mphCameraSpeed != state.mphSpeed
            || editingPreset.mphAutoReleaseDelayMs != state.mphAutoRelease
            || editingPreset.sm64AutoCenterHoldFrames != state.autoFrames
            || editingPreset.sm64AutoReleaseDelayMs != state.autoRelease
            || editingPreset.sm64DpadDeadzonePercent != state.dpadDeadzone
            || editingPreset.leftCursorSpeedLimit != state.leftCursorSpeed
            || editingPreset.rightCursorSpeedLimit != state.rightCursorSpeed
            || editingPreset.leftCursorDeadzonePercent != state.leftCursorDeadzone
            || editingPreset.rightCursorDeadzonePercent != state.rightCursorDeadzone
            || editingPreset.mappings != state.mappings;
    };
    PageId pageId = PageId::Settings; int selected = 0, scroll = 0; MenuNavigation navigation;
    while (true) {
        const PageDefinition &page = pageDefinition(pageId, editing ? editingPreset.name : state.activePreset, state.mode, state.rightMode); const std::vector<MenuEntry> entries = entriesFor(page); const auto nav = navigationItems(entries);
        if (selected < 0 || selected >= static_cast<int>(nav.size()) || !nav[static_cast<std::size_t>(selected)].selectable) selected = firstSelectable(nav);
        scroll = MenuNavigation::scrollOffsetForSelection(nav, selected, scroll, footerTop(height) - UiMenuItemGap - listTop()); render(renderer, width, height, page, config, state, entries, selected, scroll);
        input.pollEvent(); if (input.exitComboPressed()) return Result::Exit; const UiAction action = navigation.nextAction(input.takeUiAction(), input.heldUiDirection());
        if (action == UiAction::Start && pageId == PageId::InputPresetEditor && editing)
        {
            if (saveEditingPreset())
            {
                pageId = PageId::InputPresetManager;
                selected = 0;
                scroll = 0;
                editing = false;
            }
            continue;
        }
        if (action == UiAction::Back || action == UiAction::Delete)
        {
            if (pageId == PageId::InputPresetEditor && editing && hasUnsavedChanges())
            {
                const PopupScreen::Result popupResult = PopupScreen::saveChanges(renderer, width, height, input);
                if (popupResult == PopupScreen::Result::Exit) return Result::Exit;
                if (popupResult == PopupScreen::Result::Cancel) continue;
                if (popupResult == PopupScreen::Result::Save && !saveEditingPreset()) continue;
                pageId = PageId::InputPresetManager;
                selected = 0;
                scroll = 0;
                editing = false;
                continue;
            }
            if (pageId == PageId::Settings) return backResult;
            pageId = pageId == PageId::InputPresetEditor ? PageId::InputPresetManager : PageId::Settings;
            selected = 0;
            scroll = 0;
            editing = false;
            continue;
        }
        if (action == UiAction::Up || action == UiAction::Down) { selected = MenuNavigation::nextSelection(nav, selected, action == UiAction::Up ? -1 : 1); SDL_Delay(10); continue; }
        if (selected < 0 || selected >= static_cast<int>(entries.size())) { SDL_Delay(10); continue; }
        const FieldDefinition &field = *entries[static_cast<std::size_t>(selected)].field;
        const bool normalAdjustment = action == UiAction::Left || action == UiAction::Right;
        const bool fastRangeAdjustment = action == UiAction::L || action == UiAction::R;
        if ((normalAdjustment && isAdjustable(field)) || (fastRangeAdjustment && field.type == FieldType::Range)) {
            const int direction = (action == UiAction::Left || action == UiAction::L) ? -1 : 1;
            const int stepMultiplier = fastRangeAdjustment ? UiRangeFastStepMultiplier : 1;
            if (field.value == ValueId::ActiveTheme) { auto it = std::find(availableThemes.begin(), availableThemes.end(), state.theme); const int index = it == availableThemes.end() ? 0 : static_cast<int>(std::distance(availableThemes.begin(), it)); state.theme = availableThemes[static_cast<std::size_t>((index + direction + static_cast<int>(availableThemes.size())) % static_cast<int>(availableThemes.size()))]; if (uiConfig.writeValue(ThemeKey, state.theme)) reloadUiResources(renderer); }
            else if (field.value == ValueId::ActiveInputPreset && !state.presetNames.empty()) { auto it = std::find(state.presetNames.begin(), state.presetNames.end(), state.activePreset); const int index = it == state.presetNames.end() ? 0 : static_cast<int>(std::distance(state.presetNames.begin(), it)); const std::string next = state.presetNames[static_cast<std::size_t>((index + direction + static_cast<int>(state.presetNames.size())) % static_cast<int>(state.presetNames.size()))]; std::string presetError; if (InputPresetStore::setActive(next, presetError)) state.activePreset = next; else Logger::error("Cannot select input preset: " + presetError); }
            else if (field.value == ValueId::LeftStickMode) { auto it = std::find(modes.begin(), modes.end(), state.mode); const int index = it == modes.end() ? 0 : static_cast<int>(std::distance(modes.begin(), it)); state.mode = modes[static_cast<std::size_t>((index + direction + static_cast<int>(modes.size())) % static_cast<int>(modes.size()))]; if (!editing) uiConfig.writeValue(ModeKey, state.mode); }
            else if (field.value == ValueId::RightStickMode) { auto it = std::find(modes.begin(), modes.end(), state.rightMode); const int index = it == modes.end() ? 0 : static_cast<int>(std::distance(modes.begin(), it)); state.rightMode = modes[static_cast<std::size_t>((index + direction + static_cast<int>(modes.size())) % static_cast<int>(modes.size()))]; if (!editing) uiConfig.writeValue(RightModeKey, state.rightMode); }
            else
            {
                updateRange(field, direction * stepMultiplier, state, uiConfig, !editing);
                const bool isCalibration = field.value == ValueId::LeftStickScaleLeftPercent || field.value == ValueId::LeftStickScaleRightPercent
                    || field.value == ValueId::LeftStickScaleUpPercent || field.value == ValueId::LeftStickScaleDownPercent;
                if (isCalibration)
                {
                    std::string calibrationError;
                    TspsStickConfig::save(state.calibration, calibrationError);
                }
            }
        } else if (action == UiAction::Confirm) {
            if (field.action == ActionId::Quit) return Result::Exit;
            if (field.action == ActionId::NewInputPreset)
            {
                InputPreset preset = InputPresetStore::defaultPreset();
                for (unsigned int number = 1;; ++number)
                {
                    preset.name = "Config " + std::to_string(number);
                    if (std::find(state.presetNames.begin(), state.presetNames.end(), preset.name) == state.presetNames.end()) break;
                }
                std::string presetError;
                if (InputPresetStore::save(preset, true, presetError)) { state.presetNames = InputPresetStore::names(); state.activePreset = preset.name; }
                else Logger::error("Cannot create input preset: " + presetError);
            }
            if (field.action == ActionId::RenameInputPreset)
            {
                std::string nextName = state.activePreset;
                const KeyboardScreen::Result result = KeyboardScreen::edit(renderer, width, height, input, "RENAME PRESET", nextName, 48);
                if (result == KeyboardScreen::Result::Exit) return Result::Exit;
                if (result == KeyboardScreen::Result::Accepted)
                {
                    if (nextName != state.activePreset
                        && std::find(state.presetNames.begin(), state.presetNames.end(), nextName) != state.presetNames.end())
                    {
                        const std::string message = "A preset named \"" + nextName + "\" already exists.";
                        if (PopupScreen::acknowledge(renderer, width, height, input, message) == PopupScreen::Result::Exit)
                            return Result::Exit;
                        continue;
                    }
                    std::string presetError;
                    if (InputPresetStore::rename(state.activePreset, nextName, presetError))
                    {
                        state.activePreset = nextName;
                        state.presetNames = InputPresetStore::names();
                    }
                    else Logger::error("Cannot rename input preset: " + presetError);
                }
            }
            if (field.action == ActionId::SaveInputPreset && editing)
            {
                if (saveEditingPreset()) { pageId = PageId::InputPresetManager; selected = 0; scroll = 0; editing = false; }
            }
            if (field.action == ActionId::ConfigureInputMapping)
            {
                if (const std::optional<std::size_t> index = mappingIndex(field.value))
                {
                    const PopupScreen::MappingResult result = PopupScreen::editInputMapping(
                        renderer, width, height, input, physicalInputName(static_cast<PhysicalInput>(*index)), state.mappings[*index]);
                    if (result.exit) return Result::Exit;
                    if (result.accepted) state.mappings[*index] = result.mapping;
                }
            }
            if (field.action == ActionId::DiscardInputPreset && editing) { pageId = PageId::InputPresetManager; selected = 0; scroll = 0; editing = false; }
            if (field.action == ActionId::RequestDeleteInputPreset && editing)
            {
                if (state.presetNames.size() <= 1)
                {
                    if (PopupScreen::cannotDeleteLastPreset(renderer, width, height, input) == PopupScreen::Result::Exit) return Result::Exit;
                }
                else
                {
                    const PopupScreen::Result popupResult = PopupScreen::confirmDelete(renderer, width, height, input);
                    if (popupResult == PopupScreen::Result::Exit) return Result::Exit;
                    if (popupResult != PopupScreen::Result::Delete) continue;
                    std::string presetError;
                    if (InputPresetStore::remove(editingPreset.name, presetError))
                    {
                        state.presetNames = InputPresetStore::names();
                        if (state.activePreset == editingPreset.name && !state.presetNames.empty())
                        {
                            if (!InputPresetStore::setActive(state.presetNames.front(), presetError)) Logger::error("Cannot select fallback input preset: " + presetError);
                            else state.activePreset = state.presetNames.front();
                        }
                        pageId = PageId::InputPresetManager;
                        selected = 0;
                        scroll = 0;
                        editing = false;
                    }
                    else Logger::error("Cannot delete input preset: " + presetError);
                }
            }
            if (field.type == FieldType::NavigationButton)
            {
                if (field.destination == PageId::Home) { if (pageId == PageId::Settings) return backResult; pageId = PageId::Settings; }
                else if (field.destination == PageId::InputPresetEditor) {
                    const std::optional<InputPreset> preset = InputPresetStore::load(state.activePreset);
                    if (preset) { editingPreset = *preset; state.mode = preset->leftStickMode; state.rightMode = preset->rightStickMode; state.mphSpeed = preset->mphCameraSpeed; state.mphAutoRelease = preset->mphAutoReleaseDelayMs; state.autoFrames = preset->sm64AutoCenterHoldFrames; state.autoRelease = preset->sm64AutoReleaseDelayMs; state.dpadDeadzone = preset->sm64DpadDeadzonePercent; state.leftCursorSpeed = preset->leftCursorSpeedLimit; state.rightCursorSpeed = preset->rightCursorSpeedLimit; state.leftCursorDeadzone = preset->leftCursorDeadzonePercent; state.rightCursorDeadzone = preset->rightCursorDeadzonePercent; state.mappings = preset->mappings; editing = true; pageId = field.destination; selected = 0; scroll = 0; }
                    else Logger::error("Cannot load selected input preset");
                }
                else if (field.destination != pageId) { pageId = field.destination; selected = 0; scroll = 0; }
            }
        }
        SDL_Delay(10);
    }
}
}

bool MenuScreen::editInGameSettings(SDL_Renderer *renderer, int width, int height, const Config &config, EvdevInput &input)
{
    // Gameplay does not consume UI navigation actions. Discard the last
    // direction before opening Settings so it cannot alter the first field.
    input.takeUiAction();
    return editSettings(renderer, width, height, config, input, true) == Result::Exit;
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
        if (action == UiAction::Confirm || action == UiAction::Start)
        {
            const FieldDefinition &field = *entries[static_cast<std::size_t>(selected)].field;
            if (field.value == ValueId::Host)
            {
                if (NumpadScreen::edit(renderer, width, height, input, "HOST ADDRESS", config.host, 15, true) == NumpadScreen::Result::Exit) return false;
            }
            else if (field.value == ValueId::Port)
            {
                std::string port = std::to_string(config.port);
                const NumpadScreen::Result result = NumpadScreen::edit(renderer, width, height, input, "PORT", port, 5, false);
                if (result == NumpadScreen::Result::Exit) return false;
                if (result == NumpadScreen::Result::Accepted)
                {
                    unsigned int parsed = 0;
                    const auto parsedResult = std::from_chars(port.data(), port.data() + port.size(), parsed);
                    if (parsedResult.ec == std::errc{} && parsedResult.ptr == port.data() + port.size() && parsed <= 65535) config.port = static_cast<std::uint16_t>(parsed);
                    else Logger::error("Configuration form error: invalid port");
                }
            }
            else if (field.value == ValueId::PairingCode)
            {
                if (NumpadScreen::edit(renderer, width, height, input, "SESSION CODE", config.pairingCode, 16, false) == NumpadScreen::Result::Exit) return false;
            }
            else if (field.destination == PageId::Settings)
            {
                if (editSettings(renderer, width, height, config, input, false) == Result::Exit) return false;
            }
            else if (field.action == ActionId::Connect)
            {
                const ConfigLoadResult checked = ConfigLoader::validate(config);
                if (!checked.ok) Logger::error("Configuration form error: " + checked.error);
                else
                {
                    std::string error;
                    if (!ConfigLoader::saveConfiguration(config, error)) Logger::error("Configuration save warning: " + error);
                    else Logger::info("Configuration saved from setup form");
                    return true;
                }
            }
        }
        SDL_Delay(10);
    }
}

void MenuScreen::renderConnecting(SDL_Renderer *renderer, int width, int height, const Config &config)
{
    MenuState state; const PageDefinition &page = pages::homePage(); const std::vector<MenuEntry> entries = entriesFor(page); render(renderer, width, height, page, config, state, entries, 0, 0, false);
}
}
