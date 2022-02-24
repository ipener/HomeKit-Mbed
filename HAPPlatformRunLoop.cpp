// Copyright (c) 2022 Igor Pener
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.

#include "HAPPlatformRunLoop+Init.h"
#include "HAPMbed.h"

events::EventQueue eventQueue;

void HAPPlatformRunLoopCreate(const HAPPlatformRunLoopOptions* options) {
    HAPPrecondition(options);
    HAPPrecondition(options->keyValueStore);
}

void HAPPlatformRunLoopRelease(void) {
}

void HAPPlatformRunLoopRun(void) {
    eventQueue.dispatch_forever();  
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformRunLoopScheduleCallback(
        HAPPlatformRunLoopCallback callback,
        void* _Nullable context,
        size_t contextSize) {
    eventQueue.call(callback, context, contextSize);
    return kHAPError_None;
}

void HAPPlatformRunLoopStop(void) {
    eventQueue.break_dispatch();
}
