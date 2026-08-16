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

    std::string ascii_fold(std::string_view value)
    {
        std::string folded{value};
        for (char &character : folded)
        {
            if (character >= 'A' && character <= 'Z') { character = static_cast<char>(character + ('a' - 'A')); }
        }
        return folded;
    }

    const char *title_for_application(uint64_t applicationID)
    {
        data::TitleInfo *titleInfo = data::get_title_info_by_id(applicationID);
        if (!titleInfo) { return "Titulo sem identificacao"; }
        const char *title = titleInfo->get_title();
        return title && title[0] ? title : "Titulo sem identificacao";
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

    const int entryCount = m_user ? static_cast<int>(m_user->get_total_data_entries()) : 0;
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

    const int entryCount = m_user ? static_cast<int>(m_user->get_total_data_entries()) : 0;
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
        for (int index = m_firstVisible; index < lastVisible; ++index)
        {
            const int visibleIndex = index - m_firstVisible;
            const int rowY         = LIST_Y + (visibleIndex * ROW_HEIGHT);
            const bool selected    = index == m_selected;

            if (selected && hasFocus)
            {
                sdl::render_rect_fill(m_renderTarget, LIST_X - 6, rowY - 2, LIST_WIDTH + 12, ROW_HEIGHT - 2, colors::DIALOG_DARK);
                sdl::render_rect_fill(m_renderTarget, LIST_X - 2, rowY + 5, 5, ROW_HEIGHT - 15, colors::BLUE_GREEN);
            }

            const uint64_t applicationID = m_user->get_application_id_at(index);
            const char *title             = title_for_application(applicationID);
            std::string rowText{};
            if (config::is_favorite(applicationID)) { rowText = "* "; }
            rowText += title;

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
    if (!m_user)
    {
        m_selected = 0;
        m_firstVisible = 0;
        return;
    }

    uint64_t selectedApplicationID{};
    const int beforeCount = static_cast<int>(m_user->get_total_data_entries());
    if (beforeCount > 0 && m_selected >= 0 && m_selected < beforeCount)
    {
        selectedApplicationID = m_user->get_application_id_at(m_selected);
    }

    TextTitleSelectState::sort_entries_alphabetically();

    const int entryCount = static_cast<int>(m_user->get_total_data_entries());
    if (entryCount <= 0)
    {
        m_selected = 0;
        m_firstVisible = 0;
        return;
    }

    if (selectedApplicationID != 0)
    {
        for (int index = 0; index < entryCount; ++index)
        {
            if (m_user->get_application_id_at(index) == selectedApplicationID)
            {
                m_selected = index;
                break;
            }
        }
    }

    TextTitleSelectState::clamp_window();
}

//                      ---- Private functions ----

void TextTitleSelectState::handle_navigation()
{
    if (!m_user) { return; }

    const int entryCount = static_cast<int>(m_user->get_total_data_entries());
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
    if (!m_user)
    {
        m_selected = 0;
        m_firstVisible = 0;
        return;
    }

    const int entryCount = static_cast<int>(m_user->get_total_data_entries());
    if (entryCount <= 0)
    {
        m_selected = 0;
        m_firstVisible = 0;
        return;
    }

    m_selected = std::clamp(m_selected, 0, entryCount - 1);

    if (m_selected < m_firstVisible) { m_firstVisible = m_selected; }
    else if (m_selected >= m_firstVisible + VISIBLE_ROWS) { m_firstVisible = m_selected - VISIBLE_ROWS + 1; }

    const int maxFirst = std::max(0, entryCount - VISIBLE_ROWS);
    m_firstVisible = std::clamp(m_firstVisible, 0, maxFirst);
}

void TextTitleSelectState::sort_entries_alphabetically()
{
    if (!m_user) { return; }

    auto &entries = m_user->get_user_save_info_list();
    std::stable_sort(entries.begin(), entries.end(), [](const auto &entryA, const auto &entryB) {
        const uint64_t applicationIDA = entryA.first;
        const uint64_t applicationIDB = entryB.first;

        const std::string titleA = ascii_fold(title_for_application(applicationIDA));
        const std::string titleB = ascii_fold(title_for_application(applicationIDB));

        if (titleA != titleB) { return titleA < titleB; }
        return applicationIDA < applicationIDB;
    });
}

void TextTitleSelectState::create_backup_menu()
{
    const uint64_t applicationID   = m_user->get_application_id_at(m_selected);
    data::TitleInfo *titleInfo     = data::get_title_info_by_id(applicationID);
    const FsSaveDataInfo *saveInfo = m_user->get_save_info_at(m_selected);

    if (!titleInfo || !saveInfo) { return; }
    BackupMenuState::create_and_push(m_user, titleInfo, saveInfo);
}

void TextTitleSelectState::create_title_option_menu()
{
    const uint64_t applicationID   = m_user->get_application_id_at(m_selected);
    data::TitleInfo *titleInfo     = data::get_title_info_by_id(applicationID);
    const FsSaveDataInfo *saveInfo = m_user->get_save_info_at(m_selected);

    if (!titleInfo || !saveInfo) { return; }
    TitleOptionState::create_and_push(m_user, titleInfo, saveInfo, this);
}

void TextTitleSelectState::add_remove_favorite()
{
    const uint64_t applicationID = m_user->get_application_id_at(m_selected);
    if (applicationID == 0) { return; }

    config::add_remove_favorite(applicationID);
    TextTitleSelectState::refresh();
}
