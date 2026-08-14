#include "SaveNX.hpp"

#include "StateManager.hpp"
#include "app_paths.hpp"
#include "appstates/FileModeState.hpp"
#include "appstates/MainMenuState.hpp"
#include "appstates/TaskState.hpp"
#include "builddate.hpp"
#include "config/config.hpp"
#include "curl/curl.hpp"
#include "data/data.hpp"
#include "error.hpp"
#include "fslib.hpp"
#include "graphics/colors.hpp"
#include "graphics/screen.hpp"
#include "input.hpp"
#include "logging/logger.hpp"
#include "migration.hpp"
#include "remote/remote.hpp"
#include "sdl.hpp"
#include "strings/strings.hpp"
#include "stringutil.hpp"
#include "sys/sys.hpp"
#include "ui/PopMessageManager.hpp"
#include "version.hpp"

#include <array>
#include <chrono>
#include <string_view>
#include <switch.h>
#include <thread>

// Normally I try to avoid C macros in C++, but this cleans stuff up nicely.
#define ABORT_ON_FAILURE(x)                                                                                                    \
    if (!x) { return; }

namespace
{
    /// @brief Config for socket.
    constexpr SocketInitConfig SOCKET_INIT_CONFIG = {.tcp_tx_buf_size     = 0x20000,
                                                     .tcp_rx_buf_size     = 0x20000,
                                                     .tcp_tx_buf_max_size = 0x80000,
                                                     .tcp_rx_buf_max_size = 0x80000,
                                                     .udp_tx_buf_size     = 0x2400,
                                                     .udp_rx_buf_size     = 0xA500,
                                                     .sb_efficiency       = 8,
                                                     .num_bsd_sessions    = 3,
                                                     .bsd_service_type    = BsdServiceType_User};

    constexpr std::array<std::string_view, 6> REQUIRED_DIRECTORIES = {savenx::paths::APP_ROOT,
                                                                       savenx::paths::CONFIG_DIR,
                                                                       savenx::paths::CACHE_DIR,
                                                                       savenx::paths::LOG_DIR,
                                                                       savenx::paths::TEMP_DIR,
                                                                       savenx::paths::BACKUP_DIR};

} // namespace

// This function allows any service init to be logged with its name without repeating the code.
template <typename... Args>
static bool initialize_service(Result (*function)(Args...), const char *serviceName, Args... args)
{
    Result error = (*function)(args...);
    if (R_FAILED(error))
    {
        logger::log("Error initializing %s: 0x%X.", serviceName, error);
        return false;
    }
    return true;
}

//                      ---- Construction ----

SaveNX::SaveNX()
{
    // Set boost mode first.
    SaveNX::set_boost_mode();

    // Nothing in SaveNX can really continue without these.
    ABORT_ON_FAILURE(SaveNX::initialize_services());
    ABORT_ON_FAILURE(SaveNX::initialize_filesystem());

    // Create the unified layout before the logger and config try to open files inside it.
    for (const std::string_view directory : REQUIRED_DIRECTORIES)
    {
        const fslib::Path path{directory};
        const bool exists = fslib::directory_exists(path);
        ABORT_ON_FAILURE(exists || fslib::create_directories_recursively(path));
    }

    const bool migrationCompleted = migration::migrate_v0_1_0_layout();

    // Create the log file if it hasn't been already.
    logger::initialize();
    if (!migrationCompleted) { logger::log("SaveNX v0.1.0 path migration was only partially completed."); }

    // SDL2
    ABORT_ON_FAILURE(SaveNX::initialize_sdl());

    // Curl.
    ABORT_ON_FAILURE(curl::initialize());

    // Config and input.
    input::initialize();
    config::initialize();

    // These are the strings used in the UI.
    ABORT_ON_FAILURE(strings::initialize()); // This is fatal now.

    SaveNX::setup_translation_info_strings();

    // This needs the config init'd or read to work.
    SaveNX::create_directories();
    sys::threadpool::initialize(); // This is the thread pool so SaveNX isn't constantly creating and destroying threads.

    // Push the remote init.
    sys::threadpool::push_job(remote::initialize, nullptr);

    // Launch the loading init. Finish init is called afterwards.
    auto init_finish = []() { MainMenuState::create_and_push(); }; // Lambda that's exec'd after state is finished.
    data::launch_initialization(false, init_finish);

    // This isn't required, but why not?
    FadeState::create_and_push(colors::BLACK, 0xFF, 0x00, nullptr);

    // Push this warning so people can't complain if SaveNX runs out of RAM.
    SaveNX::applet_mode_warning();

    // SaveNX is now running.
    sm_isRunning = true;
}

