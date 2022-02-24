// Copyright (c) 2022 Igor Pener
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.

#include "att_api.h"
#include "ble/BLE.h"

#include "App.h"
#include "DB.h"
#include "HAPCrypto.h"
#include "HAPMbed.h"
#include "HAPPlatformBLEPeripheralManager+Init.h"

#if HAP_LOG_LEVEL
#include "mbed.h"
#include "USBSerial.h"

class USBLogger: public USBSerial {
protected:
    int _putc(int c) override {
        int ret;

        if (c == '\n') {
            static uint8_t nlcr[] = {'\r', '\n'};
            ret = send(nlcr, 2);
        } else {
            ret = send((uint8_t *)&c, 1);
        }
        return ret ? c : -1;
    }
};

FileHandle *mbed::mbed_override_console(int fd) {
    static USBLogger serial;
    return &serial;
}
#endif

static const HAPLogObject logObject = { .subsystem = "kHAPPlatform_LogSubsystem", .category = "BLEPeripheralManager" };

static struct {
    uint16_t handle;
    uint16_t size;
    uint16_t offset;
    uint8_t  bytes[ATT_VALUE_MAX_LEN];
} _readBuffer;

static struct Handles {
    uint16_t* value = nullptr;
    uint16_t* cccd = nullptr;
    uint16_t* iid = nullptr;
} * _handles = nullptr;

static ble::advertising_handle_t _advertisingHandle = ble::LEGACY_ADVERTISING_HANDLE;
static GattCharacteristic*       _chrs[kAttributeCount];
static GattAttribute*            _dscs[kAttributeCount];
static uintptr_t                 _connectionHandle = 0;
static uint8_t                   _lastIndex = 0;
static uint8_t                   _index = 0;

static HAPBLEAdvertisingInterval               _advertisingInterval = 0;
static HAPPlatformBLEPeripheralManagerRef      _blePeripheralManager = nullptr;
static HAPPlatformBLEPeripheralManagerDelegate _delegate;

void onInitComplete(BLE::InitializationCompleteCallbackContext *event) {
    if (event->error) {
        HAPLogError(&logObject, "BLE initialization failed %d", event->error);
    } else {
        // Start accessory server for App.
        AppAccessoryServerStart();
    }
}

void scheduleEvents(BLE::OnEventsToProcessCallbackContext *event) {
    eventQueue.call(&event->ble, &BLE::processEvents);
}

void updateCentralConnection(uintptr_t connectionHandle) {
    if (connectionHandle != _connectionHandle) {
        if (_connectionHandle != 0) {
            if (_delegate.handleDisconnectedCentral) {
                _delegate.handleDisconnectedCentral(_blePeripheralManager, _connectionHandle, _delegate.context);
            }
            _connectionHandle = 0;
        }
        if (connectionHandle != 0) {
            _connectionHandle = connectionHandle;
            memset(&_readBuffer.bytes, 0, sizeof _readBuffer.bytes);

            if (_delegate.handleConnectedCentral) {
                _delegate.handleConnectedCentral(_blePeripheralManager, _connectionHandle, _delegate.context);
            }
        }
    }
}

void handleReadRequest(GattReadAuthCallbackParams* params) {
    updateCentralConnection(params->connHandle);

    if (!params->offset) {
        HAPLogDebug(&logObject, "(0x%04x) ATT Read Request.", params->handle);

        auto err = _delegate.handleReadRequest(_blePeripheralManager, params->connHandle, params->handle, _readBuffer.bytes, sizeof _readBuffer.bytes, (size_t*)&_readBuffer.size, _delegate.context);

        if (err) {
            HAPAssert(err == kHAPError_InvalidState || err == kHAPError_OutOfResources);
            HAPLog(&logObject, "HandleReadRequest failed %d", err);

            params->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INSUFFICIENT_RESOURCES;
            return updateCentralConnection(0);
        }
        _readBuffer.handle = params->handle;
        params->len = _readBuffer.size;
        params->data = _readBuffer.bytes;
    } else {
        HAPLogDebug(&logObject, "(0x%04x) ATT Read Blob Request.", params->handle);

        if (_readBuffer.handle != params->handle) {
            HAPLog(&logObject, "Received Read Blob Request for a different characteristic than prior Read Request.");

            params->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_REQUEST_NOT_SUPPORTED;
            return updateCentralConnection(0);
        }

        if (params->offset > _readBuffer.size) {
            HAPLog(&logObject, "Offset %u exceeds the read buffer size %u.", params->offset, _readBuffer.offset);

            params->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
            return updateCentralConnection(0);
        }
    }
    params->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
}

