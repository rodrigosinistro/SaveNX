#include "appstates/DataLoadingState.hpp"

#include "graphics/colors.hpp"
#include "graphics/screen.hpp"
#include "logging/logger.hpp"

namespace
{
    constexpr int SCREEN_CENTER = 640;
}

//                      ---- Construction ----

DataLoadingState::DataLoadingState(data::DataContext &context,
                                   DestructFunction destructFunction,
                                   sys::threadpool::JobFunction function,
                                   sys::Task::TaskData taskData)
    : BaseTask()
    , m_context(context)
    , m_destructFunction(destructFunction)
{
    DataLoadingState::initialize_static_members();
    m_task = std::make_unique<sys::Task>(function, taskData);
}

//                      ---- Public functions ----

void DataLoadingState::update()
{
    BaseTask::update_loading_glyph();
    if (!m_task->is_running()) { DataLoadingState::deactivate_state(); }

    // SaveNX 0.2.5: never decode the complete title/profile icon queue while the
    // application is trying to leave the loading screen. Doing so used to make
    // "Finalizando" depend on every icon being successfully decoded before the
    // dashboard could appear. The dashboard already has safe visual fallbacks for
    // missing icons, so startup must not wait for cosmetic assets.
}

void DataLoadingState::sub_update() { BaseTask::update_loading_glyph(); }

void DataLoadingState::render()
{
    static constexpr int ICON_X_COORD = SCREEN_CENTER - 128;
    static constexpr int ICON_Y_COORD = 226;
    const std::string status          = m_task->get_status();

    const int statusWidth = sdl::text::get_width(BaseTask::FONT_SIZE, status);
    m_statusX             = SCREEN_CENTER - (statusWidth / 2);

    sdl::render_rect_fill(sdl::Texture::Null, 0, 0, graphics::SCREEN_WIDTH, graphics::SCREEN_HEIGHT, colors::CLEAR_COLOR);
    sm_savenxIcon->render(sdl::Texture::Null, ICON_X_COORD, ICON_Y_COORD);
    sdl::text::render(sdl::Texture::Null, m_statusX, 673, BaseTask::FONT_SIZE, sdl::text::NO_WRAP, colors::WHITE, status);
    BaseTask::render_loading_glyph();
}

//                      ---- Private functions ----

void DataLoadingState::initialize_static_members()
{
    if (sm_savenxIcon) { return; }

    sm_savenxIcon = sdl::TextureManager::load("LoadingIcon", "romfs:/Textures/LoadingIcon.png");
}

void DataLoadingState::deactivate_state()
{
    // SaveNX startup is data-first and cosmetics-later. Do not process the inherited
    // icon queue here: this callback runs while the user still sees "Finalizando".
    logger::log("Data initialization completed; entering dashboard without blocking on icon queue.");

    if (m_destructFunction) { m_destructFunction(); }
    BaseState::deactivate();
}
