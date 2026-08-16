#include "appstates/MainMenuState.hpp"

#include "StateManager.hpp"
#include "appstates/ConfirmState.hpp"
#include "appstates/ExtrasMenuState.hpp"
#include "appstates/SettingsState.hpp"
#include "appstates/TextTitleSelectState.hpp"
#include "appstates/TitleSelectCommon.hpp"
#include "appstates/TitleSelectState.hpp"
#include "config/config.hpp"
#include "graphics/gfxutil.hpp"
#include "graphics/screen.hpp"
#include "input.hpp"
#include "remote/remote.hpp"
#include "sdl.hpp"
#include "strings/strings.hpp"
#include "stringutil.hpp"
#include "tasks/mainmenu.hpp"
#include "ui/PopMessageManager.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace
{
    constexpr sdl::Color COLOR_BACKGROUND  = {0x07111FFF};
    constexpr sdl::Color COLOR_HEADER      = {0x0A1728FF};
    constexpr sdl::Color COLOR_PANEL       = {0x101F32FF};
    constexpr sdl::Color COLOR_PANEL_ALT   = {0x13263CFF};
    constexpr sdl::Color COLOR_HERO        = {0x0E3445FF};
    constexpr sdl::Color COLOR_HERO_ACCENT = {0x124D6BFF};
    constexpr sdl::Color COLOR_PRIMARY     = {0x1684F8FF};
    constexpr sdl::Color COLOR_FOCUS       = {0x58D7FFFF};
    constexpr sdl::Color COLOR_SUCCESS     = {0x35D89AFF};
    constexpr sdl::Color COLOR_WARNING     = {0xFFB84DFF};
    constexpr sdl::Color COLOR_TEXT        = {0xF5F8FFFF};
    constexpr sdl::Color COLOR_TEXT_MUTED  = {0x9CAEC4FF};
    constexpr sdl::Color COLOR_BORDER      = {0x243A52FF};

    constexpr int SCREEN_MARGIN = 32;

    void render_panel(int x,
                      int y,
                      int width,
                      int height,
                      int radius,
                      sdl::Color fill,
                      sdl::Color border,
                      int borderWidth = 1)
    {
        auto &target = sdl::Texture::Null;
        if (borderWidth > 0)
        {
            gfxutil::render_rounded_rect_fill(target, x, y, width, height, radius, border);
            gfxutil::render_rounded_rect_fill(target,
                                              x + borderWidth,
                                              y + borderWidth,
                                              width - (borderWidth * 2),
                                              height - (borderWidth * 2),
                                              std::max(0, radius - borderWidth),
                                              fill);
        }
        else { gfxutil::render_rounded_rect_fill(target, x, y, width, height, radius, fill); }
    }

    void render_thick_line(int xA, int yA, int xB, int yB, int thickness, sdl::Color color)
    {
        for (int offset = -(thickness / 2); offset <= thickness / 2; ++offset)
        {
            sdl::render_line(sdl::Texture::Null, xA, yA + offset, xB, yB + offset, color);
        }
    }

    void render_check(int x, int y, sdl::Color color)
    {
        render_thick_line(x, y + 7, x + 8, y + 15, 3, color);
        render_thick_line(x + 8, y + 15, x + 24, y - 3, 3, color);
    }

    std::string fit_text(std::string_view text, int fontSize, int maxWidth)
    {
        if (sdl::text::get_width(fontSize, text) <= maxWidth) { return std::string{text}; }

        std::string fitted{text};
        constexpr std::string_view ellipsis = "…";
        while (!fitted.empty() && sdl::text::get_width(fontSize, fitted + std::string{ellipsis}) > maxWidth)
        {
            size_t eraseFrom = fitted.size() - 1;
            while (eraseFrom > 0 && (static_cast<unsigned char>(fitted[eraseFrom]) & 0xC0) == 0x80) { --eraseFrom; }
            fitted.erase(eraseFrom);
        }
        fitted += ellipsis;
        return fitted;
    }

    size_t count_protectable_saves(const data::UserList &users)
    {
        size_t total{};
        for (const data::User *user : users)
        {
            if (user->get_account_save_type() == FsSaveDataType_System) { continue; }
            total += user->get_total_data_entries();
        }
        return total;
    }

    size_t prepare_user_save_candidates(data::User *user)
    {
        if (!user || user->get_account_save_type() != FsSaveDataType_Account) { return 0; }

        // Dashboard-first architecture: candidates are prepared only after the user
        // explicitly opens Games or starts Protect All. No global save-data scan occurs.
        user->clear_data_entries();

        data::TitleInfoList titleList;
        data::get_title_info_list(titleList);
        const AccountUid accountID = user->get_account_id();

        for (data::TitleInfo *titleInfo : titleList)
        {
            if (!titleInfo || !titleInfo->has_save_data_type(FsSaveDataType_Account)) { continue; }

            const uint64_t applicationID = titleInfo->get_application_id();
            if (applicationID == 0 || config::is_blacklisted(applicationID)) { continue; }

            // SaveNX 0.2.13 copies the display label right here, while titleInfo is
            // already available from the installed-title list. Games never resolves
            // TitleInfo during refresh/render/navigation; it consumes only this string.
            const char *title = titleInfo->get_title();
            TextTitleSelectState::cache_title_label(applicationID, title ? std::string_view{title} : std::string_view{});

            FsSaveDataInfo saveInfo{};
            saveInfo.save_data_space_id  = FsSaveDataSpaceId_User;
            saveInfo.save_data_type      = FsSaveDataType_Account;
            saveInfo.save_data_rank      = FsSaveDataRank_Primary;
            saveInfo.application_id      = applicationID;
            saveInfo.uid                 = accountID;
            saveInfo.system_save_data_id = 0;
            saveInfo.save_data_index     = 0;

            PdmPlayStatistics playStats{};
            user->add_data(applicationID, saveInfo, playStats);
        }

        return user->get_total_data_entries();
    }

    void prepare_all_user_save_candidates(const data::UserList &users)
    {
        for (data::User *user : users)
        {
            if (user && user->get_total_data_entries() == 0) { prepare_user_save_candidates(user); }
        }
    }
} // namespace

