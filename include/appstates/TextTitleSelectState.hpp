#pragma once
#include "StateManager.hpp"
#include "appstates/TitleSelectCommon.hpp"
#include "data/data.hpp"
#include "sdl.hpp"

#include <vector>

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

        // Display order is independent from User::m_userData. Sorting the actual save
        // vector while resolving TitleInfo caused the 0.2.10 Games screen to block.
        std::vector<int> m_displayOrder{};

        void handle_navigation();
        void clamp_window() noexcept;
        int get_selected_source_index() const noexcept;

        void create_backup_menu();
        void create_title_option_menu();
        void add_remove_favorite();
};
