/**
 ******************************************************************************
 * @file    Model.hpp
 * @brief   GUI-side mirror of the service's day, and the GUI's channel back
 *          to the service.
 *
 * The service is the single source of truth: the model stores the last
 * settings, state, live snapshot, event and day log it was sent, and turns
 * button presses into commands.
 ******************************************************************************
 */

#ifndef MODEL_HPP
#define MODEL_HPP

#include <cstdint>

#include "SDK/Interfaces/ICustomMessageHandler.hpp"
#include "SDK/Interfaces/IGuiLifeCycleCallback.hpp"
#include "SDK/Kernel/Kernel.hpp"

#include "Commands.hpp"
#include "SkiTypes.hpp"

class ModelListener;

class Model : public SDK::Interface::IGuiLifeCycleCallback,
              public SDK::Interface::ICustomMessageHandler
{
public:
    Model();

    void bind(ModelListener* listener) { mListener = listener; }

    // -- Commands to the service ----------------------------------------------
    void startDay();
    void control(CustomMessage::Control cmd);
    void saveSettings(const Ski::Settings& s);
    void setDemoSpeed(Ski::DemoSpeed s);
    void exitApp();

    // -- Mirrored state -------------------------------------------------------
    const Ski::Settings& settings() const { return mSettings; }
    const Ski::Season&   season() const   { return mSeason; }
    bool                 imperial() const { return mImperial; }
    bool                 clock12h() const { return mClock12h; }
    bool                 haveSettings() const { return mHaveSettings; }

    Ski::DayState  dayState() const  { return mDayState; }
    bool           demo() const      { return mDemo; }
    Ski::DemoSpeed demoSpeed() const { return mDemoSpeed; }
    bool           saved() const     { return mSaved; }

    const Ski::LiveData&  live() const  { return mLive; }
    const Ski::EventData& event() const { return mEvent; }
    const Ski::DayLog&    log() const   { return mLog; }

    uint32_t now() const { return mKernel.sys.getTimeMs(); }

protected:
    ModelListener*     mListener = nullptr;
    const SDK::Kernel& mKernel;

    Ski::Settings  mSettings;
    Ski::Season    mSeason;
    bool           mImperial     = false;
    bool           mClock12h     = false;
    bool           mHaveSettings = false;
    Ski::DayState  mDayState     = Ski::DayState::Idle;
    bool           mDemo         = false;
    Ski::DemoSpeed mDemoSpeed    = Ski::DemoSpeed::Off;
    bool           mSaved        = false;
    Ski::LiveData  mLive;
    Ski::EventData mEvent;
    Ski::DayLog    mLog;

    // IGuiLifeCycleCallback
    void onStart() override;
    void onFrame() override;
    void onResume() override;
    void onSuspend() override;
    void onStop() override;

    // ICustomMessageHandler
    bool customMessageHandler(SDK::MessageBase* msg) override;
};

#endif // MODEL_HPP
