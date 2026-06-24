#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "ship/audio/AudioChannelsSetting.h"

namespace Ship {

// Which platform audio output path is in use. libultragx has no real backend yet,
// so it reports NUL; the enumerators exist for a game's audio-settings menu.
enum class AudioBackend { WASAPI, SDL, COREAUDIO, NUL };

// Audio device configuration a game passes to Context::InitAudio.
struct AudioSettings {
    int SampleRate = 32000;
    int SampleLength = 512;
    int DesiredBuffered = 1100;
};

// The platform audio sink. No output on console yet (a libogc AESND/AX player is a
// later milestone); the player exists so a game's audio wiring compiles and links.
class AudioPlayer {
  public:
    bool Init() { return false; }
    bool IsInitialized() { return false; }
    int32_t Buffered() { return 0; }
    int32_t GetDesiredBuffered() { return 1100; }
    int32_t GetSampleRate() const { return 32000; }
    void Play(const uint8_t* /*buf*/, uint32_t /*len*/) {}
};

// Owns the AudioPlayer and channel/backend selection. All inert on console.
class Audio {
  public:
    std::shared_ptr<AudioPlayer> GetAudioPlayer() {
        if (mPlayer == nullptr) {
            mPlayer = std::make_shared<AudioPlayer>();
        }
        return mPlayer;
    }
    AudioBackend GetCurrentAudioBackend() {
        return AudioBackend::NUL;
    }
    std::shared_ptr<std::vector<AudioBackend>> GetAvailableAudioBackends() {
        return std::make_shared<std::vector<AudioBackend>>(std::vector<AudioBackend>{ AudioBackend::NUL });
    }
    void SetCurrentAudioBackend(AudioBackend /*backend*/) {}

  private:
    std::shared_ptr<AudioPlayer> mPlayer;
};

} // namespace Ship
