#pragma once

#include <stdint.h>
#include <stddef.h>

#include "ship/audio/AudioChannelsSetting.h"

#ifdef __cplusplus
extern "C" {
#endif

// C audio bridge the game's audio thread calls. libultragx has no audio output
// backend yet (a libogc AESND/AX player is a later milestone), so these are stubs:
// the game runs its synthesis and hands frames to AudioPlayerPlayFrame, which drops
// them for now. Channel selection reports stereo.
int32_t AudioPlayerBuffered();
int32_t AudioPlayerGetDesiredBuffered();
AudioChannelsSetting GetAudioChannels();
int32_t GetNumAudioChannels();
void AudioPlayerPlayFrame(const uint8_t* buf, size_t len);
void SetAudioChannels(AudioChannelsSetting channels);

#ifdef __cplusplus
}
#endif
