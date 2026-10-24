#pragma once
#include "appstates/BaseState.hpp"
#include "ui/Menu.hpp"
#include <memory>

class HomeState final : public BaseState
{
    public:
        HomeState();
        void update() override;
        void render() override;
    private:
        std::shared_ptr<ui::Menu> m_menu;
        sdl::SharedTexture m_target;
        bool m_loaded{};
};
