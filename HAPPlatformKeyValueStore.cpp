// Copyright (c) 2022 Igor Pener
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.

#include "HAPPlatformKeyValueStore+Init.h"

#include <stdlib.h>

#include "platform/mbed_error.h"
#include "kvstore_global_api.h"

static const HAPLogObject kvs_log = { .subsystem = kHAPPlatform_LogSubsystem, .category = "KeyValueStore" };

void HAPPlatformKeyValueStoreCreate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        const HAPPlatformKeyValueStoreOptions* options) {
    HAPPrecondition(options->rootDirectory);
    HAPPrecondition(keyValueStore);

    keyValueStore->rootDirectory = options->rootDirectory;
    HAPLog(&kvs_log, "Storage location: %s", keyValueStore->rootDirectory);
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreGet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        void* _Nullable bytes,
        size_t maxBytes,
        size_t* _Nullable numBytes,
        bool* found) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(!maxBytes || bytes);
    HAPPrecondition((bytes == NULL) == (numBytes == NULL));
    HAPPrecondition(found);

    char path[sizeof(MBED_CONF_NANOSTACK_HAL_KVSTORE_PATH) + 4];
    kv_info_t info;
    
    sprintf(path, "%s%02x%02x", keyValueStore->rootDirectory, domain, key);

    if (int res = kv_get_info(path, &info)) {
        HAPLogDebug(&kvs_log, "kv_get_info failed %d\n", MBED_GET_ERROR_CODE(res));
        *found = false;
        return kHAPError_None;
    }
    
    if (int res = kv_get(path, bytes, info.size, numBytes)) { 
        HAPLogError(&kvs_log, "kv_get failed %d\n", MBED_GET_ERROR_CODE(res));
        return kHAPError_Unknown;
    }
    *found = true;
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreSet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        const void* bytes,
        size_t numBytes) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(bytes);

    char path[sizeof(MBED_CONF_NANOSTACK_HAL_KVSTORE_PATH) + 4];
    
    sprintf(path, "%s%02x%02x", keyValueStore->rootDirectory, domain, key);

    if (int res = kv_set(path, bytes, numBytes, 0)) {
        HAPLogError(&kvs_log, "kv_set failed %d\n", MBED_GET_ERROR_CODE(res));
        return kHAPError_Unknown;
    } 
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreRemove(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key) {
    HAPPrecondition(keyValueStore);

    char path[sizeof(MBED_CONF_NANOSTACK_HAL_KVSTORE_PATH) + 4];

    sprintf(path, "%s%02x%02x", keyValueStore->rootDirectory, domain, key);

    int res = kv_remove(path);

    if (res == MBED_ERROR_ITEM_NOT_FOUND) {
        HAPLogDebug(&kvs_log, "Can't remove key %02x%02x because it doesn't exists.\n", domain, key);
        return kHAPError_None;
    } else if (res) {
        HAPLogError(&kvs_log, "kv_remove failed %d\n", MBED_GET_ERROR_CODE(res));
        return kHAPError_Unknown;
    }
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreEnumerate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreEnumerateCallback callback,
        void* _Nullable context) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(callback);

    char path[sizeof(MBED_CONF_NANOSTACK_HAL_KVSTORE_PATH) + 2];
    char key[sizeof(path) + 2];
    kv_iterator_t it;
    HAPError err = kHAPError_None;

    int i = sprintf(path, "%s%02x", keyValueStore->rootDirectory, domain);

    int res = kv_iterator_open(&it, path);

    if (res == MBED_ERROR_ITEM_NOT_FOUND) {
        HAPLogDebug(&kvs_log, "Can't enumerate domain %02x because it doesn't exist.\n", domain);
        err = kHAPError_None;
    } else if (res) {
        HAPLogError(&kvs_log, "kv_iterator_open failed %d\n", MBED_GET_ERROR_CODE(res));
        err = kHAPError_Unknown;
    }

    while (kv_iterator_next(it, key, sizeof key) != MBED_ERROR_ITEM_NOT_FOUND) {
        bool shouldContinue = true;
        err = callback(context, keyValueStore, domain, (uint8_t)strtol(&key[i], NULL, 16), &shouldContinue);

        if (!shouldContinue) break;
    }

    res = kv_iterator_close(it);

    if (res) {
        HAPLogError(&kvs_log, "kv_iterator_close failed %d\n", MBED_GET_ERROR_CODE(res));
        err = kHAPError_Unknown;
    }
    return err;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStorePurgeDomain(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain) {
    HAPPrecondition(keyValueStore);

    char path[sizeof(MBED_CONF_NANOSTACK_HAL_KVSTORE_PATH) + 2];
    char key[sizeof(path) + 2];
    kv_iterator_t it;
    HAPError err = kHAPError_None;

    sprintf(path, "%s%02x", keyValueStore->rootDirectory, domain);

    int res = kv_iterator_open(&it, path);

    if (res == MBED_ERROR_ITEM_NOT_FOUND) {
        HAPLogDebug(&kvs_log, "Can't enumerate domain %02x because it doesn't exist.\n", domain);
        err = kHAPError_None;
    } else if (res) {
        HAPLogError(&kvs_log, "kv_iterator_open failed %d\n", MBED_GET_ERROR_CODE(res));
        err = kHAPError_Unknown;
    }

    while (kv_iterator_next(it, key, sizeof key) != MBED_ERROR_ITEM_NOT_FOUND) {
        res = kv_remove(key);

        if (res) {
            HAPLogError(&kvs_log, "kv_remove failed %d\n", MBED_GET_ERROR_CODE(res));
            err = kHAPError_Unknown;
        }
    }

    res = kv_iterator_close(it);

    if (res) {
        HAPLogError(&kvs_log, "kv_iterator_close failed %d\n", MBED_GET_ERROR_CODE(res));
        err = kHAPError_Unknown;
    }

    return err;
}
