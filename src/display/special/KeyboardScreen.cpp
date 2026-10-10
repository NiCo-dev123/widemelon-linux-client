#include "display/special/KeyboardScreen.h"

#include "display/ConfigScreen.h"
#include "display/MenuNavigation.h"
#include "display/PageDefinition.h"
#include "display/UiTheme.h"
#include "input/EvdevInput.h"

#include <array>
#include <cctype>
#include <string_view>

namespace widemelon::display
{
    namespace
    {
        constexpr int ColumnCount = 10;
        constexpr int KeySize = 80;
        constexpr int KeyStep = 98;
        constexpr int KeyStartY = 225;
        enum class KeyType
        {
            Character,
            Space,
            Shift,
            ShiftLock
        };
        struct Key
        {
            std::string_view value;
            KeyType type{KeyType::Character};
        };
        const std::array<Key, 40> Keys{
            Key{"1"},
            Key{"2"},
            Key{"3"},
            Key{"4"},
            Key{"5"},
            Key{"6"},
            Key{"7"},
            Key{"8"},
            Key{"9"},
            Key{"0"},
            Key{"Q"},
            Key{"W"},
            Key{"E"},
            Key{"R"},
            Key{"T"},
            Key{"Y"},
            Key{"U"},
            Key{"I"},
            Key{"O"},
            Key{"P"},
            Key{"", KeyType::Shift},
            Key{"A"},
            Key{"S"},
            Key{"D"},
            Key{"F"},
            Key{"G"},
            Key{"H"},
            Key{"J"},
            Key{"K"},
            Key{"L"},
            Key{"", KeyType::ShiftLock},
            Key{"Z"},
            Key{"X"},
            Key{"C"},
            Key{"", KeyType::Space},
            Key{"", KeyType::Space},
            Key{"V"},
            Key{"B"},
            Key{"N"},
            Key{"M"},
        };

