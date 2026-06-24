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

#include <cstdint>
#include <cstring>

// Forward-declare the few libogc entry points used here instead of including
// <ogc/cache.h>/<ogc/lwp_watchdog.h>: those transitively pull <ogc/gu.h>, whose
// Mtx typedef (f32[3][4]) collides with the N64 libultra Mtx in scope from the
// libultra headers above.
extern "C" void DCFlushRange(void* startaddr, uint32_t len);
extern "C" void DCInvalidateRange(void* startaddr, uint32_t len);
extern "C" uint64_t gettime(void);

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
    // Strip the cached/uncached MEM region bits to get the physical offset.
    return (uint32_t)((uintptr_t)vaddr & 0x3FFFFFFF);
}

// ---- Time ----

uint64_t osGetTime(void) {
    return gettime();
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
    if (pad != nullptr) {
        std::memset(pad, 0, sizeof(OSContPad)); // real input comes via the ControlDeck
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

} // extern "C"
