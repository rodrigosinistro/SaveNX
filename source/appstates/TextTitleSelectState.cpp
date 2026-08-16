#include "appstates/TextTitleSelectState.hpp"

#include "StateManager.hpp"
#include "appstates/BackupMenuState.hpp"
#include "appstates/TitleOptionState.hpp"
#include "config/config.hpp"
#include "graphics/colors.hpp"
#include "input.hpp"
#include "sdl.hpp"
#include "stringutil.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
    constexpr std::string_view SECONDARY_TARGET = "SecondaryTarget";

    constexpr int LIST_X       = 32;
    constexpr int LIST_Y       = 8;
    constexpr int LIST_WIDTH   = 1000;
    constexpr int ROW_HEIGHT   = 39;
    constexpr int VISIBLE_ROWS = 13;
    constexpr int FONT_SIZE    = 23;

    // Keep title shaping bounded. NACP names can be much larger than what the Games
    // list can display, and malformed metadata must never reach SDL/HarfBuzz raw.
    constexpr size_t MAX_DISPLAY_TITLE_BYTES = 128;

    bool is_utf8_continuation(unsigned char value) noexcept { return (value & 0xC0U) == 0x80U; }

    std::string sanitize_display_title(std::string_view input)
    {
        std::string output{};
        output.reserve(std::min(input.size(), MAX_DISPLAY_TITLE_BYTES));

        size_t index{};
        while (index < input.size() && output.size() < MAX_DISPLAY_TITLE_BYTES)
        {
            const unsigned char first = static_cast<unsigned char>(input[index]);

            // Printable ASCII is always safe. Convert ASCII whitespace/control bytes
            // to a normal space so no layout control character reaches the shaper.
            if (first < 0x80U)
            {
                if (first >= 0x20U && first != 0x7FU) { output.push_back(static_cast<char>(first)); }
                else if (first == '\t' || first == '\r' || first == '\n')
                {
                    if (!output.empty() && output.back() != ' ') { output.push_back(' '); }
                }
                ++index;
                continue;
            }

            size_t sequenceLength{};
            bool valid{};

            if (first >= 0xC2U && first <= 0xDFU)
            {
                sequenceLength = 2;
                valid = index + 1 < input.size() &&
                        is_utf8_continuation(static_cast<unsigned char>(input[index + 1]));
            }
            else if (first >= 0xE0U && first <= 0xEFU)
            {
                sequenceLength = 3;
                if (index + 2 < input.size())
                {
                    const unsigned char second = static_cast<unsigned char>(input[index + 1]);
                    const unsigned char third  = static_cast<unsigned char>(input[index + 2]);
                    valid = is_utf8_continuation(second) && is_utf8_continuation(third);

                    // Reject overlong encodings and UTF-16 surrogate code points.
                    if (first == 0xE0U && second < 0xA0U) { valid = false; }
                    if (first == 0xEDU && second >= 0xA0U) { valid = false; }
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U)
            {
                sequenceLength = 4;
                if (index + 3 < input.size())
                {
                    const unsigned char second = static_cast<unsigned char>(input[index + 1]);
                    const unsigned char third  = static_cast<unsigned char>(input[index + 2]);
                    const unsigned char fourth = static_cast<unsigned char>(input[index + 3]);
                    valid = is_utf8_continuation(second) && is_utf8_continuation(third) &&
                            is_utf8_continuation(fourth);

                    // Reject overlong values and code points above U+10FFFF.
                    if (first == 0xF0U && second < 0x90U) { valid = false; }
                    if (first == 0xF4U && second > 0x8FU) { valid = false; }
                }
            }

            if (!valid || sequenceLength == 0)
            {
                output.push_back('?');
                ++index;
                continue;
            }

            if (output.size() + sequenceLength > MAX_DISPLAY_TITLE_BYTES) { break; }
            output.append(input.substr(index, sequenceLength));
            index += sequenceLength;
        }

        // Trim spaces introduced by stripped control characters.
        while (!output.empty() && output.front() == ' ') { output.erase(output.begin()); }
        while (!output.empty() && output.back() == ' ') { output.pop_back(); }

        return output;
    }
} // namespace

//                      ---- Construction ----