//                      ---- Destruction ----

SaveNX::~SaveNX()
{
    sys::threadpool::exit();
    config::save();
    curl::exit();
    SaveNX::exit_services();
    sdl::text::SystemFont::exit();
    sdl::exit();

    appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    appletUnlockExit();
}

//                      ---- Public functions ----

bool SaveNX::is_running() const noexcept { return sm_isRunning && appletMainLoop(); }

void SaveNX::update()
{
    input::update();

    const bool plusPressed = input::button_pressed(HidNpadButton_Plus);
    const bool isClosable  = StateManager::back_is_closable();
    if (plusPressed && isClosable) { sm_isRunning = false; }

    StateManager::update();
    ui::PopMessageManager::update();
}

void SaveNX::render()
{
    sdl::frame_begin(colors::CLEAR_COLOR);

    SaveNX::render_base();
    StateManager::render();
    ui::PopMessageManager::render();

    sdl::frame_end();
}

void SaveNX::request_quit() noexcept { sm_isRunning = false; }

//                      ---- Private functions ----

void SaveNX::set_boost_mode()
{
    // Log for errors, but not fatal.
    error::libnx(appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad));
}

bool SaveNX::initialize_filesystem()
{
    // This needs to be in this specific order
    const bool fslib    = fslib::is_initialized();
    const bool romfs    = initialize_service(romfsInit, "RomFS");
    const bool fslibDev = fslib && fslib::dev::initialize_sdmc();
    if (!fslib || !romfs || !fslibDev) { return false; }

    return true;
}

bool SaveNX::initialize_services()
{
    // This looks cursed, but it works.
    bool serviceInit = initialize_service(accountInitialize, "Account", AccountServiceType_Administrator);
    serviceInit      = serviceInit && initialize_service(nsInitialize, "NS");
    serviceInit      = serviceInit && initialize_service(pdmqryInitialize, "PDMQry");
    serviceInit      = serviceInit && initialize_service(plInitialize, "PL", PlServiceType_User);
    serviceInit      = serviceInit && initialize_service(pmshellInitialize, "PMShell");
    serviceInit      = serviceInit && initialize_service(setInitialize, "Set");
    serviceInit      = serviceInit && initialize_service(setsysInitialize, "SetSys");
    serviceInit      = serviceInit && initialize_service(socketInitialize, "Socket", &SOCKET_INIT_CONFIG);
    serviceInit      = serviceInit && initialize_service(nifmInitialize, "NIFM", NifmServiceType_User);
    serviceInit      = serviceInit && initialize_service(appletLockExit, "AppletLockExit");
    return serviceInit;
}

bool SaveNX::initialize_sdl()
{
    // Initialize SDL, freetype and the system font.
    bool sdlInit = sdl::initialize("SaveNX", graphics::SCREEN_WIDTH, graphics::SCREEN_HEIGHT);
    sdlInit      = sdlInit && sdl::text::SystemFont::initialize();
    if (!sdlInit) { return false; }

    // Load the icon in the top left.
    m_headerIcon = sdl::TextureManager::load("headerIcon", "romfs:/Textures/HeaderIcon.png");
    if (!m_headerIcon) { return false; }

    // Push the color changing characters.
    SaveNX::add_color_chars();

    return true;
}

bool SaveNX::create_directories()
{
    // Working directory creation.
    const fslib::Path workDir{config::get_working_directory()};
    const bool needsWorkDir   = !fslib::directory_exists(workDir);
    const bool workDirCreated = needsWorkDir && fslib::create_directories_recursively(workDir);
    if (needsWorkDir && !workDirCreated) { return false; }

    // Trash folder. This can fail without being fatal.
    const fslib::Path trashDir{workDir / "_TRASH_"};
    const bool trashEnabled = config::get_by_key(config::keys::ENABLE_TRASH_BIN);
    const bool needsTash    = trashEnabled && !fslib::directory_exists(trashDir);
    if (needsTash) { error::fslib(fslib::create_directory(trashDir)); }

    return true;
}

