// Copyright (c) 2022 Igor Pener
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.

#include "HAPPlatformTimer.h"
#include "HAPMbed.h"

#define MAX_TIMERS 32

struct {
    uint32_t mask;

    struct {
        int id;
    } timer[MAX_TIMERS];
} _timers = {0x00000000};

HAP_RESULT_USE_CHECK
HAPError HAPPlatformTimerRegister(
        HAPPlatformTimerRef* timer,
        HAPTime deadline,
        HAPPlatformTimerCallback callback,
        void* _Nullable context) {
    HAPTime interval = 0;
    HAPTime current_time = HAPPlatformClockGetCurrent();

    if (deadline > current_time) {
        interval = deadline - current_time;    
    }

    for (uint8_t i = 0; i < MAX_TIMERS; ++i) {
        if (_timers.mask & (1 << i)) continue;

        *timer = (HAPPlatformTimerRef)&_timers.timer[i];

        _timers.mask |= (1 << i);
        _timers.timer[i].id = eventQueue.call_in(std::chrono::duration<int, std::milli>(interval), callback, *timer, context);

        return kHAPError_None;
    }

    return kHAPError_OutOfResources;
}

void HAPPlatformTimerDeregister(HAPPlatformTimerRef timer) {
    for (uint8_t i = 0; i < MAX_TIMERS; ++i) {
        if (timer != (HAPPlatformTimerRef)&_timers.timer[i]) continue;

        _timers.mask &= ~(1 << i);

        if (!eventQueue.cancel(_timers.timer[i].id)) {
            HAPLogError(&kHAPLog_Default, "failed to cancel timer %d", _timers.timer[i].id);
        }
        return;
    }
}
