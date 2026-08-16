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

        // SaveNX 0.2.8 keeps the proven 0.2.7 dashboard-first startup intact. Game/save
        // candidates are prepared only after the user explicitly opens Games or starts
        // Protect All. This path never enumerates the global Switch save-data table.
        user->clear_data_entries();

        data::TitleInfoList titleList;
        data::get_title_info_list(titleList);
        const AccountUid accountID = user->get_account_id();

        for (data::TitleInfo *titleInfo : titleList)
        {
            if (!titleInfo || !titleInfo->has_save_data_type(FsSaveDataType_Account)) { continue; }

            const uint64_t applicationID = titleInfo->get_application_id();
            if (applicationID == 0 || config::is_blacklisted(applicationID)) { continue; }

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
    // Legacy states still expect SaveNX::render_base behind them. When the dashboard loses focus, leave that base untouched
    // and let the active legacy state render itself on top.
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

    // The bottom dashboard navigation is visually horizontal, so Left/Right must
    // always walk through Inicio -> Jogos -> Historico -> Configuracoes (and wrap).
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
    const std::string subtitle = totalSaves == 0
                                     ? "A lista de jogos é carregada somente quando você abre Jogos ou inicia uma proteção."
                                     : std::to_string(totalSaves) +
                                           (totalSaves == 1 ? " jogo com suporte a save preparado. "
                                                            : " jogos com suporte a save preparados. ") +
                                           "O save real é validado ao proteger.";

    sdl::text::render(sdl::Texture::Null, 126, 111, 30, sdl::text::NO_WRAP, COLOR_TEXT, title);
    sdl::text::render(sdl::Texture::Null, 127, 151, 17, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, subtitle);

    const bool buttonFocused = m_selectedAction == DashboardAction::ProtectAll;
    const sdl::Color buttonBorder = buttonFocused ? COLOR_FOCUS : COLOR_PRIMARY;
    render_panel(61, 203, 291, 51, 13, COLOR_PRIMARY, buttonBorder, buttonFocused ? 3 : 1);
    gfxutil::render_circle_fill(sdl::Texture::Null, 88, 229, 11, COLOR_TEXT);
    render_check(81, 223, COLOR_PRIMARY);
    sdl::text::render(sdl::Texture::Null,
                      108,
                      217,
                      18,
                      sdl::text::NO_WRAP,
                      COLOR_TEXT,
                      "PROTEGER TODOS AGORA");

    render_panel(847, 108, 377, 151, 15, COLOR_PANEL, COLOR_BORDER);
    sdl::text::render(sdl::Texture::Null, 871, 121, 15, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, "STATUS DA PROTEÇÃO");

    gfxutil::render_circle_fill(sdl::Texture::Null, 882, 159, 8, driveConnected ? COLOR_SUCCESS : COLOR_WARNING);
    sdl::text::render(sdl::Texture::Null, 902, 148, 17, sdl::text::NO_WRAP, COLOR_TEXT, "Google Drive");
    const std::string driveStatus = driveConnected ? "Conectado" : "Aguardando autorização";
    const sdl::Color driveStatusColor = driveConnected ? COLOR_SUCCESS : COLOR_WARNING;
    sdl::text::render(sdl::Texture::Null, 1084, 149, 15, sdl::text::NO_WRAP, driveStatusColor, driveStatus);

    gfxutil::render_circle_fill(sdl::Texture::Null, 882, 194, 8, COLOR_SUCCESS);
    sdl::text::render(sdl::Texture::Null, 902, 183, 17, sdl::text::NO_WRAP, COLOR_TEXT, "Cartão SD");
    sdl::text::render(sdl::Texture::Null, 1146, 184, 15, sdl::text::NO_WRAP, COLOR_SUCCESS, "Pronto");

    gfxutil::render_circle_fill(sdl::Texture::Null, 882, 229, 8, COLOR_PRIMARY);
    sdl::text::render(sdl::Texture::Null, 902, 218, 17, sdl::text::NO_WRAP, COLOR_TEXT, "Verificação");
    sdl::text::render(sdl::Texture::Null, 1082, 219, 15, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, "Automática");
}

