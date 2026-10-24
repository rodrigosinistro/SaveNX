#include "appstates/HomeState.hpp"
#include "StateManager.hpp"
#include "appstates/MainMenuState.hpp"
#include "appstates/SettingsState.hpp"
#include "data/data.hpp"
#include "graphics/colors.hpp"
#include "input.hpp"
#include "remote/remote.hpp"
#include "sdl.hpp"

HomeState::HomeState()
    : m_menu(ui::Menu::create(48, 20, 1160, 27, 300)),
      m_target(sdl::TextureManager::load("homeTarget", 1280, 300, SDL_TEXTUREACCESS_TARGET))
{
    m_menu->add_option("Jogos e perfis");
    m_menu->add_option("Conectar Google Drive");
    m_menu->add_option("Reenviar backups pendentes");
    m_menu->add_option("Configurações");
    m_menu->add_option("Configurar cliente Google");
}

void HomeState::update()
{
    if (!has_focus()) { return; }
    m_menu->update(true);
    if (!input::button_pressed(HidNpadButton_A)) { return; }
    switch (m_menu->get_selected())
    {
        case 0:
            if (m_loaded) { MainMenuState::create_and_push(); }
            else
            {
                data::launch_initialization(false, [this] {
                    m_loaded = true;
                    MainMenuState::create_and_push();
                });
            }
            break;
        case 1: remote::connect(); break;
        case 2: remote::retry_pending(); break;
        case 3: StateManager::push_state(SettingsState::create()); break;
        case 4: remote::configure_client(); break;
    }
}

void HomeState::render()
{
    m_target->clear(colors::TRANSPARENT);
    m_menu->render(m_target, has_focus());
    m_target->render(sdl::Texture::Null, 0, 180);
    sdl::text::render(sdl::Texture::Null, 48, 118, 28, sdl::text::NO_WRAP, colors::WHITE,
                      "Seus saves, sempre protegidos");
    const std::string status = remote::status();
    const std::string pending = "Uploads pendentes: " + std::to_string(remote::pending_count());
    sdl::text::render(sdl::Texture::Null, 48, 510, 21, sdl::text::NO_WRAP, colors::WHITE, status);
    sdl::text::render(sdl::Texture::Null, 48, 552, 21, sdl::text::NO_WRAP, colors::WHITE, pending);
    sdl::text::render(sdl::Texture::Null, 48, 606, 18, sdl::text::NO_WRAP, colors::WHITE,
                      "A: selecionar    B: voltar nos menus    +: sair");
}
