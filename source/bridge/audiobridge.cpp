#include "libultraship/bridge/audiobridge.h"

// Console audio output backend. The game's audio engine synthesises interleaved
// stereo s16 frames at 32000 Hz and hands them to AudioPlayerPlayFrame; here we
// queue them to the DSP through libogc's asndlib (ASND), which plays them via an
// interrupt-driven callback. Modelled on the mkst sm64-port GX audio backend.
//
// Ownership/threading: on console the game synthesises audio synchronously on the
// main thread (GameEngine::StartAudioFrame), so play/buffered are called from the
// main thread, while ASND's voice_callback runs in interrupt context. The shared
// buffer bookkeeping is guarded with IRQ_Disable/Restore.

#ifdef GEKKO

#include <malloc.h>
#include <string.h>

#include <asndlib.h>
#include <ogc/cache.h>
#include <ogc/irq.h>


// SM64 (US) generates at most SAMPLES_HIGH (544) samples per audio update, stereo
// 16-bit, and submits up to two updates per video frame. One buffer therefore holds
// 544 * 2 (updates) * 2 (channels) s16. Keep a small ring of them (see Engine.h).
// Sized for the largest update any supported game submits, NOT just SM64's. A frame
// bigger than AUDIO_BUFFER_SIZE is rejected outright below, so a tight value silences
// the game completely: SM64 submits 2 x 544 stereo samples, which fit the original
// 544-sample bound exactly, while Star Fox 64 runs gVIsPerFrame = 2 at up to 752
// samples and so was dropped on every single frame. 5 is the interpreter's ceiling on
// audio frames per update. The ceiling is generous because a frame that runs long
// must be able to refill the whole output buffer in one go; if it cannot, the DSP
// drains before the next top-up and the sound breaks up whenever the game dips below
// its target framerate.
#define AUDIO_SAMPLES_HIGH     752
#define AUDIO_UPDATES_MAX      10
#define AUDIO_BUFFER_COUNT     6
#define AUDIO_BUFFER_SIZE      (AUDIO_SAMPLES_HIGH * 2 * AUDIO_UPDATES_MAX * sizeof(int16_t))
#define AUDIO_BYTES_PER_FRAME  (2 * sizeof(int16_t))

enum AudioBufferState {
    BUFFER_FREE,
    BUFFER_FILLING,
    BUFFER_QUEUED,
    BUFFER_ASND,
};

static void voice_callback(int32_t voice);

static int16_t* s_buffer[AUDIO_BUFFER_COUNT];
static size_t s_buffer_len[AUDIO_BUFFER_COUNT];
static int s_buffer_frames[AUDIO_BUFFER_COUNT];
static enum AudioBufferState s_buffer_state[AUDIO_BUFFER_COUNT];
static int s_queued[AUDIO_BUFFER_COUNT];
static uint8_t s_queue_read;
static uint8_t s_queue_write;
static uint8_t s_queue_count;
static bool s_voice_started;
static bool s_feed_in_progress;
static int s_buffered_frames;
static bool s_initialized;

static int pop_queued_buffer(void) {
    int index = -1;
    u32 level = IRQ_Disable();

    if (s_queue_count > 0) {
        index = s_queued[s_queue_read];
        s_queue_read = (s_queue_read + 1) % AUDIO_BUFFER_COUNT;
        s_queue_count--;
        s_buffer_state[index] = BUFFER_ASND;
    }

    IRQ_Restore(level);
    return index;
}

static void push_queued_buffer_front(int index) {
    u32 level = IRQ_Disable();

    s_queue_read = (s_queue_read + AUDIO_BUFFER_COUNT - 1) % AUDIO_BUFFER_COUNT;
    s_queued[s_queue_read] = index;
    s_queue_count++;
    s_buffer_state[index] = BUFFER_QUEUED;

    IRQ_Restore(level);
}

static bool begin_feed(void) {
    bool can_feed = false;
    u32 level = IRQ_Disable();

    if (!s_feed_in_progress) {
        s_feed_in_progress = true;
        can_feed = true;
    }

    IRQ_Restore(level);
    return can_feed;
}

static void end_feed(void) {
    u32 level = IRQ_Disable();
    s_feed_in_progress = false;
    IRQ_Restore(level);
}

static void release_finished_buffers(void) {
    u32 level = IRQ_Disable();

    if (s_voice_started && ASND_StatusVoice(0) == SND_UNUSED) {
        s_voice_started = false;
    }

    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        if (s_buffer_state[i] != BUFFER_ASND) {
            continue;
        }

        if (!s_voice_started || ASND_TestPointer(0, s_buffer[i]) == 0) {
            s_buffer_state[i] = BUFFER_FREE;
            s_buffered_frames -= s_buffer_frames[i];
            s_buffer_len[i] = 0;
            s_buffer_frames[i] = 0;
        }
    }

    if (s_buffered_frames < 0) {
        s_buffered_frames = 0;
    }

    IRQ_Restore(level);
}