void SaveNX::add_color_chars()
{
    sdl::text::add_color_character(L'#', colors::BLUE);
    sdl::text::add_color_character(L'*', colors::DARK_RED);
    sdl::text::add_color_character(L'<', colors::YELLOW);
    sdl::text::add_color_character(L'>', colors::GREEN);
    sdl::text::add_color_character(L'`', colors::BLUE_GREEN);
    sdl::text::add_color_character(L'^', colors::PINK);
    sdl::text::add_color_character(L'$', colors::GOLD);
}

void SaveNX::setup_translation_info_strings()
{
    const char *translationFormat = strings::get_by_name(strings::names::TRANSLATION, 0);
    const char *author            = strings::get_by_name(strings::names::TRANSLATION, 1);
    m_showTranslationInfo         = std::char_traits<char>::compare(author, "NULL", 4) != 0; // This is whether or not to show.
    m_translationInfo             = stringutil::get_formatted_string(translationFormat, author);
    m_buildString = stringutil::get_formatted_string("v%s  |  build %02d.%02d.%04d",
                                                     savenx::VERSION.data(),
                                                     builddate::MONTH,
                                                     builddate::DAY,
                                                     builddate::YEAR);
}

void SaveNX::applet_mode_warning() noexcept
{
    // This hangs longer than other pop messages.
    static constexpr int APPLET_TICKS = 5000;

    // Anything that doesn't register as an application is an applet as far as I'm concerned.
    const bool isApplet = appletGetAppletType() != AppletType_Application;
    if (!isApplet) { return; }

    // Get the string and push the pop message.
    const char *appletString = strings::get_by_name(strings::names::APPLET_MODE, 0);
    ui::PopMessageManager::push_message(APPLET_TICKS, appletString);
}

void SaveNX::render_base()
{
    // These are the same for both.
    static constexpr int LINE_X_BEGIN = 30;
    static constexpr int LINE_X_END   = 1250;

    // These are the Y's.
    static constexpr int LINE_A_Y = 88;
    static constexpr int LINE_B_Y = 648;

    // Coordinates for the header icon.
    static constexpr int HEADER_X = 66;
    static constexpr int HEADER_Y = 27;

    // Coordinates for the title text.
    static constexpr int TITLE_X    = 130;
    static constexpr int TITLE_Y    = 32;
    static constexpr int TITLE_SIZE = 34;

    // Coordinates for the translation info and build date.
    static constexpr int BUILD_X    = 8;
    static constexpr int BUILD_Y    = 700;
    static constexpr int TRANS_Y    = 680;
    static constexpr int BUILD_SIZE = 14;

    // This is just the SaveNX string.
    static constexpr std::string_view TITLE_TEXT = "SaveNX";

    // Top and bottom framing lines.
    sdl::render_line(sdl::Texture::Null, LINE_X_BEGIN, LINE_A_Y, LINE_X_END, LINE_A_Y, colors::WHITE);
    sdl::render_line(sdl::Texture::Null, LINE_X_BEGIN, LINE_B_Y, LINE_X_END, LINE_B_Y, colors::WHITE);

    // Icon
    m_headerIcon->render(sdl::Texture::Null, HEADER_X, HEADER_Y);

    // "SaveNX"
    sdl::text::render(sdl::Texture::Null, TITLE_X, TITLE_Y, TITLE_SIZE, sdl::text::NO_WRAP, colors::WHITE, TITLE_TEXT);

    // Translation info in bottom left.
    if (m_showTranslationInfo)
    {
        sdl::text::render(sdl::Texture::Null,
                          BUILD_X,
                          TRANS_Y,
                          BUILD_SIZE,
                          sdl::text::NO_WRAP,
                          colors::WHITE,
                          m_translationInfo);
    }

    // Build date
    sdl::text::render(sdl::Texture::Null, BUILD_X, BUILD_Y, BUILD_SIZE, sdl::text::NO_WRAP, colors::WHITE, m_buildString);
}

void SaveNX::exit_services()
{
    nifmExit();
    socketExit();
    setsysExit();
    setExit();
    pmshellExit();
    plExit();
    pdmqryExit();
    nsExit();
    accountExit();
}