void MainMenuState::render_game_cards()
{
    constexpr int headingY = 294;
    constexpr int cardY = 326;
    constexpr int cardH = 114;
    constexpr int cardGap = 15;
    constexpr int cardW = (graphics::SCREEN_WIDTH - (SCREEN_MARGIN * 2) - (cardGap * 2)) / 3;

    sdl::text::render(sdl::Texture::Null, SCREEN_MARGIN, headingY, 21, sdl::text::NO_WRAP, COLOR_TEXT, "Jogos deste perfil");
    if (sm_users.empty()) { return; }

    data::User *activeUser = sm_users[m_activeUserIndex];
    const size_t saveCount = activeUser->get_total_data_entries();
    const std::string countText = saveCount == 0 ? "sob demanda" : std::to_string(saveCount) + (saveCount == 1 ? " jogo" : " jogos");
    const int countX = graphics::SCREEN_WIDTH - SCREEN_MARGIN - sdl::text::get_width(14, countText);
    sdl::text::render(sdl::Texture::Null, countX, 299, 14, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, countText);

    if (saveCount == 0)
    {
        render_panel(SCREEN_MARGIN, cardY, graphics::SCREEN_WIDTH - (SCREEN_MARGIN * 2), cardH, 14, COLOR_PANEL, COLOR_BORDER);
        gfxutil::render_circle_fill(sdl::Texture::Null, 75, cardY + (cardH / 2), 22, COLOR_PANEL_ALT);
        sdl::text::render(sdl::Texture::Null, 112, cardY + 29, 20, sdl::text::NO_WRAP, COLOR_TEXT, "Jogos ainda não carregados");
        sdl::text::render(sdl::Texture::Null,
                          112,
                          cardY + 60,
                          15,
                          sdl::text::NO_WRAP,
                          COLOR_TEXT_MUTED,
                          "Selecione Jogos e pressione A para preparar a lista deste perfil.");
        return;
    }

    const int visibleCards = std::min(3, static_cast<int>(saveCount));
    for (int i = 0; i < visibleCards; ++i)
    {
        const int cardX = SCREEN_MARGIN + (i * (cardW + cardGap));
        render_panel(cardX, cardY, cardW, cardH, 14, COLOR_PANEL, COLOR_BORDER);

        const uint64_t applicationID = activeUser->get_application_id_at(i);
        data::TitleInfo *titleInfo = data::get_title_info_by_id(applicationID);
        sdl::SharedTexture icon = titleInfo ? titleInfo->get_icon() : nullptr;
        if (icon) { icon->render_stretched(sdl::Texture::Null, cardX + 18, cardY + 24, 66, 66); }
        else
        {
            render_panel(cardX + 18, cardY + 24, 66, 66, 12, COLOR_PANEL_ALT, COLOR_BORDER);
            sdl::text::render(sdl::Texture::Null, cardX + 43, cardY + 43, 22, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, "S");
        }

        const std::string title = fit_text(titleInfo ? titleInfo->get_title() : "Título sem identificação", 19, cardW - 122);
        sdl::text::render(sdl::Texture::Null, cardX + 101, cardY + 23, 19, sdl::text::NO_WRAP, COLOR_TEXT, title);
        gfxutil::render_circle_fill(sdl::Texture::Null, cardX + 108, cardY + 65, 5, COLOR_PRIMARY);
        sdl::text::render(sdl::Texture::Null,
                          cardX + 121,
                          cardY + 55,
                          15,
                          sdl::text::NO_WRAP,
                          COLOR_TEXT_MUTED,
                          "Save validado ao abrir");
        sdl::text::render(sdl::Texture::Null,
                          cardX + 101,
                          cardY + 82,
                          13,
                          sdl::text::NO_WRAP,
                          COLOR_PRIMARY,
                          "Acesse pela aba Jogos");
    }
}