void handleWriteRequest(GattWriteAuthCallbackParams *params) {
    updateCentralConnection(params->connHandle);

    auto err = _delegate.handleWriteRequest(_blePeripheralManager, params->connHandle, params->handle, (void*)params->data, params->len, _delegate.context);

    if (err) {
        HAPAssert(err == kHAPError_InvalidState || err == kHAPError_InvalidData);
        HAPLogError(&logObject, "HandleWriteRequest failed %d", err);

        params->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_PDU;
        return updateCentralConnection(0);
    }

    params->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
}

struct EventHandler : private mbed::NonCopyable<EventHandler>, public ble::Gap::EventHandler, public ble::GattServer::EventHandler {
    void onAdvertisingEnd(const ble::AdvertisingEndEvent &event) override {
        if (!_advertisingInterval) return;

        ble::AdvertisingParameters params(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(_advertisingInterval))
        );

        auto &ble = BLE::Instance();
        auto &gap = ble.gap();

        if (auto err = gap.setAdvertisingParameters(_advertisingHandle, params)) {
            HAPLogError(&logObject, "Gap::setAdvertisingParameters() failed %d", err);
        }

        if (auto err = gap.startAdvertising(_advertisingHandle)) {
            HAPLogError(&logObject, "Gap::startAdvertising() failed %d", err);
        }
    }

    void onConnectionComplete(const ble::ConnectionCompleteEvent &event) override {
        if (auto err = event.getStatus()) {
            HAPLogError(&logObject, "Gap::onConnectionComplete failed %d", err);
        } else {
            auto addr = event.getPeerAddress();
            HAPLog(&logObject, "Connected to: %02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
        }
    }

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override {
        HAPLog(&logObject, "Disconnected with reason %02x.", event.getReason().value());

        updateCentralConnection(0);
    }

    void onUpdatesEnabled(const GattUpdatesEnabledCallbackParams &params) override {
        HAPLog(&logObject, "Subscribed to characteristic %04x", params.charHandle);

        updateCentralConnection(params.connHandle);
    }

    void onUpdatesDisabled(const GattUpdatesEnabledCallbackParams &params) override {
        HAPLog(&logObject, "Unsubscribed from characteristic %04x", params.charHandle);

        updateCentralConnection(0);
    }
};

static EventHandler _eventHandler;

//-------------------------------------------------------------------------------------------------

void HAPPlatformBLEPeripheralManagerCreate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerOptions* options) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(options);
    HAPPrecondition(options->keyValueStore);

    HAPRawBufferZero(&_delegate, sizeof _delegate);

    auto &ble = BLE::Instance();

    if (ble.hasInitialized()) {
        HAPLogError(&logObject, "The BLE instance has already been initialized.");
    } else {
        auto &gap = ble.gap();

        gap.setEventHandler(&_eventHandler);
        ble.onEventsToProcess(scheduleEvents);

        if (auto err = ble.init(onInitComplete)) {
            HAPLogError(&logObject, "BLE::init failed with %d", err);
        }
    }
}

void HAPPlatformBLEPeripheralManagerSetDelegate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerDelegate* _Nullable delegate) {
    HAPPrecondition(blePeripheralManager);

    if (delegate) {
        _delegate = *delegate;
        _blePeripheralManager = blePeripheralManager;
        _handles = new Handles[kAttributeCount];
    } else {
        HAPRawBufferZero(&_delegate, sizeof _delegate);

        auto &ble = BLE::Instance();

        if (ble.hasInitialized()) {
            if (auto err = ble.shutdown()) {
                HAPLogError(&kHAPLog_Default, "BLE::shutdown failed with %d", err);
            }
        }
    }
}

void HAPPlatformBLEPeripheralManagerSetDeviceAddress(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerDeviceAddress* deviceAddress) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(deviceAddress);

    HAPLog(&logObject, __func__);
}

