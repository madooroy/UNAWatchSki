/**
 ******************************************************************************
 * @file    Ui.hpp
 * @brief   Owns the screens, shows the one that matches the day (Home,
 *          Settings, Day, Summary), and draws the overlays on LVGL's top
 *          layer, over any screen:
 *
 *   banner   end of a run (vertical, distance, top speed, a best), GPS
 *            ready, low battery; gone after a few seconds or on any button
 *   storm    full screen, amber, until a button is pressed
 *
 *   A press that dismisses an overlay does nothing else: with gloves on, a
 *   button pressed to clear a banner must not also pause the day.
 ******************************************************************************
 */

#ifndef UI_HPP
#define UI_HPP

#include "gui/model/Model.hpp"
#include "gui/model/ModelListener.hpp"
#include "gui/screens/DayScreen.hpp"
#include "gui/screens/HomeScreen.hpp"
#include "gui/screens/SettingsScreen.hpp"
#include "gui/screens/SummaryScreen.hpp"

class Ui : public ModelListener, public KeyRouter
{
public:
    explicit Ui(Model& model);

    void onFrame() override;
    void onSettings() override;
    void onState() override;
    void onLive() override;
    void onEvent() override;
    void onLog() override;

    bool routeKey(uint8_t code) override;

private:
    Model&         mModel;
    HomeScreen     mHome;
    SettingsScreen mSettings;
    DayScreen      mDay;
    SummaryScreen  mSummary;
    Screen*        mActive     = nullptr;
    bool           mInSettings = false;

    // Banner
    lv_obj_t* mBanner = nullptr;
    lv_obj_t* mBTitle = nullptr;
    lv_obj_t* mBBig   = nullptr;
    lv_obj_t* mBUnit  = nullptr;
    lv_obj_t* mBLine  = nullptr;
    lv_obj_t* mBNote  = nullptr;
    uint32_t  mBannerUntil = 0;
    bool      mBannerShown = false;

    // Storm
    lv_obj_t* mStorm = nullptr;
    lv_obj_t* mSRate = nullptr;
    lv_obj_t* mSUnit = nullptr;
    lv_point_precise_t mBolt[6] = {};
    bool      mStormShown = false;

    void    show(Screen& s);
    Screen& screenForState();
    void    buildOverlays();
    void    showBanner(const char* title, uint32_t titleColor, const char* big, const char* unit,
                       const char* line, const char* note, uint32_t ms);
    void    hideBanner();
    void    showStorm(int16_t rateCenti);
    void    hideStorm();

    static void openSettings(void* ctx);
    static void closeSettings(void* ctx);
};

#endif // UI_HPP
