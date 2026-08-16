#pragma once
#include "StateManager.hpp"
#include "appstates/TitleSelectCommon.hpp"
#include "data/data.hpp"
#include "sdl.hpp"

/// @brief SaveNX text title selection state.
class TextTitleSelectState final : public TitleSelectCommon
{
    public:
        /// @brief Constructs a new text title selector for a real Switch user.
        TextTitleSelectState(data::User *user);

        static inline std::shared_ptr<TextTitleSelectState> create(data::User *user)
        {
            return std::make_shared<TextTitleSelectState>(user);
        }

        static std::shared_ptr<TextTitleSelectState> create_and_push(data::User *user)
        {
            auto newState = TextTitleSelectState::create(user);
            StateManager::push_state(newState);
            return newState;
        }

        void update() override;
        void render() override;
        void refresh() override;

    private:
        data::User *m_user{};
        sdl::SharedTexture m_renderTarget{};

        // SaveNX owns its title-list navigation instead of relying on the inherited
        // animated ui::Menu/TextScroll path. This keeps long lists deterministic.
        int m_selected{};
        int m_firstVisible{};

        void handle_navigation();
        void clamp_window() noexcept;
        void sort_entries_alphabetically();

        void create_backup_menu();
        void create_title_option_menu();
        void add_remove_favorite();
};