//                      ---- Construction ----

MainMenuState::MainMenuState()
    : m_dataStruct(std::make_shared<MainMenuState::DataStruct>())
{
    MainMenuState::initialize_settings_extras();
    MainMenuState::initialize_users();
    MainMenuState::initialize_view_states();
    MainMenuState::initialize_data_struct();
}

//                      ---- Public functions ----

void MainMenuState::update()
{
    MainMenuState::update_navigation();

    const bool previousProfile = input::button_pressed(HidNpadButton_L) || input::button_pressed(HidNpadButton_ZL);
    const bool nextProfile     = input::button_pressed(HidNpadButton_R) || input::button_pressed(HidNpadButton_ZR);
    if (previousProfile) { MainMenuState::cycle_active_user(-1); }
    else if (nextProfile) { MainMenuState::cycle_active_user(1); }

    const bool aPressed = input::button_pressed(HidNpadButton_A);
    const bool xPressed = input::button_pressed(HidNpadButton_X);
    const bool yPressed = input::button_pressed(HidNpadButton_Y);

    if (aPressed) { MainMenuState::activate_selected_action(); }
    else if (xPressed) { MainMenuState::open_persistent_state(sm_extrasState); }
    else if (yPressed) { MainMenuState::backup_all_for_all(); }
}

void MainMenuState::sub_update() {}

void MainMenuState::render()
{
    if (!BaseState::has_focus()) { return; }
    MainMenuState::render_dashboard();
}

void MainMenuState::initialize_view_states()
{
    const bool jksmMode = config::get_by_key(config::keys::JKSM_TEXT_MODE);

    sm_states.clear();
    for (data::User *user : sm_users)
    {
        std::shared_ptr<BaseState> state{};
        if (jksmMode) { state = TextTitleSelectState::create(user); }
        else { state = TitleSelectState::create(user); }
        sm_states.push_back(state);
    }
    sm_states.push_back(sm_settingsState);
    sm_states.push_back(sm_extrasState);
}

