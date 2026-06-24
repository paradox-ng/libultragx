#include "libultraship/bridge/audiobridge.h"

extern "C" {

// No console audio output backend yet; report a small steady buffer so the game's
// audio thread paces itself and does not busy-spin, and drop submitted frames.
int32_t AudioPlayerBuffered() {
    return 0;
}

int32_t AudioPlayerGetDesiredBuffered() {
    return 1100;
}

AudioChannelsSetting GetAudioChannels() {
    return audioStereo;
}

int32_t GetNumAudioChannels() {
    return 2;
}

void AudioPlayerPlayFrame(const uint8_t* /*buf*/, size_t /*len*/) {
}

void SetAudioChannels(AudioChannelsSetting /*channels*/) {
}
}