TextTitleSelectState::TextTitleSelectState(data::User *user)
    : TitleSelectCommon()
    , m_user(user)
    , m_renderTarget(sdl::TextureManager::load(SECONDARY_TARGET, 1080, 555, SDL_TEXTUREACCESS_TARGET))
{
    TextTitleSelectState::refresh();
}

//                      ---- Public functions ----

void TextTitleSelectState::cache_title_label(uint64_t applicationID, std::string_view title)
{
    if (applicationID == 0) { return; }

    std::string safeTitle = sanitize_display_title(title);
    if (safeTitle.empty())
    {
        safeTitle = stringutil::get_formatted_string("Title ID %016llX",
                                                     static_cast<unsigned long long>(applicationID));
    }

    sm_titleLabels[applicationID] = std::move(safeTitle);
}

void TextTitleSelectState::update()
{
    const bool hasFocus = BaseState::has_focus();
    if (!hasFocus)
    {
        sm_controlGuide->update(false);
        return;
    }

    TextTitleSelectState::handle_navigation();

    const bool aPressed = input::button_pressed(HidNpadButton_A);
    const bool bPressed = input::button_pressed(HidNpadButton_B);
    const bool xPressed = input::button_pressed(HidNpadButton_X);
    const bool yPressed = input::button_pressed(HidNpadButton_Y);

    const int entryCount = static_cast<int>(m_displayOrder.size());
    if (aPressed && entryCount > 0) { TextTitleSelectState::create_backup_menu(); }
    else if (xPressed && entryCount > 0) { TextTitleSelectState::create_title_option_menu(); }
    else if (yPressed && entryCount > 0) { TextTitleSelectState::add_remove_favorite(); }
    else if (bPressed) { BaseState::deactivate(); }

    sm_controlGuide->update(hasFocus);
}

void TextTitleSelectState::render()
{
    const bool hasFocus = BaseState::has_focus();

    m_renderTarget->clear(colors::TRANSPARENT);

    const int entryCount = static_cast<int>(m_displayOrder.size());
    if (entryCount <= 0)
    {
        sdl::text::render(m_renderTarget,
                          LIST_X,
                          LIST_Y + 20,
                          FONT_SIZE,
                          sdl::text::NO_WRAP,
                          colors::WHITE,
                          "Nenhum jogo disponivel para este perfil.");
    }
    else
    {
        const int lastVisible = std::min(entryCount, m_firstVisible + VISIBLE_ROWS);
        for (int displayIndex = m_firstVisible; displayIndex < lastVisible; ++displayIndex)
        {
            const int sourceIndex  = m_displayOrder[displayIndex];
            const int visibleIndex = displayIndex - m_firstVisible;
            const int rowY         = LIST_Y + (visibleIndex * ROW_HEIGHT);
            const bool selected    = displayIndex == m_selected;

            if (selected && hasFocus)
            {
                sdl::render_rect_fill(m_renderTarget,
                                      LIST_X - 6,
                                      rowY - 2,
                                      LIST_WIDTH + 12,
                                      ROW_HEIGHT - 2,
                                      colors::DIALOG_DARK);
                sdl::render_rect_fill(m_renderTarget,
                                      LIST_X - 2,
                                      rowY + 5,
                                      5,
                                      ROW_HEIGHT - 15,
                                      colors::BLUE_GREEN);
            }

            const uint64_t applicationID = m_user ? m_user->get_application_id_at(sourceIndex) : 0;
            const std::string rowText = TextTitleSelectState::get_cached_title(applicationID);

            sdl::text::render(m_renderTarget,
                              LIST_X + 12,
                              rowY + 6,
                              FONT_SIZE,
                              sdl::text::NO_WRAP,
                              selected ? colors::BLUE_GREEN : colors::WHITE,
                              rowText);
        }

        const std::string position = stringutil::get_formatted_string("%d / %d", m_selected + 1, entryCount);
        const int positionWidth    = sdl::text::get_width(16, position);
        sdl::text::render(m_renderTarget,
                          1040 - positionWidth,
                          525,
                          16,
                          sdl::text::NO_WRAP,
                          colors::WHITE,
                          position);
    }

    sm_controlGuide->render(sdl::Texture::Null, hasFocus);
    m_renderTarget->render(sdl::Texture::Null, 201, 91);
}