        void render(SDL_Renderer *renderer, int width, int height, const std::string &title,
                    const std::string &value, int selectedKey, bool shift, bool shiftLock)
        {
            const PageDefinition page{PageId::Numpad, title, {
                                                                 {FooterItemType::Icon, FooterIcon::A, "SELECT"},
                                                                 {FooterItemType::Icon, FooterIcon::B, "BACK"},
                                                                 {FooterItemType::Icon, FooterIcon::Y, "DELETE"},
                                                                 {FooterItemType::Icon, FooterIcon::L, "SHIFT"},
                                                                 {FooterItemType::Icon, FooterIcon::Start, "OK"},
                                                             },
                                      {}};
            const ThemeTextSettings &text = uiTextSettings();
            const UiTextures &textures = uiTextures();
            drawGradientBackground(renderer, width, height);
            drawText(renderer, page.title, (width - textWidth(page.title, text.titleFontSize)) / 2, 65, text.titleFontSize);
            if (!value.empty())
                drawText(renderer, value, (width - textWidth(value, text.keyboardValueFontSize)) / 2, 125, text.keyboardValueFontSize);
            const int startX = (width - ((ColumnCount - 1) * KeyStep + KeySize)) / 2;
            for (std::size_t index = 0; index < Keys.size(); ++index)
            {
                const int row = static_cast<int>(index) / ColumnCount;
                const int column = static_cast<int>(index) % ColumnCount;
                const SDL_Rect key{startX + column * KeyStep, KeyStartY + row * KeySize, KeySize, KeySize};
                const Key &keyboardKey = Keys[index];
                drawKeyboardKey(renderer, key, static_cast<int>(index) == selectedKey || (keyboardKey.type == KeyType::ShiftLock && shiftLock));
                if (keyboardKey.type == KeyType::Shift || keyboardKey.type == KeyType::ShiftLock)
                {
                    const std::string_view glyph = keyboardKey.type == KeyType::Shift ? "↑" : "↑↑";
                    drawSymbol(renderer, glyph, key.x + (key.w - symbolWidth(glyph, text.keyboardValueFontSize)) / 2,
                               key.y + (key.h - text.keyboardValueFontSize) / 2, text.keyboardValueFontSize, uiPalette().primary);
                }
                else if (keyboardKey.type == KeyType::Space)
                {
                    // A blank key is less visually noisy than a text label for space.
                }
                else
                {
                    std::string label(keyboardKey.value);
                    if (!(shift || shiftLock) && std::isalpha(static_cast<unsigned char>(label.front())))
                        label.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(label.front())));
                    drawText(renderer, label, key.x + (key.w - textWidth(label, text.keyboardValueFontSize)) / 2,
                             key.y + (key.h - text.keyboardValueFontSize) / 2, text.keyboardValueFontSize);
                }
            }
            int total = 0;
            for (const FooterItem &item : page.footer)
            {
                SDL_Texture *texture = item.icon == FooterIcon::A ? textures.hintA : item.icon == FooterIcon::B ? textures.hintB
                                                                                 : item.icon == FooterIcon::Y   ? textures.hintY
                                                                                 : item.icon == FooterIcon::L   ? textures.hintL
                                                                                                                : textures.hintStart;
                const std::string_view fallback = item.icon == FooterIcon::A ? "A" : item.icon == FooterIcon::B ? "B"
                                                                                 : item.icon == FooterIcon::Y   ? "Y"
                                                                                 : item.icon == FooterIcon::L   ? "L"
                                                                                                                : "START";
                total += controlHintWidth(texture, fallback, item.text, text.hintFontSize);
            }
            int hintX = (width - total) / 2;
            const int hintY = height - 64;
            hintX = drawControlHint(renderer, textures.hintA, "A", page.footer[0].text, hintX, hintY, text.hintFontSize);
            hintX = drawControlHint(renderer, textures.hintB, "B", page.footer[1].text, hintX, hintY, text.hintFontSize);
            hintX = drawControlHint(renderer, textures.hintY, "Y", page.footer[2].text, hintX, hintY, text.hintFontSize);
            hintX = drawControlHint(renderer, textures.hintL, "L", page.footer[3].text, hintX, hintY, text.hintFontSize);
            drawControlHint(renderer, textures.hintStart, "START", page.footer[4].text, hintX, hintY, text.hintFontSize);
            SDL_RenderPresent(renderer);
        }
    }

    KeyboardScreen::Result KeyboardScreen::edit(SDL_Renderer *renderer, int width, int height, EvdevInput &input,
                                                const std::string &title, std::string &value, std::size_t maximumLength)
    {
        const std::string original = value;
        int selectedKey = 0;
        bool shift = false;
        bool shiftLock = false;
        MenuNavigation navigation;
        while (true)
        {
            render(renderer, width, height, title, value, selectedKey, shift, shiftLock);
            input.pollEvent();
            if (input.exitComboPressed())
                return Result::Exit;
            switch (navigation.nextAction(input.takeUiAction(), input.heldUiDirection()))
            {
            case UiAction::Up:
                selectedKey = (selectedKey + static_cast<int>(Keys.size()) - ColumnCount) % static_cast<int>(Keys.size());
                break;
            case UiAction::Down:
                selectedKey = (selectedKey + ColumnCount) % static_cast<int>(Keys.size());
                break;
            case UiAction::Left:
                selectedKey = (selectedKey + static_cast<int>(Keys.size()) - 1) % static_cast<int>(Keys.size());
                break;
            case UiAction::Right:
                selectedKey = (selectedKey + 1) % static_cast<int>(Keys.size());
                break;
            case UiAction::Delete:
                value = original;
                return Result::Cancelled;
            case UiAction::Y:
                if (!value.empty())
                    value.pop_back();
                break;
            case UiAction::L:
                if (shift || shiftLock)
                {
                    shift = false;
                    shiftLock = false;
                }
                else
                    shift = true;
                break;
            case UiAction::Back:
                break;
            case UiAction::Start:
                return Result::Accepted;
            case UiAction::Confirm:
            {
                const Key &key = Keys[static_cast<std::size_t>(selectedKey)];
                if (key.type == KeyType::Shift || key.type == KeyType::ShiftLock)
                {
                    if (shift || shiftLock)
                    {
                        shift = false;
                        shiftLock = false;
                    }
                    else if (key.type == KeyType::Shift)
                        shift = true;
                    else
                        shiftLock = true;
                }
                else if (value.size() < maximumLength)
                {
                    if (key.type == KeyType::Space)
                        value += ' ';
                    else
                    {
                        char character = key.value.front();
                        if (!(shift || shiftLock) && std::isalpha(static_cast<unsigned char>(character)))
                            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                        value += character;
                    }
                    shift = false;
                }
                break;
            }
            default:
                break;
            }
            SDL_Delay(10);
        }
    }
}
