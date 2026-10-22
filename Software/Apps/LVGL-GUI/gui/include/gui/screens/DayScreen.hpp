/**
 ******************************************************************************
 * @file    DayScreen.hpp
 * @brief   During the day: four faces, stepped with L1/L2.
 *
 *   Live    follows what you are doing:
 *             skiing   speed, this run's drop, distance and time; the ring
 *                      fills with the drop against your biggest run
 *             on lift  the run you just did (when you have time to look),
 *                      altitude and climb rate
 *             stopped  the last run, or the day so far
 *   Day     runs, vertical, top speed, distance, and the day split into
 *           skiing / lifts / standing
 *   Storm   the pressure trend, its sparkline and what it means
 *   Clock   time, battery, GPS, altitude (and the demo's progress)
 *
 *   R1 opens the menu (pause or resume, end the day, discard, demo speed).
 *   R2 goes back to the Live face.
 ******************************************************************************
 */

#ifndef DAY_SCREEN_HPP
#define DAY_SCREEN_HPP

#include <memory>

#include "SDK/GUI/LVGL/ScrollIndicator.hpp"
#include "SDK/GUI/LVGL/TimerRing.hpp"

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Charts.hpp"

class DayScreen : public Screen
{
public:
    explicit DayScreen(Model& model);
    ~DayScreen() override;

    void refresh() override;
    void onShow() override;

protected:
    void onClick(uint8_t code) override;

private:
    enum Face : uint8_t { LIVE = 0, DAY, STORM, CLOCK, FACES };

    uint8_t mFace = LIVE;
    std::unique_ptr<SDK::LVGL::ScrollIndicator> mScroll;

    // Live
    lv_obj_t* mLive = nullptr;
    std::unique_ptr<SDK::LVGL::TimerRing> mRing;
    lv_obj_t* mLHeader  = nullptr;
    lv_obj_t* mLCaption = nullptr;
    lv_obj_t* mLBig     = nullptr;
    lv_obj_t* mLUnit    = nullptr;
    lv_obj_t* mLLine1   = nullptr;
    lv_obj_t* mLLine2   = nullptr;
    lv_obj_t* mLFoot    = nullptr;

    // Day
    lv_obj_t* mDay = nullptr;
    lv_obj_t* mDHeader = nullptr;
    lv_obj_t* mDRuns   = nullptr;
    lv_obj_t* mDRunsU  = nullptr;
    lv_obj_t* mDVert   = nullptr;
    lv_obj_t* mDLine   = nullptr;
    lv_obj_t* mDBarRun  = nullptr;
    lv_obj_t* mDBarLift = nullptr;
    lv_obj_t* mDBarIdle = nullptr;
    lv_obj_t* mDSplit  = nullptr;

    // Storm
    lv_obj_t* mStorm = nullptr;
    lv_obj_t* mSHeader = nullptr;
    lv_obj_t* mSRate   = nullptr;
    lv_obj_t* mSUnit   = nullptr;
    std::unique_ptr<Sparkline> mSpark;
    lv_obj_t* mSStatus = nullptr;
    lv_obj_t* mSInfo   = nullptr;
    lv_obj_t* mSPress  = nullptr;

    // Clock
    lv_obj_t* mClock = nullptr;
    lv_obj_t* mCTime   = nullptr;
    lv_obj_t* mCDate   = nullptr;
    lv_obj_t* mCBatt   = nullptr;
    lv_obj_t* mCGps    = nullptr;
    lv_obj_t* mCAlt    = nullptr;
    lv_obj_t* mCDemo   = nullptr;

    // Menu
    static constexpr uint8_t kMenuMax = 4;
    enum class Item : uint8_t { PauseResume, End, Discard, Speed };
    lv_obj_t* mMenu = nullptr;
    lv_obj_t* mMTitle = nullptr;
    lv_obj_t* mMSel   = nullptr;
    lv_obj_t* mMItem[kMenuMax] = {};
    Item      mItems[kMenuMax] = {};
    uint8_t   mItemCount   = 0;
    uint8_t   mSel         = 0;
    bool      mMenuOpen    = false;
    bool      mConfirmDiscard = false;

    void showFace(uint8_t f);
    void refreshLive();
    void refreshDay();
    void refreshStorm();
    void refreshClock();
    void openMenu();
    void closeMenu();
    void refreshMenu();
    void menuClick(uint8_t code);
    void setBig(const char* value, const char* unit, int32_t y);
};

#endif // DAY_SCREEN_HPP