void TextTitleSelectState::refresh()
{
    m_displayOrder.clear();
    m_selected     = 0;
    m_firstVisible = 0;

    if (!m_user) { return; }

    // SaveNX 0.2.16 keeps the proven 0.2.12/0.2.15 navigation path. No sorting,
    // favorites lookup or TitleInfo lookup happens while opening or drawing Games.
    // Cached labels are sanitized once before they can reach the text renderer.
    const int sourceCount = static_cast<int>(m_user->get_total_data_entries());
    m_displayOrder.reserve(sourceCount);
    for (int sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) { m_displayOrder.push_back(sourceIndex); }
}

//                      ---- Private functions ----

std::string TextTitleSelectState::get_cached_title(uint64_t applicationID)
{
    const auto found = sm_titleLabels.find(applicationID);
    if (found != sm_titleLabels.end() && !found->second.empty()) { return found->second; }

    return stringutil::get_formatted_string("Title ID %016llX", static_cast<unsigned long long>(applicationID));
}

void TextTitleSelectState::handle_navigation()
{
    const int entryCount = static_cast<int>(m_displayOrder.size());
    if (entryCount <= 0) { return; }

    const bool upPressed        = input::button_pressed(HidNpadButton_AnyUp);
    const bool downPressed      = input::button_pressed(HidNpadButton_AnyDown);
    const bool leftPressed      = input::button_pressed(HidNpadButton_AnyLeft);
    const bool rightPressed     = input::button_pressed(HidNpadButton_AnyRight);
    const bool lShoulderPressed = input::button_pressed(HidNpadButton_L);
    const bool rShoulderPressed = input::button_pressed(HidNpadButton_R);

    if (upPressed) { --m_selected; }
    else if (downPressed) { ++m_selected; }
    else if (leftPressed || lShoulderPressed) { m_selected -= VISIBLE_ROWS; }
    else if (rightPressed || rShoulderPressed) { m_selected += VISIBLE_ROWS; }

    m_selected = std::clamp(m_selected, 0, entryCount - 1);
    TextTitleSelectState::clamp_window();
}

void TextTitleSelectState::clamp_window() noexcept
{
    const int entryCount = static_cast<int>(m_displayOrder.size());
    if (entryCount <= 0)
    {
        m_selected     = 0;
        m_firstVisible = 0;
        return;
    }

    m_selected = std::clamp(m_selected, 0, entryCount - 1);

    if (m_selected < m_firstVisible) { m_firstVisible = m_selected; }
    else if (m_selected >= m_firstVisible + VISIBLE_ROWS) { m_firstVisible = m_selected - VISIBLE_ROWS + 1; }

    const int maxFirst = std::max(0, entryCount - VISIBLE_ROWS);
    m_firstVisible = std::clamp(m_firstVisible, 0, maxFirst);
}

int TextTitleSelectState::get_selected_source_index() const noexcept
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_displayOrder.size())) { return -1; }
    return m_displayOrder[m_selected];
}

void TextTitleSelectState::create_backup_menu()
{
    const int sourceIndex = TextTitleSelectState::get_selected_source_index();
    if (!m_user || sourceIndex < 0) { return; }

    const uint64_t applicationID   = m_user->get_application_id_at(sourceIndex);
    data::TitleInfo *titleInfo     = data::get_title_info_by_id(applicationID);
    const FsSaveDataInfo *saveInfo = m_user->get_save_info_at(sourceIndex);

    if (!titleInfo || !saveInfo) { return; }
    BackupMenuState::create_and_push(m_user, titleInfo, saveInfo);
}

void TextTitleSelectState::create_title_option_menu()
{
    const int sourceIndex = TextTitleSelectState::get_selected_source_index();
    if (!m_user || sourceIndex < 0) { return; }

    const uint64_t applicationID   = m_user->get_application_id_at(sourceIndex);
    data::TitleInfo *titleInfo     = data::get_title_info_by_id(applicationID);
    const FsSaveDataInfo *saveInfo = m_user->get_save_info_at(sourceIndex);

    if (!titleInfo || !saveInfo) { return; }
    TitleOptionState::create_and_push(m_user, titleInfo, saveInfo, this);
}

void TextTitleSelectState::add_remove_favorite()
{
    const int sourceIndex = TextTitleSelectState::get_selected_source_index();
    if (!m_user || sourceIndex < 0) { return; }

    const uint64_t applicationID = m_user->get_application_id_at(sourceIndex);
    if (applicationID == 0) { return; }

    config::add_remove_favorite(applicationID);
}
