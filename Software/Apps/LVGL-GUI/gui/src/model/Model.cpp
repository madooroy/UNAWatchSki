/**
 ******************************************************************************
 * @file    Model.cpp
 * @brief   GUI-side mirror of the day and channel to the service.
 ******************************************************************************
 */

#include "gui/model/Model.hpp"
#include "gui/model/ModelListener.hpp"

#include "SDK/Kernel/KernelProviderGUI.hpp"
#include "SDK/Messages/MessageGuard.hpp"
#include "SDK/Port/LVGL/LvglPort.hpp"

#define LOG_MODULE_PRX      "Model"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

Model::Model()
    : mKernel(SDK::KernelProviderGUI::GetInstance().getKernel())
{
    SDK::LVGL::Port::GetInstance().setAppLifeCycleCallback(this);
    SDK::LVGL::Port::GetInstance().setCustomMessageHandler(this);
#if defined(SIMULATOR)
    LOG_INFO("Ski Day simulator. Keys: 1=L1  2=L2  3=R1  4=R2, 5 = raise wrist, Esc = close\n");
#endif
}

// -- Commands -----------------------------------------------------------------

void Model::startDay()
{
    SDK::send_msg<CustomMessage::DayStart>(mKernel);
}

void Model::control(CustomMessage::Control cmd)
{
    SDK::send_msg<CustomMessage::DayControl>(mKernel, cmd);
}

void Model::saveSettings(const Ski::Settings& s)
{
    mSettings = s;   // show it at once; the service echoes it back
    SDK::send_msg<CustomMessage::SettingsSave>(mKernel, s);
}

void Model::setDemoSpeed(Ski::DemoSpeed s)
{
    mDemoSpeed = s;
    SDK::send_msg<CustomMessage::DemoSpeed>(mKernel, static_cast<uint8_t>(s));
}

void Model::exitApp()
{
    LOG_INFO("Exit\n");
    SDK::LVGL::Port::GetInstance().setAppLifeCycleCallback(nullptr);
    SDK::LVGL::Port::GetInstance().setCustomMessageHandler(nullptr);
    mKernel.sys.exit();
}

// -- Lifecycle ----------------------------------------------------------------

void Model::onStart()   {}
void Model::onResume()  {}
void Model::onSuspend() {}
void Model::onStop()    {}

void Model::onFrame()
{
    if (mListener) {
        mListener->onFrame();
    }
}

// -- Service messages ---------------------------------------------------------

bool Model::customMessageHandler(SDK::MessageBase* msg)
{
    switch (msg->getType()) {
        case CustomMessage::SETTINGS: {
            auto* m       = static_cast<CustomMessage::Settings*>(msg);
            mSettings     = m->settings;
            mSeason       = m->season;
            mImperial     = m->imperial != 0;
            mClock12h     = m->clock12h != 0;
            mHaveSettings = true;
            if (mListener) {
                mListener->onSettings();
            }
        } break;

        case CustomMessage::STATE: {
            auto* m    = static_cast<CustomMessage::State*>(msg);
            mDayState  = static_cast<Ski::DayState>(m->dayState);
            mDemo      = m->demo != 0;
            mDemoSpeed = static_cast<Ski::DemoSpeed>(m->demoSpeed);
            mSaved     = m->saved != 0;
            if (mListener) {
                mListener->onState();
            }
        } break;

        case CustomMessage::LIVE:
            mLive = static_cast<CustomMessage::Live*>(msg)->data;
            if (mListener) {
                mListener->onLive();
            }
            break;

        case CustomMessage::EVENT:
            mEvent = static_cast<CustomMessage::Event*>(msg)->data;
            if (mListener) {
                mListener->onEvent();
            }
            break;

        case CustomMessage::DAY_LOG: {
            auto* m = static_cast<CustomMessage::DayLog*>(msg);
            if (m->log) {
                mLog = *m->log;   // copy before the message is released
            }
            if (mListener) {
                mListener->onLog();
            }
        } break;

        default:
            break;
    }
    return true;
}