static void feed_queued_buffers(int32_t voice) {
    if (!begin_feed()) {
        return;
    }

    if (!s_voice_started) {
        int index = pop_queued_buffer();
        if (index >= 0) {
            s32 result = ASND_SetVoice(voice, VOICE_STEREO_16BIT, 32000, 0, s_buffer[index],
                                       s_buffer_len[index], MAX_VOLUME, MAX_VOLUME, voice_callback);
            if (result == SND_OK) {
                s_voice_started = true;
            } else {
                push_queued_buffer_front(index);
            }
        }
    }

    while (s_voice_started && ASND_TestVoiceBufferReady(voice) == 1) {
        int index = pop_queued_buffer();
        if (index < 0) {
            break;
        }

        s32 result = ASND_AddVoice(voice, s_buffer[index], s_buffer_len[index]);
        if (result == SND_OK) {
            continue;
        }

        push_queued_buffer_front(index);
        if (result == SND_INVALID) {
            s_voice_started = false;
        }
        break;
    }

    end_feed();
}

static void voice_callback(int32_t voice) {
    feed_queued_buffers(voice);
}

// Allocate the buffer ring and start ASND on first use. Runs on the main thread the
// first time the game submits an audio frame; ASND_Init sets up the DSP/AI interrupt
// so playback continues on its own callback afterwards.
static bool audio_ensure_init(void) {
    if (s_initialized) {
        return true;
    }

    s_queue_read = 0;
    s_queue_write = 0;
    s_queue_count = 0;
    s_voice_started = false;
    s_feed_in_progress = false;
    s_buffered_frames = 0;

    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        s_buffer[i] = (int16_t*)memalign(32, AUDIO_BUFFER_SIZE);
        if (s_buffer[i] == NULL) {
            // Free whatever we managed to allocate so a later retry can succeed.
            for (int j = 0; j < i; j++) {
                free(s_buffer[j]);
                s_buffer[j] = NULL;
            }
            return false;
        }
        memset(s_buffer[i], 0, AUDIO_BUFFER_SIZE);
        DCFlushRange(s_buffer[i], AUDIO_BUFFER_SIZE);
        s_buffer_len[i] = 0;
        s_buffer_frames[i] = 0;
        s_buffer_state[i] = BUFFER_FREE;
    }

    ASND_Init();
    ASND_Pause(0);

    s_initialized = true;
    return true;
}

extern "C" {

int32_t AudioPlayerBuffered() {
    if (!audio_ensure_init()) {
        return 0;
    }

    release_finished_buffers();

    u32 level = IRQ_Disable();
    int result = s_buffered_frames;
    IRQ_Restore(level);

    return result;
}

int32_t AudioPlayerGetDesiredBuffered() {
    // Frames of stereo audio the game aims to keep queued. This is the setpoint for the
    // game's own sample-count feedback, not a hard limit. Keep it at the value SM64 was
    // tuned and verified against - raising it changes which of the game's high/low
    // sample counts it picks every frame.
    return 1100;
}

AudioChannelsSetting GetAudioChannels() {
    return audioStereo;
}

int32_t GetNumAudioChannels() {
    return 2;
}

void AudioPlayerPlayFrame(const uint8_t* buf, size_t len) {
    if (!audio_ensure_init() || len > AUDIO_BUFFER_SIZE) {
        return;
    }



    release_finished_buffers();

    int index = -1;
    u32 level = IRQ_Disable();

    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
        if (s_buffer_state[i] == BUFFER_FREE) {
            index = i;
            s_buffer_state[i] = BUFFER_FILLING;
            break;
        }
    }

    IRQ_Restore(level);

    if (index < 0) {
        // Ring full (the DSP has not drained yet): drop this frame.
        return;
    }

    memcpy(s_buffer[index], buf, len);
    DCFlushRange(s_buffer[index], len);

    level = IRQ_Disable();
    s_queued[s_queue_write] = index;
    s_queue_write = (s_queue_write + 1) % AUDIO_BUFFER_COUNT;
    s_queue_count++;
    s_buffer_len[index] = len;
    s_buffer_frames[index] = len / AUDIO_BYTES_PER_FRAME;
    s_buffered_frames += s_buffer_frames[index];
    s_buffer_state[index] = BUFFER_QUEUED;
    IRQ_Restore(level);

    feed_queued_buffers(0);
}

void SetAudioChannels(AudioChannelsSetting /*channels*/) {
}

} // extern "C"

#else // !GEKKO

// Host/desktop fallback: no console DSP available, drop frames.
extern "C" {

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

#endif // GEKKO
