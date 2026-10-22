/**
 ******************************************************************************
 * @file    SettingsScreen.cpp
 ******************************************************************************
 */

#include "gui/screens/SettingsScreen.hpp"

#include <cstdio>

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/LVGL/Draw.hpp"

#include "gui/Assets.hpp"
#include "gui/Theme.hpp"

namespace Draw = SDK::LVGL::Draw;
using SDK::LVGL::Buttons;

SettingsScreen::SettingsScreen(Model& model, void (*back)(void*), void* ctx)
    : Screen(model), mBack(back), mCtx(ctx)
{
    mTitle   = Draw::label(mRoot, &poppins_semibold_15, "SETTINGS", 40, 26, 160, LV_TEXT_ALIGN_CENTER, Theme::kBrand);
    mPrev    = Draw::label(mRoot, &poppins_medium_18, "", 30, 56, 180, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);
    mBand    = Draw::box(mRoot, 14, 86, 212, 76, Theme::kSelect, 20);
    mCaption = Draw::label(mRoot, &poppins_regular_15, "", 30, 90, 180, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);
    mValue   = Draw::label(mRoot, &poppins_semibold_30, "", 20, 106, 200, LV_TEXT_ALIGN_CENTER, Theme::kWarn);
    mHint    = Draw::label(mRoot, &poppins_regular_15, "", 20, 140, 200, LV_TEXT_ALIGN_CENTER, Theme::kText);
    mNext    = Draw::label(mRoot, &poppins_medium_18, "", 30, 170, 180, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);
    for (uint8_t i = 0; i < ROWS; ++i) {
        mDots[i] = Draw::dot(mRoot, 104 + i * 8, 206, 2, Theme::kTrack);
    }

    mButtons = std::make_unique<Buttons>(mRoot);
    mButtons->set(Buttons::WHITE, Buttons::WHITE, Buttons::AMBER, Buttons::WHITE);
    refresh();
}

void SettingsScreen::onShow()
{
    mSel = 0;
    refresh();
}

const char* SettingsScreen::caption(uint8_t row)
{
    switch (row) {
        case SPORT:      return "Sport";
        case STORM:      return "Storm alert";
        case RUN_ALERTS: return "Run alerts";
        case SOUND:      return "Sound";
        case DEMO:       return "Demo day";
        default:         return "";
    }
}

void SettingsScreen::valueText(uint8_t row, char* buf, size_t n) const
{
    const Ski::Settings& s = mModel.settings();
    switch (row) {
        case SPORT:
            std::snprintf(buf, n, "%s", s.sport == static_cast<uint8_t>(Ski::Sport::Snowboard) ? "Snowboard" : "Ski");
            break;
        case STORM:
            if (s.stormHpa) {
                std::snprintf(buf, n, "%u hPa", s.stormHpa);
            } else {
                std::snprintf(buf, n, "Off");
            }
            break;
        case RUN_ALERTS:
            std::snprintf(buf, n, "%s", s.runAlerts ? "On" : "Off");
            break;
        case SOUND: {
            static const char* const kMode[] = { "Beep + buzz", "Buzz only", "Beep only" };
            std::snprintf(buf, n, "%s", kMode[s.alertMode < 3 ? s.alertMode : 0]);
        } break;
        case DEMO: {
            static const char* const kDemo[] = { "Off", "1x", "10x", "60x" };
            std::snprintf(buf, n, "%s", kDemo[s.demo < 4 ? s.demo : 0]);
        } break;
        default:
            buf[0] = '\0';
    }
}

void SettingsScreen::refresh()
{
    char buf[48], v[24];
    const Ski::Settings& s = mModel.settings();

    const uint8_t prev = static_cast<uint8_t>((mSel + ROWS - 1) % ROWS);
    const uint8_t next = static_cast<uint8_t>((mSel + 1) % ROWS);
    valueText(prev, v, sizeof(v));
    std::snprintf(buf, sizeof(buf), "%s  %s", caption(prev), v);
    lv_label_set_text(mPrev, buf);
    valueText(next, v, sizeof(v));
    std::snprintf(buf, sizeof(buf), "%s  %s", caption(next), v);
    lv_label_set_text(mNext, buf);

    lv_label_set_text(mCaption, caption(mSel));
    valueText(mSel, v, sizeof(v));
    lv_label_set_text(mValue, v);

    const char* hint = "";
    switch (mSel) {
        case SPORT:      hint = "Sport in the FIT file"; break;
        case STORM:      hint = s.stormHpa ? "Pressure fall in 3 h" : "No storm warnings"; break;
        case RUN_ALERTS: hint = s.runAlerts ? "Buzz after each run" : "Quiet, except storms"; break;
        case SOUND:      hint = "How alerts play"; break;
        case DEMO:       hint = s.demo ? "Plays a simulated day" : "Try it without snow"; break;
        default: break;
    }
    lv_label_set_text(mHint, hint);

    for (uint8_t i = 0; i < ROWS; ++i) {
        lv_obj_set_style_bg_color(mDots[i], Draw::rgb(i == mSel ? Theme::kText : Theme::kTrack), LV_PART_MAIN);
    }
}

void SettingsScreen::change(uint8_t row)
{
    Ski::Settings s = mModel.settings();
    switch (row) {
        case SPORT:
            s.sport = static_cast<uint8_t>((s.sport + 1u) % static_cast<uint8_t>(Ski::Sport::COUNT));
            break;
        case STORM: {
            uint8_t i = 0;
            for (uint8_t k = 0; k < sizeof(Ski::kStormChoices); ++k) {
                if (Ski::kStormChoices[k] == s.stormHpa) {
                    i = k;
                }
            }
            s.stormHpa = Ski::kStormChoices[(i + 1u) % sizeof(Ski::kStormChoices)];
        } break;
        case RUN_ALERTS:
            s.runAlerts = s.runAlerts ? 0 : 1;
            break;
        case SOUND:
            s.alertMode = static_cast<uint8_t>((s.alertMode + 1u) % static_cast<uint8_t>(Ski::AlertMode::COUNT));
            break;
        case DEMO:
            s.demo = static_cast<uint8_t>((s.demo + 1u) % static_cast<uint8_t>(Ski::DemoSpeed::COUNT));
            break;
        default:
            break;
    }
    mModel.saveSettings(s);
}

void SettingsScreen::onClick(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    switch (code) {
        case Btn::L1: mSel = static_cast<uint8_t>((mSel + ROWS - 1) % ROWS); refresh(); break;
        case Btn::L2: mSel = static_cast<uint8_t>((mSel + 1) % ROWS); refresh(); break;
        case Btn::R1: change(mSel); refresh(); break;
        case Btn::R2:
            if (mBack) {
                mBack(mCtx);
            }
            break;
        default: break;
    }
}
