// Copyright (c) 2022 Igor Pener
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.

#include "HAPPlatformTimer.h"
#include "HAPMbed.h"

HAP_RESULT_USE_CHECK
HAPError HAPPlatformTimerRegister(
        HAPPlatformTimerRef* timer,
        HAPTime deadline,
        HAPPlatformTimerCallback callback,
        void* _Nullable context) {
    HAPTime interval = 0;
    HAPTime now = HAPPlatformClockGetCurrent();

    if (deadline > now) {
        interval = deadline - now;
    }
    *timer = eventQueue.call_in(std::chrono::duration<int, std::milli>(interval), [timer, context, callback] {
        callback(*timer, context);
    });

    return kHAPError_None;
}

void HAPPlatformTimerDeregister(HAPPlatformTimerRef timer) {
    if (!eventQueue.cancel(timer)) {
        HAPLogError(&kHAPLog_Default, "Failed to cancel timer %d", timer);
    }
}