void MainMenuState::render_history()
{
    sdl::text::render(sdl::Texture::Null, SCREEN_MARGIN, 459, 21, sdl::text::NO_WRAP, COLOR_TEXT, "Proteção recente");
    render_panel(SCREEN_MARGIN, 491, graphics::SCREEN_WIDTH - (SCREEN_MARGIN * 2), 105, 14, COLOR_PANEL, COLOR_BORDER);

    gfxutil::render_circle_fill(sdl::Texture::Null, 72, 543, 18, COLOR_PANEL_ALT);
    gfxutil::render_circle_fill(sdl::Texture::Null, 72, 543, 6, COLOR_PRIMARY);
    sdl::render_line(sdl::Texture::Null, 104, 520, 104, 566, COLOR_BORDER);
    sdl::text::render(sdl::Texture::Null, 127, 513, 19, sdl::text::NO_WRAP, COLOR_TEXT, "Histórico inteligente");
    sdl::text::render(sdl::Texture::Null,
                      127,
                      543,
                      15,
                      sdl::text::NO_WRAP,
                      COLOR_TEXT_MUTED,
                      "As próximas proteções aparecerão aqui com destino, horário e verificação de integridade.");
    sdl::text::render(sdl::Texture::Null,
                      1015,
                      532,
                      14,
                      sdl::text::NO_WRAP,
                      COLOR_PRIMARY,
                      "NOVO NO SAVENX 0.2");
}

void MainMenuState::render_navigation()
{
    constexpr int navX = SCREEN_MARGIN;
    constexpr int navY = 614;
    constexpr int navW = graphics::SCREEN_WIDTH - (SCREEN_MARGIN * 2);
    constexpr int navH = 82;
    render_panel(navX, navY, navW, navH, 16, COLOR_HEADER, COLOR_BORDER);

    struct NavigationItem
    {
        DashboardAction action;
        const char *label;
    };
    constexpr std::array<NavigationItem, 4> items = {{{DashboardAction::ProtectAll, "Início"},
                                                       {DashboardAction::Games, "Jogos"},
                                                       {DashboardAction::History, "Histórico"},
                                                       {DashboardAction::Settings, "Configurações"}}};

    constexpr int itemX = 47;
    constexpr int itemY = 629;
    constexpr int itemW = 176;
    constexpr int itemH = 50;
    constexpr int itemGap = 7;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
    {
        const int x = itemX + (i * (itemW + itemGap));
        const bool selected = m_selectedAction == items[i].action;
        if (selected) { render_panel(x, itemY, itemW, itemH, 12, COLOR_PANEL_ALT, COLOR_FOCUS, 2); }

        const int textWidth = sdl::text::get_width(17, items[i].label);
        sdl::text::render(sdl::Texture::Null,
                          x + ((itemW - textWidth) / 2),
                          itemY + 15,
                          17,
                          sdl::text::NO_WRAP,
                          selected ? COLOR_TEXT : COLOR_TEXT_MUTED,
                          items[i].label);
    }

    sdl::text::render(sdl::Texture::Null, 805, 634, 15, sdl::text::NO_WRAP, COLOR_TEXT, "A  Abrir");
    sdl::text::render(sdl::Texture::Null, 905, 634, 15, sdl::text::NO_WRAP, COLOR_TEXT, "Y  Proteger");
    sdl::text::render(sdl::Texture::Null, 1024, 634, 15, sdl::text::NO_WRAP, COLOR_TEXT, "X  Avançado");
    sdl::text::render(sdl::Texture::Null, 805, 660, 13, sdl::text::NO_WRAP, COLOR_TEXT_MUTED, "+  Sair   •   ←→ Menu   •   L/R Perfil");
}

void MainMenuState::backup_all_for_all()
{
    // Keep save preparation out of application startup. It is cheap and safe to build
    // account-save candidates here because the user explicitly requested protection.
    prepare_all_user_save_candidates(sm_users);
    MainMenuState::refresh_view_states();

    remote::Storage *remote = remote::get_remote_storage();
    const bool autoUpload   = config::get_by_key(config::keys::AUTO_UPLOAD);
    const char *query       = strings::get_by_name(strings::names::MAINMENU_CONFS, 0);
    if (remote && autoUpload)
    {
        ConfirmProgress::create_push_fade(query, true, tasks::mainmenu::backup_all_for_all_remote, nullptr, m_dataStruct);
    }
    else { ConfirmProgress::create_push_fade(query, true, tasks::mainmenu::backup_all_for_all_local, nullptr, m_dataStruct); }
}