void MainMenuState::refresh_view_states()
{
    for (int i = 0; i < sm_userCount; i++)
    {
        TitleSelectCommon *target = static_cast<TitleSelectCommon *>(sm_states.at(i).get());
        target->refresh();
    }
}

//                      ---- Private functions ----

void MainMenuState::initialize_settings_extras()
{
    if (!sm_settingsState || !sm_extrasState)
    {
        sm_settingsState = SettingsState::create();
        sm_extrasState   = ExtrasMenuState::create();
    }
}

void MainMenuState::initialize_users()
{
    sm_users.clear();
    data::get_users(sm_users);
    sm_userCount      = static_cast<int>(sm_users.size());
    m_activeUserIndex = 0;
}

void MainMenuState::initialize_data_struct()
{
    m_dataStruct->userList      = sm_users;
    m_dataStruct->spawningState = this;
}

void MainMenuState::activate_selected_action()
{
    switch (m_selectedAction)
    {
        case DashboardAction::ProtectAll: MainMenuState::backup_all_for_all(); break;
        case DashboardAction::Games:      MainMenuState::open_active_user(); break;
        case DashboardAction::History:
            ui::PopMessageManager::push_message(ui::PopMessageManager::DEFAULT_TICKS,
                                                "O histórico inteligente será ativado após a primeira proteção da versão 0.2.0.");
            break;
        case DashboardAction::Settings: MainMenuState::open_persistent_state(sm_settingsState); break;
    }
}

void MainMenuState::open_active_user()
{
    if (sm_users.empty() || m_activeUserIndex < 0 || m_activeUserIndex >= sm_userCount) { return; }

    data::User *user = sm_users[m_activeUserIndex];
    if (user->get_total_data_entries() == 0)
    {
        const size_t candidates = prepare_user_save_candidates(user);
        if (candidates == 0)
        {
            std::string message = stringutil::get_formatted_string("Nenhum jogo com save por perfil encontrado para %s.",
                                                                   user->get_nickname());
            ui::PopMessageManager::push_message(ui::PopMessageManager::DEFAULT_TICKS, message);
            return;
        }
    }

    auto &target = sm_states[m_activeUserIndex];
    TitleSelectCommon *titleState = static_cast<TitleSelectCommon *>(target.get());
    titleState->refresh();
    target->reactivate();
    StateManager::push_state(target);
}

void MainMenuState::open_persistent_state(std::shared_ptr<BaseState> &state)
{
    if (!state) { return; }
    state->reactivate();
    StateManager::push_state(state);
}

void MainMenuState::cycle_active_user(int direction) noexcept
{
    if (sm_userCount <= 1) { return; }
    m_activeUserIndex = (m_activeUserIndex + direction + sm_userCount) % sm_userCount;
}

void MainMenuState::update_navigation() noexcept
{
    const bool upPressed    = input::button_pressed(HidNpadButton_AnyUp);
    const bool downPressed  = input::button_pressed(HidNpadButton_AnyDown);
    const bool leftPressed  = input::button_pressed(HidNpadButton_AnyLeft);
    const bool rightPressed = input::button_pressed(HidNpadButton_AnyRight);

    if (leftPressed || rightPressed)
    {
        constexpr int ACTION_COUNT = 4;
        int selected = static_cast<int>(m_selectedAction);
        selected = (selected + (leftPressed ? -1 : 1) + ACTION_COUNT) % ACTION_COUNT;
        m_selectedAction = static_cast<DashboardAction>(selected);
    }
    else if (upPressed) { m_selectedAction = DashboardAction::ProtectAll; }
    else if (downPressed && m_selectedAction == DashboardAction::ProtectAll) { m_selectedAction = DashboardAction::Games; }
}

