/**
 ******************************************************************************
 * @file    HomeScreen.cpp
 ******************************************************************************
 */

#include "gui/screens/HomeScreen.hpp"

#include <cstdio>

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/LVGL/Draw.hpp"

#include "Ski/Format.hpp"
#include "gui/Assets.hpp"
#include "gui/Theme.hpp"

namespace Draw = SDK::LVGL::Draw;
namespace Fmt  = Ski::Fmt;
using SDK::LVGL::Buttons;

HomeScreen::HomeScreen(Model& model, void (*openSettings)(void*), void* ctx)
    : Screen(model), mOpenSettings(openSettings), mCtx(ctx)
{
    // Logo: a ridge line.
    const lv_point_precise_t pts[] = { { 0, 16 }, { 9, 5 }, { 13, 10 }, { 20, 0 }, { 27, 9 }, { 31, 5 }, { 40, 16 } };
    for (int i = 0; i < 7; ++i) {
        mLogoPts[i] = pts[i];
    }
    mLogo = lv_line_create(mRoot);
    lv_line_set_points(mLogo, mLogoPts, 7);
    lv_obj_set_style_line_width(mLogo, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(mLogo, Draw::rgb(Theme::kBrand), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(mLogo, true, LV_PART_MAIN);
    lv_obj_set_pos(mLogo, 100, 16);

    mTitle = Draw::label(mRoot, &poppins_semibold_15, "SKI DAY", 60, 36, 120, LV_TEXT_ALIGN_CENTER, Theme::kBrand);

    // L1 zone (top left): sport.
    mSportCaption = Draw::label(mRoot, &poppins_regular_15, "SPORT", 34, 64, 90, LV_TEXT_ALIGN_LEFT, Theme::kTextDim);
    mSport        = Draw::label(mRoot, &poppins_medium_22, "", 34, 80, 100, LV_TEXT_ALIGN_LEFT, Theme::kText);

    // R1 zone (top right): start.
    mStart = Draw::label(mRoot, &poppins_medium_22, "START", 120, 72, 88, LV_TEXT_ALIGN_RIGHT, Theme::kWarn);

    // Centre: GPS (or demo) status and the storm alert setting.
    mStatusDot = Draw::dot(mRoot, 0, 0, 5, Theme::kWarn);
    mStatus    = Draw::label(mRoot, &poppins_medium_22, "", 30, 116, 180, LV_TEXT_ALIGN_CENTER, Theme::kText);
    mStorm     = Draw::label(mRoot, &poppins_regular_15, "", 30, 146, 180, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);

    // L2 zone (bottom left): settings.
    mSettings = Draw::label(mRoot, &poppins_medium_18, "SETTINGS", 40, 170, 110, LV_TEXT_ALIGN_LEFT, Theme::kText);

    // Bottom: the season.
    mSeason = Draw::label(mRoot, &poppins_regular_15, "", 30, 196, 180, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);

    mButtons = std::make_unique<Buttons>(mRoot);
    mButtons->set(Buttons::WHITE, Buttons::WHITE, Buttons::AMBER, Buttons::WHITE);

    refresh();
}

void HomeScreen::refresh()
{
    const Ski::Settings& s   = mModel.settings();
    const bool           imp = mModel.imperial();
    char                 buf[48];

    lv_label_set_text(mSport, s.sport == static_cast<uint8_t>(Ski::Sport::Snowboard) ? "Snowboard" : "Ski");

    // Status line with a coloured dot to its left.
    uint32_t col;
    if (s.demo) {
        static const char* const kSpeed[] = { "", "1x", "10x", "60x" };
        std::snprintf(buf, sizeof(buf), "Demo day %s", kSpeed[s.demo < 4 ? s.demo : 0]);
        col = Theme::kBrand;
        lv_label_set_text(mStart, "DEMO");
    } else if (mModel.live().gpsFix) {
        std::snprintf(buf, sizeof(buf), "GPS ready");
        col = Theme::kGood;
        lv_label_set_text(mStart, "START");
    } else {
        std::snprintf(buf, sizeof(buf), "Finding GPS");
        col = Theme::kWarn;
        lv_label_set_text(mStart, "START");
    }
    lv_label_set_text(mStatus, buf);
    lv_point_t size;
    lv_text_get_size(&size, buf, &poppins_medium_22, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_pos(mStatusDot, 120 - size.x / 2 - 16, 128);
    lv_obj_set_style_bg_color(mStatusDot, Draw::rgb(col), LV_PART_MAIN);

    if (s.stormHpa) {
        char thr[16];
        Fmt::stormThreshold(thr, sizeof(thr), s.stormHpa, imp);
        std::snprintf(buf, sizeof(buf), "Storm alert %s", thr);
    } else {
        std::snprintf(buf, sizeof(buf), "Storm alert off");
    }
    lv_label_set_text(mStorm, buf);

    const Ski::Season& se = mModel.season();
    if (se.days > 0) {
        char v[16];
        Ski::Fmt::vertical(v, sizeof(v), static_cast<int32_t>(se.verticalM), imp);
        std::snprintf(buf, sizeof(buf), "Season %u day%s \xC2\xB7 %s %s", se.days, se.days == 1 ? "" : "s", v,
                      Ski::Fmt::verticalUnit(imp));
    } else {
        std::snprintf(buf, sizeof(buf), "Season starts today");
    }
    lv_label_set_text(mSeason, buf);
}

void HomeScreen::onClick(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    switch (code) {
        case Btn::L1: {
            Ski::Settings s = mModel.settings();
            s.sport = s.sport == static_cast<uint8_t>(Ski::Sport::Ski) ? static_cast<uint8_t>(Ski::Sport::Snowboard)
                                                                       : static_cast<uint8_t>(Ski::Sport::Ski);
            mModel.saveSettings(s);
            refresh();
        } break;
        case Btn::L2:
            if (mOpenSettings) {
                mOpenSettings(mCtx);
            }
            break;
        case Btn::R1:
            mModel.startDay();
            break;
        case Btn::R2:
            mModel.exitApp();
            break;
        default:
            break;
    }
}
