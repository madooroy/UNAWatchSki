/**
 ******************************************************************************
 * @file    Screen.hpp
 * @brief   Base of the app's screens.
 *
 * Each screen owns one LVGL screen object. The kernel delivers button events
 * to the active screen as LV_EVENT_KEY; the base hands each click first to
 * the key router (the UI, for its banners and alerts), then to onClick().
 ******************************************************************************
 */

#ifndef SCREEN_HPP
#define SCREEN_HPP

#include <cstdint>
#include <memory>

#include "lvgl.h"

#include "SDK/GUI/LVGL/Buttons.hpp"

#include "gui/model/Model.hpp"

class KeyRouter
{
public:
    virtual ~KeyRouter() = default;
    /// @return true if the click was taken (e.g. it dismissed a banner).
    virtual bool routeKey(uint8_t code) = 0;
};

class Screen
{
public:
    explicit Screen(Model& model);
    virtual ~Screen();

    Screen(const Screen&)            = delete;
    Screen& operator=(const Screen&) = delete;

    lv_obj_t* root() const { return mRoot; }

    /// Called when the screen becomes active, and whenever the model changes.
    virtual void refresh() = 0;
    /// Called every display frame while active.
    virtual void onFrame() {}
    /// Called when the screen is shown.
    virtual void onShow() {}

    static void setRouter(KeyRouter* r) { sRouter = r; }

protected:
    /// A button click (SDK::GUI::Button::L1 / L2 / R1 / R2).
    virtual void onClick(uint8_t code) = 0;

    Model&    mModel;
    lv_obj_t* mRoot = nullptr;
    std::unique_ptr<SDK::LVGL::Buttons> mButtons;

private:
    static KeyRouter* sRouter;
    static void keyEventCb(lv_event_t* e);
};

#endif // SCREEN_HPP