void HAPPlatformBLEPeripheralManagerSetDeviceName(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const char* deviceName) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(deviceName);

    HAPLog(&logObject, __func__);
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformBLEPeripheralManagerAddService(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* type,
        bool isPrimary HAP_UNUSED) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(type);

    HAPLog(&logObject, __func__);

    GattService svc { { type->bytes, UUID::LSB }, &_chrs[_lastIndex], (uint16_t)(_index - _lastIndex) };

    auto &ble = BLE::Instance();
    auto &server = ble.gattServer();
    auto err = server.addService(svc);

    if (err) {
        HAPLogError(&logObject, "ble::GattServer::addService() failed %d", err);
    }

    for (auto i = _lastIndex; i < _index; ++i) {
        *(_handles[i].value) = _chrs[i]->getValueHandle();

        if (_chrs[i]->getDescriptorCount()) {
            *(_handles[i].iid) = _chrs[i]->getDescriptor(0)->getHandle();

            if (_handles[i].cccd) {
                *(_handles[i].cccd) = _chrs[i]->getDescriptor(1)->getHandle();
            }
        }
    }

    _lastIndex = _index;

    return err ? kHAPError_OutOfResources : kHAPError_None;
}

void HAPPlatformBLEPeripheralManagerRemoveAllServices(HAPPlatformBLEPeripheralManagerRef blePeripheralManager) {
    HAPPrecondition(blePeripheralManager);

    HAPLog(&logObject, __func__);

    auto &ble = BLE::Instance();
    auto &gap = ble.gap();
    auto &server = ble.gattServer();

    _advertisingInterval = 0;

    if (gap.isAdvertisingActive(_advertisingHandle)) {
        if (auto err = gap.stopAdvertising(_advertisingHandle)) {
            HAPLogError(&logObject, "ble::Gap::stopAdvertising() failed %d", err);
        }
    }

    if (auto err = server.reset()) {
        HAPLogError(&logObject, "ble::GattServer::reset() failed %d", err);
    }

    for (uint8_t i = 0; i < kAttributeCount; ++i) {
        if (_chrs[i]) { delete _chrs[i]; }
        if (_dscs[i]) { delete _dscs[i]; }
    }
    _index = 0;
    _lastIndex = 0;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformBLEPeripheralManagerAddCharacteristic(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* type,
        HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        HAPPlatformBLEPeripheralManagerAttributeHandle* valueHandle,
        HAPPlatformBLEPeripheralManagerAttributeHandle* _Nullable cccDescriptorHandle) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(type);
    HAPPrecondition((!constBytes && !constNumBytes) || (constBytes && constNumBytes));
    HAPPrecondition(valueHandle);
    if (properties.notify || properties.indicate) {
        HAPPrecondition(cccDescriptorHandle);
    } else {
        HAPPrecondition(!cccDescriptorHandle);
    }

    HAPLog(&logObject, __func__);

    uint8_t prop = 0;

    if (properties.read) { prop |= GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ; }
    if (properties.write) { prop |= GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE; }
    if (properties.writeWithoutResponse) { prop |= GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE; }
    if (properties.notify) { prop |= GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY; }
    if (properties.indicate) { prop |= GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE; }

    if (constNumBytes && constBytes) {
        _chrs[_index] = new GattCharacteristic {{type->bytes, UUID::LSB}, (uint8_t*)constBytes, (uint16_t)constNumBytes, (uint16_t)constNumBytes, prop, nullptr, 0};
    } else {
        _chrs[_index] = new GattCharacteristic {{type->bytes, UUID::LSB}, nullptr, 0, ATT_VALUE_MAX_LEN, prop, &_dscs[_index], 1};
        _chrs[_index]->setReadAuthorizationCallback(handleReadRequest);
        _chrs[_index]->setWriteAuthorizationCallback(handleWriteRequest);
    }
    _handles[_index].value = valueHandle;
    _handles[_index].cccd = cccDescriptorHandle;
    _index++;

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformBLEPeripheralManagerAddDescriptor(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* type,
        HAPPlatformBLEPeripheralManagerDescriptorProperties properties HAP_UNUSED,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        HAPPlatformBLEPeripheralManagerAttributeHandle* descriptorHandle) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(type);
    HAPPrecondition(constBytes && constNumBytes);
    HAPPrecondition(descriptorHandle);

    HAPLog(&logObject, __func__);

    _dscs[_index] = new GattAttribute {{type->bytes, UUID::LSB}, (uint8_t*)constBytes, (uint16_t)constNumBytes, (uint16_t)constNumBytes};
    _handles[_index].iid = descriptorHandle;

    return kHAPError_None;
}

