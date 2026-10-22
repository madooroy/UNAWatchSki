/**
 ******************************************************************************
 * @file    Ui.cpp
 ******************************************************************************
 */

#include "gui/Ui.hpp"

#include <cstdio>

#include "SDK/GUI/LVGL/Draw.hpp"

#include "Ski/Format.hpp"
#include "gui/Assets.hpp"
#include "gui/Theme.hpp"

namespace Draw = SDK::LVGL::Draw;
namespace Fmt  = Ski::Fmt;
using Ski::DayState;

namespace
{
const char* const kDot = " \xC2\xB7 ";
}

Ui::Ui(Model& model)
    : mModel(model)
    , mHome(model, &Ui::openSettings, this)
    , mSettings(model, &Ui::closeSettings, this)
    , mDay(model)
    , mSummary(model)
{
    buildOverlays();
    Screen::setRouter(this);
    mModel.bind(this);
    show(mHome);
}

void Ui::openSettings(void* ctx)
{
    auto* self        = static_cast<Ui*>(ctx);
    self->mInSettings = true;
    self->show(self->mSettings);
}

void Ui::closeSettings(void* ctx)
{
    auto* self        = static_cast<Ui*>(ctx);
    self->mInSettings = false;
    self->show(self->screenForState());
}

Screen& Ui::screenForState()
{
    switch (mModel.dayState()) {
        case DayState::Active:
        case DayState::Paused: return mDay;
        case DayState::Done:   return mSummary;
        default:               return mInSettings ? static_cast<Screen&>(mSettings) : static_cast<Screen&>(mHome);
    }
}

void Ui::show(Screen& s)
{
    if (mActive != &s) {
        mActive = &s;
        s.onShow();
        lv_screen_load(s.root());
    }
    s.refresh();
}

// -- Model events ---------------------------------------------------------------

void Ui::onFrame()
{
    if (mBannerShown && static_cast<int32_t>(mModel.now() - mBannerUntil) >= 0) {
        hideBanner();
    }
}

void Ui::onSettings()
{
    if (mActive) {
        mActive->refresh();
    }
}

void Ui::onState()
{
    if (mModel.dayState() != DayState::Idle) {
        mInSettings = false;
    }
    if (mModel.dayState() == DayState::Done || mModel.dayState() == DayState::Idle) {
        hideBanner();
        hideStorm();
    }
    show(screenForState());
}

void Ui::onLive()
{
    if (mActive == &mHome || mActive == &mDay) {
        mActive->refresh();
    }
}

void Ui::onLog()
{
    mSummary.refresh();
}

void Ui::onEvent()
{
    const Ski::EventData& e   = mModel.event();
    const bool            imp = mModel.imperial();
    char t[32], big[24], line[64];

    switch (static_cast<Ski::EventKind>(e.kind)) {
        case Ski::EventKind::RunEnded: {
            if (!mModel.settings().runAlerts) {
                break;
            }
            std::snprintf(t, sizeof(t), "RUN %u", e.run.number);
            Fmt::vertical(big, sizeof(big), e.run.verticalM, imp);
            char d[16], s[16];
            Fmt::distance(d, sizeof(d), e.run.distanceM, imp);
            Fmt::speed(s, sizeof(s), e.run.maxSpeedCms, imp);
            std::snprintf(line, sizeof(line), "%s %s%s%s %s top", d, Fmt::distanceUnit(imp), kDot, s, Fmt::speedUnit(imp));
            const char* note = e.bestSpeed ? "New top speed" : (e.bestVert ? "Biggest run today" : "");
            showBanner(t, Theme::kRun, big, Fmt::verticalUnit(imp), line, note, 7000);
        } break;
        case Ski::EventKind::Storm:
            showStorm(e.rateCenti);
            break;
        case Ski::EventKind::FirstFix:
            if (mModel.dayState() == DayState::Active) {
                showBanner("GPS READY", Theme::kGood, "", "", "Speed and distance on", "", 3000);
            }
            break;
        case Ski::EventKind::LowBattery:
            std::snprintf(big, sizeof(big), "%u", e.value);
            showBanner("BATTERY", Theme::kWarn, big, "%", "The day is saved as you go", "", 5000);
            break;
        default:
            break;
    }
}

bool Ui::routeKey(uint8_t)
{
    if (mStormShown) {
        hideStorm();
        return true;
    }
    if (mBannerShown) {
        hideBanner();
        return true;
    }
    return false;
}

// -- Overlays -------------------------------------------------------------------

