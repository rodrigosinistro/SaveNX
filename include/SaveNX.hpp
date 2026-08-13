#pragma once
#include "appstates/BaseState.hpp"
#include "sdl.hpp"

#include <atomic>
#include <memory>
#include <vector>

/// @brief Main application class.
class SaveNX
{
    public:
        /// @brief Initializes SaveNX. Initializes services.
        SaveNX();

        /// @brief Exits services.
        ~SaveNX();

        /// @brief Returns if initializing was successful and SaveNX is running.
        /// @return True or false.
        bool is_running() const noexcept;

        /// @brief Runs SaveNX's update routine.
        void update();

        /// @brief Runs SaveNX's render routine.
        void render();

        /// @brief Function to allow tasks to tell SaveNX to exit.
        static void request_quit() noexcept;

    private:
        /// @brief Whether or not initialization was successful and SaveNX is still running.
        static inline std::atomic_bool sm_isRunning{};

        /// @brief Whether or not to print the translation credits.
        bool m_showTranslationInfo{};

        /// @brief SaveNX icon in upper left corner.
        sdl::SharedTexture m_headerIcon{};

        /// @brief Stores the translation string.
        std::string m_translationInfo{};

        /// @brief Stores the build string.
        std::string m_buildString{};

        /// @brief Sets the system to enable boost mode.
        void set_boost_mode();

        /// @brief Initializes fslib and takes care of a few other things.
        bool initialize_filesystem();

        /// @brief Initializes the services SaveNX uses.
        bool initialize_services();

        /// @brief Initializes SDL and loads the header icon.
        bool initialize_sdl();

        // Creates the needed directories on SD.
        bool create_directories();

        /// @brief Adds the text color changing characters.
        void add_color_chars();

        /// @brief Retrieves the strings from the map and sets them up for printing.
        void setup_translation_info_strings();

        /// @brief Pushed the applet mode warning pop-up if needed.
        void applet_mode_warning() noexcept;

        /// @brief Renders the base UI.
        void render_base();

        /// @brief Exits all services.
        void exit_services();
};
