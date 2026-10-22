/**
 ******************************************************************************
 * @file    Commands.hpp
 * @brief   Service <-> GUI message contract of the Ski Day app.
 *
 * The service owns the day: sensors, run tracking, storm watch, FIT file,
 * cues. The GUI only renders what it is sent and turns button presses into
 * commands.
 *
 *   GUI -> service   DayStart, DayControl, SettingsSave, DemoSpeed
 *   service -> GUI   Settings (on GUI start and on change)
 *                    State    (on every change of day state)
 *                    Live     (once a second while a day is open)
 *                    Event    (run finished, storm, ...: banners)
 *                    DayLog   (runs and altitude profile, for the summary)
 *
 * Every message fits the kernel's 256-byte message block; the day log is
 * shared by pointer and copied by the GUI before it releases the message.
 ******************************************************************************
 */

#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <cstdint>

#include "SDK/Messages/MessageBase.hpp"
#include "SDK/Messages/MessageTypes.hpp"

#include "SkiTypes.hpp"

#pragma pack(push, 4)

namespace CustomMessage
{

// -- Message type ids (application-custom) ------------------------------------

// GUI --> Service
constexpr SDK::MessageType::Type DAY_START     = 0x00000001;
constexpr SDK::MessageType::Type DAY_CONTROL   = 0x00000002;
constexpr SDK::MessageType::Type SETTINGS_SAVE = 0x00000003;
constexpr SDK::MessageType::Type DEMO_SPEED    = 0x00000004;

// Service --> GUI
constexpr SDK::MessageType::Type SETTINGS = 0x00000010;
constexpr SDK::MessageType::Type STATE    = 0x00000011;
constexpr SDK::MessageType::Type LIVE     = 0x00000012;
constexpr SDK::MessageType::Type EVENT    = 0x00000013;
constexpr SDK::MessageType::Type DAY_LOG  = 0x00000014;

enum class Control : uint8_t {
    PAUSE = 0,
    RESUME,
    END_SAVE,   ///< End the day, save the FIT file, show the summary
    DISCARD,    ///< End the day without saving
    HOME,       ///< Leave the summary for the home screen
};

// -- GUI --> Service ----------------------------------------------------------

struct DayStart : public SDK::MessageBase {
    DayStart()
        : SDK::MessageBase(DAY_START)
    {}
};

struct DayControl : public SDK::MessageBase {
    Control cmd;
    DayControl()
        : SDK::MessageBase(DAY_CONTROL), cmd(Control::PAUSE)
    {}
    explicit DayControl(Control c)
        : DayControl()
    {
        cmd = c;
    }
};

struct SettingsSave : public SDK::MessageBase {
    Ski::Settings settings;
    SettingsSave()
        : SDK::MessageBase(SETTINGS_SAVE), settings{}
    {}
    explicit SettingsSave(const Ski::Settings& s)
        : SettingsSave()
    {
        settings = s;
    }
};

/// Change the demo's playback speed while it runs.
struct DemoSpeed : public SDK::MessageBase {
    uint8_t speed;   ///< Ski::DemoSpeed
    DemoSpeed()
        : SDK::MessageBase(DEMO_SPEED), speed(0)
    {}
    explicit DemoSpeed(uint8_t s)
        : DemoSpeed()
    {
        speed = s;
    }
};

// -- Service --> GUI ----------------------------------------------------------

struct Settings : public SDK::MessageBase {
    Ski::Settings settings;
    Ski::Season   season;
    uint8_t       imperial;    ///< watch set to imperial units
    uint8_t       clock12h;    ///< watch set to a 12-hour clock
    Settings()
        : SDK::MessageBase(SETTINGS), settings{}, season{}, imperial(0), clock12h(0)
    {}
};

struct State : public SDK::MessageBase {
    uint8_t dayState;    ///< Ski::DayState
    uint8_t demo;        ///< this day is the demo day
    uint8_t demoSpeed;   ///< Ski::DemoSpeed while the demo runs
    uint8_t saved;       ///< Done: the FIT file was saved
    State()
        : SDK::MessageBase(STATE), dayState(0), demo(0), demoSpeed(0), saved(0)
    {}
};

struct Live : public SDK::MessageBase {
    Ski::LiveData data;
    Live()
        : SDK::MessageBase(LIVE), data{}
    {}
};

struct Event : public SDK::MessageBase {
    Ski::EventData data;
    Event()
        : SDK::MessageBase(EVENT), data{}
    {}
    explicit Event(const Ski::EventData& d)
        : Event()
    {
        data = d;
    }
};

struct DayLog : public SDK::MessageBase {
    const Ski::DayLog* log;   ///< Non-owning; the GUI copies it before releasing the message.
    DayLog()
        : SDK::MessageBase(DAY_LOG), log(nullptr)
    {}
    explicit DayLog(const Ski::DayLog* l)
        : DayLog()
    {
        log = l;
    }
};

} // namespace CustomMessage

#pragma pack(pop)

#endif // COMMANDS_HPP
