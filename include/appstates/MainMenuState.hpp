#pragma once
#include "StateManager.hpp"
#include "appstates/BaseState.hpp"
#include "data/data.hpp"
#include "sdl.hpp"

#include <cstdint>
#include <memory>
#include <vector>

/// @brief The main
class MainMenuState final : public BaseState
{
    public:
        /// @brief Creates and initializes the main menu.
        MainMenuState();

        /// @brief Returns a new MainMenuState
        static inline std::shared_ptr<MainMenuState> create() { return std::make_shared<MainMenuState>(); }

        /// @brief Creates and returns a new MainMenuState. Pushes it automatically.
        static inline std::shared_ptr<MainMenuState> create_and_push()
        {
            auto newState = MainMenuState::create();
            StateManager::push_state(newState);
            return newState;
        }

        /// @brief Runs update routine.
        void update() override;

        /// @brief Runs the sub-update routine.
        void sub_update() override;

        /// @brief Renders menu to screen.
        void render() override;

        /// @brief Signals to
        static void initialize_view_states();

        /// @brief Calls refresh on on view states in the vector.
        static void refresh_view_states();

        // clang-format off
        struct DataStruct : sys::Task::DataStruct
        {
            data::UserList userList;
            MainMenuState *spawningState{};
        };
        // clang-format on

    private:
        enum class DashboardAction : uint8_t
        {
            ProtectAll,
            Games,
            History,
            Settings
        };

        /// @brief Current dashboard action selected by the user.
        DashboardAction m_selectedAction{DashboardAction::ProtectAll};

        /// @brief Profile shown on the dashboard and opened by the Games action.
        int m_activeUserIndex{};

        /// @brief This is the data struct passed to tasks.
        std::shared_ptr<MainMenuState::DataStruct> m_dataStruct{};

        /// @brief Records the size of the sm_users vector.
        static inline int sm_userCount{};

        /// @brief This is the list of user pointers from data.
        static inline data::UserList sm_users{};

        /// @brief This is the pointer to the settings state.
        static inline std::shared_ptr<BaseState> sm_settingsState{};

        /// @brief This is the pointer to the extras state.
        static inline std::shared_ptr<BaseState> sm_extrasState{};

        /// @brief This is the vector of title selection states.
        static inline std::vector<std::shared_ptr<BaseState>> sm_states{};

        /// @brief Creates the settings and extras.
        void initialize_settings_extras();

        /// @brief Loads the Switch users used by the dashboard.
        void initialize_users();

        /// @brief Initializes the data struct.
        void initialize_data_struct();

        /// @brief Executes the currently highlighted dashboard action.
        void activate_selected_action();

        /// @brief Opens the game list for the active profile.
        void open_active_user();

        /// @brief Opens one of the persistent legacy states while it is being redesigned.
        void open_persistent_state(std::shared_ptr<BaseState> &state);

        /// @brief Cycles through the available Switch profiles.
        void cycle_active_user(int direction) noexcept;

        /// @brief Updates directional navigation between the hero action and bottom tabs.
        void update_navigation() noexcept;

        /// @brief Renders the complete SaveNX dashboard.
        void render_dashboard();

        /// @brief Renders the SaveNX identity and active profile.
        void render_header();

        /// @brief Renders the protection hero and its live status summary.
        void render_protection_hero();

        /// @brief Renders up to three real games for the active profile.
        void render_game_cards();

        /// @brief Renders the protection history placeholder.
        void render_history();

        /// @brief Renders the bottom dashboard navigation and control guide.
        void render_navigation();

        /// @brief Backups up all save data for all users.
        void backup_all_for_all();

};
