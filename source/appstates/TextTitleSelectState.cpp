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

    if (title.empty())
    {
        sm_titleLabels[applicationID] = stringutil::get_formatted_string("Title ID %016llX",
                                                                         static_cast<unsigned long long>(applicationID));
        return;
    }

    sm_titleLabels[applicationID] = std::string{title};
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
            std::string rowText = TextTitleSelectState::get_cached_title(applicationID);
            if (config::is_favorite(applicationID)) { rowText = "* " + rowText; }

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

    const int sourceCount = static_cast<int>(m_user->get_total_data_entries());
    m_displayOrder.reserve(sourceCount);
    for (int sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) { m_displayOrder.push_back(sourceIndex); }

    // SaveNX 0.2.13: sort only the detached visual index list. Names are already
    // copied into sm_titleLabels by MainMenu while it owns safe TitleInfo pointers.
    // No TitleInfo lookup occurs here or during render/navigation.
    std::stable_sort(m_displayOrder.begin(), m_displayOrder.end(), [&](int sourceIndexA, int sourceIndexB) {
        const uint64_t applicationIDA = m_user->get_application_id_at(sourceIndexA);
        const uint64_t applicationIDB = m_user->get_application_id_at(sourceIndexB);
        const std::string keyA = TextTitleSelectState::make_sort_key(TextTitleSelectState::get_cached_title(applicationIDA));
        const std::string keyB = TextTitleSelectState::make_sort_key(TextTitleSelectState::get_cached_title(applicationIDB));

        if (keyA != keyB) { return keyA < keyB; }
        return applicationIDA < applicationIDB;
    });
}

//                      ---- Private functions ----

std::string TextTitleSelectState::get_cached_title(uint64_t applicationID)
{
    const auto found = sm_titleLabels.find(applicationID);
    if (found != sm_titleLabels.end() && !found->second.empty()) { return found->second; }

    return stringutil::get_formatted_string("Title ID %016llX", static_cast<unsigned long long>(applicationID));
}

std::string TextTitleSelectState::make_sort_key(std::string_view title)
{
    std::string key{};
    key.reserve(title.size());

    for (size_t index = 0; index < title.size(); ++index)
    {
        const unsigned char current = static_cast<unsigned char>(title[index]);

        if (current >= 'A' && current <= 'Z')
        {
            key.push_back(static_cast<char>(current + ('a' - 'A')));
            continue;
        }

        // Safe, small normalization for common Latin-1 letters encoded as UTF-8.
        // Unknown multi-byte sequences are kept byte-for-byte; there is no decoder
        // loop here, so malformed text can never stall sorting.
        if (current == 0xC3 && index + 1 < title.size())
        {
            const unsigned char next = static_cast<unsigned char>(title[index + 1]);
            char replacement{};
            switch (next)
            {
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
                case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: replacement = 'a'; break;
                case 0x87: case 0xA7: replacement = 'c'; break;
                case 0x88: case 0x89: case 0x8A: case 0x8B:
                case 0xA8: case 0xA9: case 0xAA: case 0xAB: replacement = 'e'; break;
                case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                case 0xAC: case 0xAD: case 0xAE: case 0xAF: replacement = 'i'; break;
                case 0x91: case 0xB1: replacement = 'n'; break;
                case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
                case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: replacement = 'o'; break;
                case 0x99: case 0x9A: case 0x9B: case 0x9C:
                case 0xB9: case 0xBA: case 0xBB: case 0xBC: replacement = 'u'; break;
                default: break;
            }

            if (replacement != 0)
            {
                key.push_back(replacement);
                ++index;
                continue;
            }
        }

        key.push_back(static_cast<char>(current));
    }

    return key;
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
