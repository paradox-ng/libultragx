// N64 OS (libultra) compatibility shim for GameCube/Wii.
//
// The decompiled game still calls a handful of libultra os* functions - mostly the
// audio thread's message queue, plus cache, time, controller, eeprom, motor and VI
// entry points. libultragx drives the loop synchronously (no N64 RCP or threads), so
// these are implemented minimally: message queues are plain ring buffers, cache ops
// map to libogc, and the hardware-specific paths (controller/eeprom/motor/VI) are
// inert here (real input goes through the ControlDeck, saves through the port).

#include "libultraship/libultra/types.h"
#include "libultraship/libultra/message.h"
#include "libultraship/libultra/os.h"
#include "libultraship/libultra/controller.h"
#include "libultraship/libultra/pfs.h"

// Controller input is owned by the ControlDeck (reads the live GC/Wii pad). Both
// headers are pure ship-side C++ - no <ogc/gu.h>, so no GX-Mtx collision here.
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

#include <cstdint>
#include <cstring>

// <ogc/cache.h> is clean (no <ogc/gu.h>, so no Mtx collision with the libultra Mtx)
// and provides the real DCFlushRange/DCInvalidateRange cache ops.
#include <ogc/cache.h>

namespace {
// Read the 64-bit PPC time base directly, rather than libogc's gettime() whose
// header (<ogc/lwp_watchdog.h> -> ogcsys -> gccore -> gu.h) would collide with the
// N64 Mtx.
inline uint64_t ReadTimeBase() {
    uint32_t hi, lo, tmp;
    do {
        __asm__ volatile("mftbu %0" : "=r"(hi));
        __asm__ volatile("mftb %0" : "=r"(lo));
        __asm__ volatile("mftbu %0" : "=r"(tmp));
    } while (hi != tmp);
    return ((uint64_t)hi << 32) | lo;
}
} // namespace

extern "C" {

// ---- Message queues: N64 ring-buffer semantics (non-blocking on console) ----

void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msgBuf, int32_t count) {
    if (mq == nullptr) {
        return;
    }
    mq->mtqueue = nullptr;
    mq->fullqueue = nullptr;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msgBuf;
}

int32_t osSendMesg(OSMesgQueue* mq, OSMesg msg, int32_t /*flag*/) {
    if (mq == nullptr || mq->validCount >= mq->msgCount) {
        return -1; // full; never blocks on console
    }
    int32_t index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = msg;
    mq->validCount++;
    return 0;
}

int32_t osJamMesg(OSMesgQueue* mq, OSMesg msg, int32_t /*flag*/) {
    if (mq == nullptr || mq->validCount >= mq->msgCount) {
        return -1;
    }
    mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
    mq->msg[mq->first] = msg;
    mq->validCount++;
    return 0;
}

int32_t osRecvMesg(OSMesgQueue* mq, OSMesg* msg, int32_t /*flag*/) {
    if (mq == nullptr || mq->validCount == 0) {
        return -1; // empty; never blocks on console
    }
    if (msg != nullptr) {
        *msg = mq->msg[mq->first];
    }
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    return 0;
}

void osSetEventMesg(OSEvent /*event*/, OSMesgQueue* /*mq*/, OSMesg /*msg*/) {
    // No RCP events on console.
}

// ---- Address translation: CPU virtual -> physical (DMA) address ----

uint32_t osVirtualToPhysical(void* vaddr) {
    // The software renderer never issues a real RCP DMA, so this must return a
    // usable CPU pointer, not a physical offset. The Goddard engine is the only
    // caller: it feeds osVirtualToPhysical(ram_ptr) straight into gSPVertex /
    // gSPMatrix / gSPLight in a display list it builds in RAM, and the
    // interpreter's SegAddr() treats any bit-0-clear address as a raw pointer.
    // Masking off the 0x80000000 cache-region bit here would corrupt every one
    // of those reads, so pass the pointer through unchanged (matches the
    // sm64-port NO_SEGMENTED_MEMORY behaviour).
    return (uint32_t)(uintptr_t)vaddr;
}

// ---- Time ----

uint64_t osGetTime(void) {
    return ReadTimeBase();
}