void MainMenuState::render_dashboard()
{
    sdl::render_rect_fill(sdl::Texture::Null,
                          0,
                          0,
                          graphics::SCREEN_WIDTH,
                          graphics::SCREEN_HEIGHT,
                          COLOR_BACKGROUND);
    MainMenuState::render_header();
    MainMenuState::render_protection_hero();
    MainMenuState::render_game_cards();
    MainMenuState::render_history();
    MainMenuState::render_navigation();
}

void MainMenuState::render_header()
{
    sdl::render_rect_fill(sdl::Texture::Null, 0, 0, graphics::SCREEN_WIDTH, 78, COLOR_HEADER);
    sdl::render_line(sdl::Texture::Null, 0, 77, graphics::SCREEN_WIDTH, 77, COLOR_BORDER);

    gfxutil::render_circle_fill(sdl::Texture::Null, 47, 38, 21, COLOR_PRIMARY);
    render_thick_line(37, 31, 47, 26, 2, COLOR_TEXT);
    render_thick_line(47, 26, 57, 31, 2, COLOR_TEXT);
    render_thick_line(37, 31, 39, 45, 2, COLOR_TEXT);
    render_thick_line(57, 31, 55, 45, 2, COLOR_TEXT);
    render_thick_line(39, 45, 47, 51, 2, COLOR_TEXT);
    render_thick_line(47, 51, 55, 45, 2, COLOR_TEXT);

    sdl::text::render(sdl::Texture::Null, 79, 15, 29, sdl::text::NO_WRAP, COLOR_TEXT, "SaveNX");
    sdl::text::render(sdl::Texture::Null,
                      80,
                      47,
                      14,
                      sdl::text::NO_WRAP,
                      COLOR_TEXT_MUTED,
                      "SEUS SAVES, SEMPRE PROTEGIDOS");

    if (sm_users.empty()) { return; }
    data::User *activeUser = sm_users[m_activeUserIndex];
    const std::string nickname = fit_text(activeUser->get_nickname(), 18, 145);

    render_panel(1004, 17, 244, 46, 23, COLOR_PANEL, COLOR_BORDER);
    sdl::SharedTexture userIcon = activeUser->get_icon();
    if (userIcon) { userIcon->render_stretched(sdl::Texture::Null, 1010, 23, 34, 34); }
    else { gfxutil::render_circle_fill(sdl::Texture::Null, 1027, 40, 16, COLOR_PRIMARY); }
    sdl::text::render(sdl::Texture::Null, 1054, 23, 18, sdl::text::NO_WRAP, COLOR_TEXT, nickname);

    const std::string profileHint = sm_userCount > 1 ? "L / R  trocar perfil" : "Perfil ativo";
    sdl::text::render(sdl::Texture::Null, 1054, 45, 12, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, profileHint);
}

void MainMenuState::render_protection_hero()
{
    constexpr int heroX = SCREEN_MARGIN;
    constexpr int heroY = 92;
    constexpr int heroW = graphics::SCREEN_WIDTH - (SCREEN_MARGIN * 2);
    constexpr int heroH = 184;
    render_panel(heroX, heroY, heroW, heroH, 18, COLOR_HERO, COLOR_HERO_ACCENT, 2);

    gfxutil::render_circle_fill(sdl::Texture::Null, 78, 135, 29, COLOR_SUCCESS);
    render_check(66, 128, COLOR_BACKGROUND);

    const size_t totalSaves = count_protectable_saves(sm_users);
    const bool driveConnected = remote::get_remote_storage() != nullptr;
    const std::string title = driveConnected ? "Proteção na nuvem pronta" : "Seus saves, sob seu controle";
    const std::string subtitle = driveConnected
                                     ? "Google Drive conectado. Seus backups podem ser protegidos na nuvem."
                                     : "Proteção local agora; conecte o Google Drive quando quiser sincronizar.";

    sdl::text::render(sdl::Texture::Null, 126, 112, 24, sdl::text::NO_WRAP, COLOR_TEXT, title);
    sdl::text::render(sdl::Texture::Null, 126, 145, 15, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, subtitle);

    const std::string saveCount = totalSaves == 0 ? "Jogos ainda não carregados"
                                                   : stringutil::get_formatted_string("%zu candidatos preparados", totalSaves);
    sdl::text::render(sdl::Texture::Null, 126, 177, 15, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, saveCount);

    const bool focused = m_selectedAction == DashboardAction::ProtectAll;
    render_panel(1018,
                 122,
                 198,
                 54,
                 12,
                 focused ? COLOR_PRIMARY : COLOR_PANEL_ALT,
                 focused ? COLOR_FOCUS : COLOR_BORDER,
                 focused ? 2 : 1);
    sdl::text::render(sdl::Texture::Null,
                      1051,
                      139,
                      17,
                      sdl::text::NO_WRAP,
                      COLOR_TEXT,
                      "Proteger Todos");

    const std::string driveState = driveConnected ? "Drive conectado" : "Drive aguardando autorização";
    const sdl::Color driveColor = driveConnected ? COLOR_SUCCESS : COLOR_WARNING;
    sdl::text::render(sdl::Texture::Null, 1019, 196, 14, sdl::text::NO_WRAP, driveColor, driveState);
}

