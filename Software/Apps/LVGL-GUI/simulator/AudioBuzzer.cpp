/**
 ******************************************************************************
 * @file    AudioBuzzer.cpp
 ******************************************************************************
 */

#include "AudioBuzzer.hpp"

#include <SDL.h>

#include <cstdlib>
#include <vector>

#define LOG_MODULE_PRX      "Sim.Buzzer"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

namespace
{
constexpr uint16_t kDefaultHz = 2700;   // typical piezo resonance
}

AudioBuzzer::AudioBuzzer()
{
    const char* mute = std::getenv("UNA_SIM_MUTE");
    if (mute && mute[0] == '1') {
        return;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        LOG_INFO("no audio (%s): beeps are logged only\n", SDL_GetError());
        return;
    }
    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    want.freq     = mRate;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 512;
    mDevice       = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (mDevice == 0) {
        LOG_INFO("no audio device (%s): beeps are logged only\n", SDL_GetError());
        return;
    }
    mRate = have.freq;
    SDL_PauseAudioDevice(mDevice, 0);
}

AudioBuzzer::~AudioBuzzer()
{
    if (mDevice) {
        SDL_CloseAudioDevice(mDevice);
    }
}

bool AudioBuzzer::play()
{
    Note n;
    n.time  = 100;
    n.level = 2;
    return play(&n, 1);
}

bool AudioBuzzer::play(Note note)
{
    return play(&note, 1);
}

bool AudioBuzzer::play(const Note melody[], uint8_t len)
{
    if (len == 0) {
        stop();
        return true;
    }

    uint32_t total = 0;
    for (uint8_t i = 0; i < len; ++i) {
        total += melody[i].time;
        LOG_INFO("tone %u ms %u Hz level %u\n", melody[i].time,
                 melody[i].level ? (melody[i].frequency ? melody[i].frequency : kDefaultHz) : 0,
                 melody[i].level);
    }
    if (!mDevice) {
        return true;
    }

    std::vector<int16_t> pcm;
    pcm.reserve(static_cast<size_t>(total) * mRate / 1000 + 1);
    for (uint8_t i = 0; i < len; ++i) {
        const Note&    n       = melody[i];
        const uint32_t samples = n.time * static_cast<uint32_t>(mRate) / 1000;
        const uint32_t hz      = n.frequency >= 500 ? n.frequency : kDefaultHz;
        const int16_t  amp     = static_cast<int16_t>(n.level >= 3 ? 9000 : n.level == 2 ? 6000 : n.level == 1 ? 3000 : 0);
        const uint32_t period  = static_cast<uint32_t>(mRate) / hz;
        const uint32_t fade    = static_cast<uint32_t>(mRate) / 500;  // 2 ms ramps, no clicks
        for (uint32_t s = 0; s < samples; ++s) {
            int32_t v = (period && (s % period) < period / 2) ? amp : -amp;
            if (s < fade) {
                v = v * static_cast<int32_t>(s) / static_cast<int32_t>(fade);
            } else if (samples - s < fade) {
                v = v * static_cast<int32_t>(samples - s) / static_cast<int32_t>(fade);
            }
            pcm.push_back(static_cast<int16_t>(v));
        }
    }
    // A new pattern replaces whatever is still sounding, as on the watch.
    SDL_ClearQueuedAudio(mDevice);
    SDL_QueueAudio(mDevice, pcm.data(), static_cast<uint32_t>(pcm.size() * sizeof(int16_t)));
    return true;
}

bool AudioBuzzer::isPlaying()
{
    return mDevice && SDL_GetQueuedAudioSize(mDevice) > 0;
}

void AudioBuzzer::stop()
{
    if (mDevice) {
        SDL_ClearQueuedAudio(mDevice);
    }
}
