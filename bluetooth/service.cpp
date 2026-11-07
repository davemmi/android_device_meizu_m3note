//
// Copyright 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define LOG_TAG "android.hardware.bluetooth@1.0-service.m3note"

#include <android/hardware/bluetooth/1.0/IBluetoothHci.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

#include "hal/bluetooth_hci.h"

// Extra threads make priority inheritance faster.
static const size_t kMaxThreads = 5;

using android::OK;
using android::sp;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::bluetooth::V1_0::IBluetoothHci;
using android::hardware::bluetooth::V1_0::implementation::BluetoothHci;

// The HCI implementation is built in (hal/, a copy of the AOSP default one
// with a filter for the MTK firmware's unsolicited vendor events) instead of
// being loaded as the passthrough android.hardware.bluetooth@1.0-impl.
int main() {
  configureRpcThreadpool(kMaxThreads, true /* callerWillJoin */);

  sp<IBluetoothHci> hci = new BluetoothHci();
  if (hci->registerAsService() != OK) {
    ALOGE("Could not register IBluetoothHci/default");
    return 1;
  }

  joinRpcThreadpool();
  return 1;
}