void Ui::buildOverlays()
{
    lv_obj_t* top = lv_layer_top();

    // Banner: a rounded card across the middle of the face.
    mBanner = Draw::container(top, 12, 58, 216, 124);
    lv_obj_set_style_bg_color(mBanner, Draw::rgb(SDK::GUI::Color::BLACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mBanner, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(mBanner, 26, LV_PART_MAIN);
    lv_obj_set_style_border_width(mBanner, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(mBanner, Draw::rgb(Theme::kRun), LV_PART_MAIN);
    mBTitle = Draw::label(mBanner, &poppins_semibold_15, "", 8, 8, 200, LV_TEXT_ALIGN_CENTER, Theme::kRun);
    mBBig   = Draw::label(mBanner, &poppins_semibold_48, "", 8, 24, 200, LV_TEXT_ALIGN_CENTER, Theme::kText);
    mBUnit  = Draw::label(mBanner, &poppins_medium_18, "", 0, 0, 40, LV_TEXT_ALIGN_LEFT, Theme::kTextDim);
    mBLine  = Draw::label(mBanner, &poppins_regular_15, "", 8, 78, 200, LV_TEXT_ALIGN_CENTER, Theme::kText);
    mBNote  = Draw::label(mBanner, &poppins_semibold_15, "", 8, 98, 200, LV_TEXT_ALIGN_CENTER, Theme::kBest);
    Draw::setHidden(mBanner, true);

    // Storm: the whole face, amber.
    mStorm = Draw::container(top, 0, 0, 240, 240);
    lv_obj_set_style_bg_color(mStorm, Draw::rgb(Theme::kStormBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mStorm, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(mStorm, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    const lv_point_precise_t bolt[6] = { { 12, 0 }, { 2, 16 }, { 10, 16 }, { 4, 30 }, { 20, 11 }, { 11, 11 } };
    for (int i = 0; i < 6; ++i) {
        mBolt[i] = bolt[i];
    }
    lv_obj_t* b = lv_line_create(mStorm);
    lv_line_set_points(b, mBolt, 6);
    lv_obj_set_style_line_width(b, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(b, Draw::rgb(Theme::kStormFg), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(b, true, LV_PART_MAIN);
    lv_obj_set_pos(b, 109, 18);

    Draw::label(mStorm, &poppins_semibold_15, "STORM WARNING", 30, 54, 180, LV_TEXT_ALIGN_CENTER, Theme::kStormFg);
    mSRate = Draw::label(mStorm, &poppins_semibold_48, "", 20, 72, 200, LV_TEXT_ALIGN_CENTER, Theme::kStormFg);
    mSUnit = Draw::label(mStorm, &poppins_regular_15, "", 30, 124, 180, LV_TEXT_ALIGN_CENTER, Theme::kStormDim);
    Draw::label(mStorm, &poppins_medium_18, "Pressure falling fast", 20, 146, 200, LV_TEXT_ALIGN_CENTER, Theme::kStormDim);
    Draw::label(mStorm, &poppins_regular_15, "Weather may turn soon", 30, 170, 180, LV_TEXT_ALIGN_CENTER, Theme::kStormDim);
    Draw::label(mStorm, &poppins_semibold_15, "ANY BUTTON", 50, 200, 140, LV_TEXT_ALIGN_CENTER, Theme::kStormFg);
    Draw::setHidden(mStorm, true);
}

void Ui::showBanner(const char* title, uint32_t titleColor, const char* big, const char* unit, const char* line,
                    const char* note, uint32_t ms)
{
    lv_label_set_text(mBTitle, title);
    lv_obj_set_style_text_color(mBTitle, Draw::rgb(titleColor), LV_PART_MAIN);
    lv_obj_set_style_border_color(mBanner, Draw::rgb(note && *note ? Theme::kBest : titleColor), LV_PART_MAIN);
    lv_label_set_text(mBBig, big);
    lv_point_t sz;
    lv_text_get_size(&sz, big, &poppins_semibold_48, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_label_set_text(mBUnit, unit);
    lv_obj_set_pos(mBUnit, 108 + sz.x / 2 + 4, 52);
    lv_label_set_text(mBLine, line);
    lv_obj_set_y(mBLine, (big && *big) ? 78 : 44);
    lv_label_set_text(mBNote, note ? note : "");
    mBannerUntil = mModel.now() + ms;
    mBannerShown = true;
    Draw::setHidden(mBanner, false);
}

void Ui::hideBanner()
{
    mBannerShown = false;
    Draw::setHidden(mBanner, true);
}

void Ui::showStorm(int16_t rateCenti)
{
    const bool imp = mModel.imperial();
    char       buf[24], unit[32];
    Fmt::pressureRate(buf, sizeof(buf), rateCenti, imp);
    lv_label_set_text(mSRate, buf);
    std::snprintf(unit, sizeof(unit), "%s in 3 hours", Fmt::pressureUnit(imp));
    lv_label_set_text(mSUnit, unit);
    hideBanner();
    mStormShown = true;
    Draw::setHidden(mStorm, false);
}

void Ui::hideStorm()
{
    mStormShown = false;
    Draw::setHidden(mStorm, true);
}