// ---- Data cache (real ops; the game uses these around audio DMA buffers) ----

void osInvalDCache(void* p, int32_t len) {
    if (p != nullptr && len > 0) {
        DCInvalidateRange(p, (uint32_t)len);
    }
}

void osWritebackDCache(void* p, int32_t len) {
    if (p != nullptr && len > 0) {
        DCFlushRange(p, (uint32_t)len);
    }
}

void osWritebackDCacheAll(void) {
    DCFlushRange((void*)0x80000000, 0x01800000); // MEM1 (24 MB)
}

// ---- Controller / EEPROM / Motor / VI: inert (handled elsewhere or unused) ----

int32_t osContInit(OSMesgQueue* /*mq*/, uint8_t* controllerBits, OSContStatus* /*status*/) {
    if (controllerBits != nullptr) {
        *controllerBits = 0x01; // report one controller present
    }
    return 0;
}

int32_t osContStartReadData(OSMesgQueue* /*mq*/) {
    return 0;
}

void osContGetReadData(OSContPad* pad) {
    if (pad == nullptr) {
        return;
    }
    // Pull the live pad through the ControlDeck. WriteToPad refreshes all
    // MAXCONTROLLERS entries (matching the game's gControllerPads[] array) from the
    // GameCube/Wii hardware. Before the ControlDeck exists, report neutral input.
    auto ctx = Ship::Context::GetInstance();
    auto deck = ctx != nullptr ? ctx->GetControlDeck() : nullptr;
    if (deck != nullptr) {
        deck->WriteToPad(pad);
    } else {
        std::memset(pad, 0, sizeof(OSContPad));
    }
}

int32_t osEepromProbe(OSMesgQueue* /*mq*/) {
    return 0; // no EEPROM; saves go through the port's save backend
}

s32 osEepromLongRead(OSMesgQueue* /*mq*/, u8 /*address*/, u8* /*buffer*/, int /*nbytes*/) {
    return -1;
}

s32 osMotorInit(OSMesgQueue* /*mq*/, OSPfs* /*pfs*/, s32 /*channel*/) {
    return 0;
}

s32 __osMotorAccess(OSPfs* /*pfs*/, u32 /*vibrate*/) {
    return 0;
}

void osViBlack(uint8_t /*active*/) {
    // GX owns the video interface.
}

void osViSetSpecialFeatures(u32 /*func*/) {
}

// These N64 OS primitives are provided as WEAK defaults: a port that excludes its own
// src/libultra (e.g. Starship) links these, while a port that keeps its N64 libultra io
// files (e.g. Ghostship's lib/src/osAiSetFrequency.c) overrides them with its strong
// definition - no multiple-definition clash either way.

// GX owns the video interface (framebuffer flips and the retrace event are driven by
// the Fast3D window), so the N64 VI calls the game still makes are inert.
__attribute__((weak)) void osViSwapBuffer(void* /*framebuffer*/) {
}

__attribute__((weak)) void osViSetEvent(OSMesgQueue* /*mq*/, OSMesg /*msg*/, u32 /*retraceCount*/) {
}

// CPU count register: the PPC time base lower word. Games use it for timing and RNG
// seeding, so return the live value rather than a constant.
__attribute__((weak)) uint32_t osGetCount(void) {
    uint32_t count;
    __asm__ volatile("mftb %0" : "=r"(count));
    return count;
}

// No EEPROM on console; saves go through the port's save backend. Report failure so
// callers fall back rather than trusting uninitialised data.
__attribute__((weak)) s32 osEepromRead(OSMesgQueue* /*mq*/, u8 /*address*/, u8* /*buffer*/) {
    return -1;
}

__attribute__((weak)) s32 osEepromWrite(OSMesgQueue* /*mq*/, u8 /*address*/, u8* /*buffer*/) {
    return -1;
}

// Audio interface frequency: the console mixer (ASND) runs at its own fixed rate, so
// this is inert. Return the requested frequency as the "actual" rate, matching the N64
// contract closely enough for callers that read it back.
__attribute__((weak)) s32 osAiSetFrequency(u32 freq) {
    return (s32) freq;
}

} // extern "C"
