/**
 ******************************************************************************
 * @file    Screen.cpp
 ******************************************************************************
 */

#include "gui/screens/Screen.hpp"

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/LVGL/Draw.hpp"

KeyRouter* Screen::sRouter = nullptr;

Screen::Screen(Model& model)
    : mModel(model)
{
    mRoot = lv_obj_create(nullptr);
    SDK::LVGL::Draw::applyScreen(mRoot);
    lv_obj_add_event_cb(mRoot, &Screen::keyEventCb, LV_EVENT_KEY, this);
}

Screen::~Screen()
{
    mButtons.reset();
    lv_obj_delete(mRoot);
}

void Screen::keyEventCb(lv_event_t* e)
{
    namespace Btn = SDK::GUI::Button;
    auto*         self = static_cast<Screen*>(lv_event_get_user_data(e));
    const uint8_t code = static_cast<uint8_t>(lv_event_get_key(e));
    if (code != Btn::L1 && code != Btn::L2 && code != Btn::R1 && code != Btn::R2) {
        return;
    }
    if (sRouter && sRouter->routeKey(code)) {
        return;
    }
    self->onClick(code);
}
