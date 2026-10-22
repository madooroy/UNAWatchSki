/**
 ******************************************************************************
 * @file    HomeScreen.hpp
 * @brief   Before the day: sport, start, GPS status, the season so far.
 *
 *              /\/\            <- logo
 *            SKI DAY
 *   SPORT                START  <- L1 / R1
 *   Ski
 *            GPS ready          <- or "Finding GPS", or "Demo day 60x"
 *         Storm alert 3 hPa
 *   SETTINGS                    <- L2
 *      Season 4 days · 9,420 m
 *
 *   Buttons: L1 ski/snowboard, L2 settings, R1 start, R2 exit.
 ******************************************************************************
 */

#ifndef HOME_SCREEN_HPP
#define HOME_SCREEN_HPP

#include "gui/screens/Screen.hpp"

class HomeScreen : public Screen
{
public:
    HomeScreen(Model& model, void (*openSettings)(void*), void* ctx);

    void refresh() override;

protected:
    void onClick(uint8_t code) override;

private:
    void (*mOpenSettings)(void*);
    void*     mCtx;
    lv_point_precise_t mLogoPts[7] = {};
    lv_obj_t* mLogo         = nullptr;
    lv_obj_t* mTitle        = nullptr;
    lv_obj_t* mSportCaption = nullptr;
    lv_obj_t* mSport        = nullptr;
    lv_obj_t* mStart        = nullptr;
    lv_obj_t* mStatus       = nullptr;
    lv_obj_t* mStatusDot    = nullptr;
    lv_obj_t* mStorm        = nullptr;
    lv_obj_t* mSettings     = nullptr;
    lv_obj_t* mSeason       = nullptr;
};

#endif // HOME_SCREEN_HPP
