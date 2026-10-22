/**
 ******************************************************************************
 * @file    SettingsScreen.hpp
 * @brief   Five settings on a wheel: the selected one large in the middle,
 *          with a line saying what it does; its neighbours above and below.
 *
 *   Sport        Ski / Snowboard (sets the FIT sport)
 *   Storm alert  Off / 2 / 3 / 4 hPa fall in 3 hours
 *   Run alerts   a buzz at the end of each run, a tap on each lift
 *   Sound        Beep + buzz / Buzz only / Beep only
 *   Demo day     Off / 1x / 10x / 60x: a simulated day to try the app
 *
 *   L1/L2 move, R1 changes the value (saved at once), R2 goes back.
 ******************************************************************************
 */

#ifndef SETTINGS_SCREEN_HPP
#define SETTINGS_SCREEN_HPP

#include "gui/screens/Screen.hpp"

class SettingsScreen : public Screen
{
public:
    SettingsScreen(Model& model, void (*back)(void*), void* ctx);

    void refresh() override;
    void onShow() override;

protected:
    void onClick(uint8_t code) override;

private:
    enum Row : uint8_t { SPORT = 0, STORM, RUN_ALERTS, SOUND, DEMO, ROWS };

    void (*mBack)(void*);
    void*     mCtx;
    uint8_t   mSel = 0;
    lv_obj_t* mTitle   = nullptr;
    lv_obj_t* mPrev    = nullptr;
    lv_obj_t* mBand    = nullptr;
    lv_obj_t* mCaption = nullptr;
    lv_obj_t* mValue   = nullptr;
    lv_obj_t* mHint    = nullptr;
    lv_obj_t* mNext    = nullptr;
    lv_obj_t* mDots[ROWS] = {};

    static const char* caption(uint8_t row);
    void        valueText(uint8_t row, char* buf, size_t n) const;
    void        change(uint8_t row);
};

#endif // SETTINGS_SCREEN_HPP
