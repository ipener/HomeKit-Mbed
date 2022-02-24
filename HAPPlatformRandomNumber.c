// Copyright (c) 2022 Igor Pener
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.

#include "HAPPlatform.h"
#include "HAPCrypto.h"

void HAPPlatformRandomNumberFill(void* bytes, size_t numBytes) {
    if (!bytes || !numBytes) return;

    CRYSError_t err = CRYS_RND_GenerateVector(&rndState, numBytes, (uint8_t*)bytes);

    if (err) {
        HAPLogError(&kHAPLog_Default, "CRYS_RND_GenerateVector failed %08x", err);
    }
}