void HAPPlatformBLEPeripheralManagerPublishServices(HAPPlatformBLEPeripheralManagerRef blePeripheralManager) {
    HAPPrecondition(blePeripheralManager);

    HAPLog(&logObject, __func__);

    auto &ble = BLE::Instance();
    auto &server = ble.gattServer();

    server.setEventHandler(&_eventHandler);

    delete[] _handles;
}

void HAPPlatformBLEPeripheralManagerStartAdvertising(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        HAPBLEAdvertisingInterval advertisingInterval,
        const void* advertisingBytes,
        size_t numAdvertisingBytes,
        const void* _Nullable scanResponseBytes,
        size_t numScanResponseBytes) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(advertisingInterval);
    HAPPrecondition(advertisingBytes);
    HAPPrecondition(numAdvertisingBytes);
    HAPPrecondition(!numScanResponseBytes || scanResponseBytes);

    HAPLog(&logObject, __func__);

    auto &ble = BLE::Instance();
    auto &gap = ble.gap();

    _advertisingInterval = advertisingInterval;

    if (advertisingBytes && numAdvertisingBytes) {
        if (auto err = gap.setAdvertisingPayload(_advertisingHandle, { (uint8_t*)advertisingBytes, (uint8_t)numAdvertisingBytes })) {
            HAPLogError(&logObject, "Gap::setAdvertisingPayload() failed %d", err);
        }
    }

    if (scanResponseBytes && numScanResponseBytes) {
        if (auto err = gap.setAdvertisingScanResponse(_advertisingHandle, { (uint8_t*)scanResponseBytes, (uint8_t)numScanResponseBytes })) {
            HAPLogError(&logObject, "Gap::setAdvertisingScanResponse() failed %d", err);
        }
    }

    if (gap.isAdvertisingActive(_advertisingHandle)) {
        if (auto err = gap.stopAdvertising(_advertisingHandle)) {
            HAPLogError(&logObject, "ble::Gap::stopAdvertising() failed %d", err);
        }
    } else {
        ble::AdvertisingParameters params(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(advertisingInterval))
        );

        if (auto err = gap.setAdvertisingParameters(_advertisingHandle, params)) {
            HAPLogError(&logObject, "Gap::setAdvertisingParameters() failed %d", err);
        }

        if (auto err = gap.startAdvertising(_advertisingHandle)) {
            HAPLogError(&logObject, "Gap::startAdvertising() failed %d", err);
        }
    }
}

void HAPPlatformBLEPeripheralManagerStopAdvertising(HAPPlatformBLEPeripheralManagerRef blePeripheralManager) {
    HAPPrecondition(blePeripheralManager);

    HAPLog(&logObject, __func__);

    auto &ble = BLE::Instance();
    auto &gap = ble.gap();

    _advertisingInterval = 0;

    if (gap.isAdvertisingActive(_advertisingHandle)) {
        if (auto err = gap.stopAdvertising(_advertisingHandle)) {
            HAPLogError(&logObject, "ble::Gap::stopAdvertising() failed %d", err);
        }
    }
}

void HAPPlatformBLEPeripheralManagerCancelCentralConnection(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle HAP_UNUSED) {
    HAPPrecondition(blePeripheralManager);

    HAPLog(&logObject, __func__);

    updateCentralConnection(0);
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformBLEPeripheralManagerSendHandleValueIndication(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle,
        HAPPlatformBLEPeripheralManagerAttributeHandle valueHandle,
        const void* _Nullable bytes,
        size_t numBytes) {
    HAPPrecondition(blePeripheralManager);

    HAPLog(&logObject, __func__);

    auto &ble = BLE::Instance();
    auto &server = ble.gattServer();

    if (auto err = server.write(connectionHandle, valueHandle, (const uint8_t*)bytes, (uint16_t)numBytes)) {
        HAPLogError(&logObject, "ble::GattServer::write() failed %d", err);
        return kHAPError_InvalidState;
    }
    return kHAPError_None;
}