void MainMenuState::render_game_cards()
{
    render_panel(32, 294, 756, 212, 18, COLOR_PANEL, COLOR_BORDER);
    sdl::text::render(sdl::Texture::Null, 58, 314, 20, sdl::text::NO_WRAP, COLOR_TEXT, "Jogos protegidos");

    if (sm_users.empty()) { return; }
    data::User *activeUser = sm_users[m_activeUserIndex];
    const size_t entries = activeUser->get_total_data_entries();

    const std::string primary = entries == 0 ? "Abra Jogos para preparar a lista deste perfil"
                                              : stringutil::get_formatted_string("%zu candidatos de save preparados", entries);
    const std::string secondary = entries == 0 ? "Nenhuma enumeração de saves ocorre durante a inicialização."
                                                : "A existência real de cada save é validada quando o backup é iniciado.";

    sdl::text::render(sdl::Texture::Null, 58, 361, 19, sdl::text::NO_WRAP, COLOR_TEXT, primary);
    sdl::text::render(sdl::Texture::Null, 58, 395, 14, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, secondary);
}

void MainMenuState::render_history()
{
    render_panel(812, 294, 436, 212, 18, COLOR_PANEL, COLOR_BORDER);
    sdl::text::render(sdl::Texture::Null, 838, 314, 20, sdl::text::NO_WRAP, COLOR_TEXT, "Histórico");
    sdl::text::render(sdl::Texture::Null,
                      838,
                      361,
                      17,
                      sdl::text::NO_WRAP,
                      COLOR_TEXT_MUTED,
                      "Últimas proteções aparecerão aqui.");
}

void MainMenuState::render_navigation()
{
    constexpr int navY = 548;
    constexpr int navH = 78;
    render_panel(32, navY, 1216, navH, 18, COLOR_HEADER, COLOR_BORDER);

    constexpr std::array<std::string_view, 4> labels = {"Início", "Jogos", "Histórico", "Configurações"};
    constexpr std::array<int, 4> xs = {70, 334, 615, 915};

    for (int i = 0; i < static_cast<int>(labels.size()); ++i)
    {
        const bool selected = static_cast<int>(m_selectedAction) == i;
        const sdl::Color color = selected ? COLOR_FOCUS : COLOR_TEXT_MUTED;
        if (selected) { sdl::render_rect_fill(sdl::Texture::Null, xs[i] - 18, navY + 59, 160, 3, COLOR_PRIMARY); }
        sdl::text::render(sdl::Texture::Null, xs[i], navY + 23, 18, sdl::text::NO_WRAP, color, labels[i]);
    }

    sdl::text::render(sdl::Texture::Null,
                      68,
                      642,
                      13,
                      sdl::text::NO_WRAP,
                      COLOR_TEXT_MUTED,
                      "← → Menu    L/R Perfil    A Abrir    Y Proteger tudo    X Extras");
}

void MainMenuState::backup_all_for_all()
{
    prepare_all_user_save_candidates(sm_users);
    tasks::backup_all_for_all_users(m_dataStruct.get());
}
