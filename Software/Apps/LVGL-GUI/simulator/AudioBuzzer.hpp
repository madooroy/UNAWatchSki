/**
 ******************************************************************************
 * @file    AudioBuzzer.hpp
 * @brief   Simulator-only buzzer that plays the watch's beep patterns through
 *          the PC speakers (square wave, like the watch's piezo), so a demo
 *          of the simulator has the same audible cues as the watch.
 *
 * Falls back to logging, like the SDK's mock buzzer, when no audio device is
 * available (CI, headless runs). Set UNA_SIM_MUTE=1 to silence it.
 ******************************************************************************
 */

#ifndef AUDIO_BUZZER_HPP
#define AUDIO_BUZZER_HPP

#include <cstdint>

#include "SDK/Interfaces/IBuzzer.hpp"

class AudioBuzzer : public SDK::Interface::IBuzzer
{
public:
    AudioBuzzer();
    ~AudioBuzzer() override;

    bool play() override;
    bool play(Note note) override;
    bool play(const Note melody[], uint8_t len) override;
    bool isPlaying() override;
    void stop() override;

private:
    uint32_t mDevice = 0;   // SDL_AudioDeviceID, 0 = no audio
    int      mRate   = 44100;
};

#endif // AUDIO_BUZZER_HPP
