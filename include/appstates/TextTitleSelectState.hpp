#pragma once
#include "StateManager.hpp"
#include "appstates/TitleSelectCommon.hpp"
#include "data/data.hpp"
#include "sdl.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
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

        /// @brief Copies a detached title label during lazy candidate preparation.
        static void cache_title_label(uint64_t applicationID, std::string_view title);

        void update() override;
        void render() override;
        void refresh() override;

    private:
        data::User *m_user{};
        sdl::SharedTexture m_renderTarget{};

        int m_selected{};
        int m_firstVisible{};
        std::vector<int> m_displayOrder{};

        // Detached strings only. The Games screen never resolves TitleInfo to draw rows.
        static inline std::unordered_map<uint64_t, std::string> sm_titleLabels{};
        static std::string get_cached_title(uint64_t applicationID);

        void handle_navigation();
        void clamp_window() noexcept;
        int get_selected_source_index() const noexcept;

        void create_backup_menu();
        void create_title_option_menu();
        void add_remove_favorite();
};
