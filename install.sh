#!/bin/bash -e

git submodule add -f https://github.com/apple/HomeKitADK.git
git submodule add -f https://github.com/ARMmbed/mbed-os.git
git submodule update --init --recursive

cd HomeKitADK
git checkout -q fb201f9

rm Makefile PAL/Raspi PAL/Linux Applications/Lightbulb/Main.c Applications/Lock/Main.c
rm -rf Build/ Tests/ Tools/ PAL/Crypto/OpenSSL PAL/Crypto/MbedTLS/Ed25519 Applications/Lock
rm -rf PAL/Mock/HAPPlatform.c PAL/Mock/HAPPlatform+Init.h PAL/Mock/HAPPlatformAbort.c PAL/Mock/HAPPlatformBLEPeripheralManager.c PAL/Mock/HAPPlatformBLEPeripheralManager+Init.h PAL/Mock/HAPPlatformBLEPeripheralManager+Test.h PAL/Mock/HAPPlatformClock.c PAL/Mock/HAPPlatformClock+Test.h PAL/Mock/HAPPlatformKeyValueStore.c PAL/Mock/HAPPlatformKeyValueStore+Init.h PAL/Mock/HAPPlatformLog.c PAL/Mock/HAPPlatformRandomNumber.c PAL/Mock/HAPPlatformRunLoop.c PAL/Mock/HAPPlatformServiceDiscovery.c PAL/Mock/HAPPlatformServiceDiscovery+Init.h PAL/Mock/HAPPlatformServiceDiscovery+Test.h PAL/Mock/HAPPlatformStartup.S PAL/Mock/HAPPlatformSystemCommand.c PAL/Mock/HAPPlatformSystemCommand.h PAL/Mock/HAPPlatformTCPStreamManager.c PAL/Mock/HAPPlatformTCPStreamManager+Init.h PAL/Mock/HAPPlatformTCPStreamManager+Test.h PAL/Mock/HAPPlatformTimer.c PAL/Mock/HAPPlatformTimer+Init.h
rm -rf PAL/POSIX/HAPPlatform+Init.h PAL/POSIX/HAPPlatformAccessorySetup+Init.h PAL/POSIX/HAPPlatformAccessorySetup.c PAL/POSIX/HAPPlatformAccessorySetupDisplay+Init.h PAL/POSIX/HAPPlatformAccessorySetupDisplay.c PAL/POSIX/HAPPlatformAccessorySetupNFC+Init.h PAL/POSIX/HAPPlatformAccessorySetupNFC.c PAL/POSIX/HAPPlatformBLEPeripheralManager+Init.h PAL/POSIX/HAPPlatformBLEPeripheralManager.c PAL/POSIX/HAPPlatformClock.c PAL/POSIX/HAPPlatformFileHandle.h PAL/POSIX/HAPPlatformFileManager.c PAL/POSIX/HAPPlatformFileManager.h PAL/POSIX/HAPPlatformKeyValueStore+Init.h PAL/POSIX/HAPPlatformKeyValueStore+SDKDomains.h PAL/POSIX/HAPPlatformKeyValueStore.c PAL/POSIX/HAPPlatformLog+Init.h PAL/POSIX/HAPPlatformMFiHWAuth+Init.h PAL/POSIX/HAPPlatformMFiHWAuth.c PAL/POSIX/HAPPlatformMFiTokenAuth.c PAL/POSIX/HAPPlatformRandomNumber.c PAL/POSIX/HAPPlatformRunLoop+Init.h PAL/POSIX/HAPPlatformRunLoop.c PAL/POSIX/HAPPlatformServiceDiscovery+Init.h PAL/POSIX/HAPPlatformServiceDiscovery.c PAL/POSIX/HAPPlatformSystemCommand.c PAL/POSIX/HAPPlatformSystemCommand.h PAL/POSIX/HAPPlatformTCPStreamManager+Init.h PAL/POSIX/HAPPlatformTCPStreamManager.c
rm -rf PAL/Darwin/*.m

git apply ../patches/HomeKitADK.patch
cd ..

cd mbed-os
git checkout -q tags/mbed-os-6.15.1

echo "https://github.com/ARMmbed/mbed-os.git#$(git rev-parse HEAD)" > ../mbed-os.lib
cd ..