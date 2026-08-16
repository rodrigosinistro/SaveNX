#include "appstates/TaskState.hpp"

#include "appstates/FadeState.hpp"
#include "graphics/colors.hpp"
#include "graphics/screen.hpp"
#include "input.hpp"
#include "sdl.hpp"
#include "strings/strings.hpp"
#include "ui/PopMessageManager.hpp"

//                      ---- Construction ----

TaskState::TaskState(sys::threadpool::JobFunction function, sys::Task::TaskData taskData)
{
    m_task = std::make_unique<sys::Task>(function, taskData);
}

//                      ---- Public functions ----

void TaskState::update()
{
    BaseTask::pop_on_plus();
    BaseTask::update_loading_glyph();
    if (!m_task->is_running()) { TaskState::deactivate_state(); }
}

void TaskState::render()
{
    const std::string status = m_task->get_status();
    const int statusWidth    = sdl::text::get_width(BaseTask::FONT_SIZE, status.c_str());
    const int statusX        = 640 - (statusWidth / 2);

    sdl::render_rect_fill(sdl::Texture::Null, 0, 0, graphics::SCREEN_WIDTH, graphics::SCREEN_HEIGHT, colors::DIM_BACKGROUND);

    // SaveNX task messages can contain important instructions (notably Google OAuth).
    // Keep them visually independent from the menu/state underneath by placing an
    // opaque black band behind the status text instead of drawing directly over the
    // previous screen.
    constexpr sdl::Color STATUS_PANEL = {0x000000FF};
    constexpr int STATUS_PANEL_Y      = 315;
    constexpr int STATUS_PANEL_HEIGHT = 92;
    sdl::render_rect_fill(sdl::Texture::Null,
                          24,
                          STATUS_PANEL_Y,
                          graphics::SCREEN_WIDTH - 48,
                          STATUS_PANEL_HEIGHT,
                          STATUS_PANEL);

    sdl::text::render(sdl::Texture::Null, statusX, 351, BaseTask::FONT_SIZE, sdl::text::NO_WRAP, colors::WHITE, status);

    BaseTask::render_loading_glyph();
}

void TaskState::deactivate_state()
{
    FadeState::create_and_push(colors::DIM_BACKGROUND, colors::ALPHA_FADE_END, colors::ALPHA_FADE_BEGIN, nullptr);
    BaseState::deactivate();
}
