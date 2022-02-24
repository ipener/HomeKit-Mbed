// Copyright (c) 2022 Igor Pener
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.

#include "HAPPlatform.h"

#include "rtos/Kernel.h"

HAPTime HAPPlatformClockGetCurrent(void) {
    return (HAPTime)rtos::Kernel::Clock::now().time_since_epoch().count();
}
