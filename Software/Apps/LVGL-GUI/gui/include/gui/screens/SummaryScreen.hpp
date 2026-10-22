/**
 ******************************************************************************
 * @file    SummaryScreen.hpp
 * @brief   After the day: three pages, stepped with L1/L2.
 *
 *   Overview  runs, vertical, top speed, distance, the time split, the hours
 *   Profile   altitude through the day: runs white, lifts blue
 *   Runs      every run: vertical and top speed, the best ones marked; the
 *             one in the middle shows its start, distance, time and average
 *
 *   On the Runs page L1/L2 scroll the list and move on past either end.
 *   R1 or R2 goes home.
 ******************************************************************************
 */

#ifndef SUMMARY_SCREEN_HPP
#define SUMMARY_SCREEN_HPP

#include <memory>

#include "SDK/GUI/LVGL/ScrollIndicator.hpp"

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Charts.hpp"

class SummaryScreen : public Screen
{
public:
    explicit SummaryScreen(Model& model);
    ~SummaryScreen() override;

    void refresh() override;
    void onShow() override;

protected:
    void onClick(uint8_t code) override;

private:
    enum Page : uint8_t { OVERVIEW = 0, PROFILE, RUNS, PAGES };
    static constexpr uint8_t kRows = 5;

    uint8_t mPage = OVERVIEW;
    uint8_t mSel  = 0;   ///< selected run on the Runs page
    std::unique_ptr<SDK::LVGL::ScrollIndicator> mScroll;

    lv_obj_t* mOverview = nullptr;
    lv_obj_t* mOHeader  = nullptr;
    lv_obj_t* mORuns    = nullptr;
    lv_obj_t* mORunsU   = nullptr;
    lv_obj_t* mOVert    = nullptr;
    lv_obj_t* mOLine    = nullptr;
    lv_obj_t* mOBar[3]  = {};
    lv_obj_t* mOSplit   = nullptr;
    lv_obj_t* mOHours   = nullptr;

    lv_obj_t* mProfile  = nullptr;
    lv_obj_t* mPHeader  = nullptr;
    std::unique_ptr<ProfileChart> mChart;
    lv_obj_t* mPLine    = nullptr;
    lv_obj_t* mPRange   = nullptr;
    lv_obj_t* mPKeyRun  = nullptr;
    lv_obj_t* mPKeyLift = nullptr;
    lv_obj_t* mPKeyRunT = nullptr;
    lv_obj_t* mPKeyLiftT = nullptr;

    lv_obj_t* mRuns    = nullptr;
    lv_obj_t* mRHeader = nullptr;
    lv_obj_t* mRSel    = nullptr;
    lv_obj_t* mRRow[kRows] = {};
    lv_obj_t* mRDetail = nullptr;

    void showPage(uint8_t p);
    void refreshOverview();
    void refreshProfile();
    void refreshRuns();
};

#endif // SUMMARY_SCREEN_HPP
