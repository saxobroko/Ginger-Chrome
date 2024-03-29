// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace base {
class TimeDelta;
}

namespace device {
class BluetoothUUID;
}

namespace content {

// General Metrics

// Enumeration for outcomes of querying the bluetooth cache.
enum class CacheQueryOutcome {
  SUCCESS = 0,
  BAD_RENDERER = 1,
  NO_DEVICE = 2,
  NO_SERVICE = 3,
  NO_CHARACTERISTIC = 4,
  NO_DESCRIPTOR = 5,
};

// requestDevice() Metrics

// Records stats about the arguments used when calling requestDevice.
//  - The union of filtered and optional service UUIDs.
void RecordRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options);

// GattServer.connect() Metrics

enum class UMAConnectGATTOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  UNKNOWN = 2,
  IN_PROGRESS = 3,
  FAILED = 4,
  AUTH_FAILED = 5,
  AUTH_CANCELED = 6,
  AUTH_REJECTED = 7,
  AUTH_TIMEOUT = 8,
  UNSUPPORTED_DEVICE = 9,
  // Note: Add new ConnectGATT outcomes immediately above this line. Make sure
  // to update the enum list in tools/metrics/histograms/histograms.xml
  // accordingly.
  COUNT
};

// There should be a call to this function before every
// Send(BluetoothMsg_ConnectGATTSuccess) and
// Send(BluetoothMsg_ConnectGATTError).
void RecordConnectGATTOutcome(UMAConnectGATTOutcome outcome);

// Records the outcome of the cache query for connectGATT. Should only be called
// if QueryCacheForDevice fails.
void RecordConnectGATTOutcome(CacheQueryOutcome outcome);

// Records how long it took for the connection to succeed.
void RecordConnectGATTTimeSuccess(const base::TimeDelta& duration);

// Records how long it took for the connection to fail.
void RecordConnectGATTTimeFailed(const base::TimeDelta& duration);

// getPrimaryService() and getPrimaryServices() Metrics

enum class UMAGetPrimaryServiceOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NOT_FOUND = 2,
  NO_SERVICES = 3,
  DEVICE_DISCONNECTED = 4,
  // Note: Add new GetPrimaryService outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

// There should be a call to this function whenever
// RemoteServerGetPrimaryServicesCallback is run.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE for
// getPrimaryService.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE for
// getPrimaryServices.
void RecordGetPrimaryServicesOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetPrimaryServiceOutcome outcome);

// Records the outcome of the cache query for getPrimaryServices. Should only be
// called if QueryCacheForDevice fails.
void RecordGetPrimaryServicesOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome);

// Records the UUID of the service used when calling getPrimaryService.
void RecordGetPrimaryServicesServices(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const absl::optional<device::BluetoothUUID>& service);

// getCharacteristic() and getCharacteristics() Metrics

enum class UMAGetCharacteristicOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NO_SERVICE = 2,
  NOT_FOUND = 3,
  BLOCKLISTED = 4,
  NO_CHARACTERISTICS = 5,
  // Note: Add new outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histogram/histograms.xml accordingly.
  COUNT
};

enum class UMAGetDescriptorOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NO_SERVICE = 2,
  NO_CHARACTERISTIC = 3,
  NOT_FOUND = 4,
  BLOCKLISTED = 5,
  NO_DESCRIPTORS = 6,
  // Note: Add new outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histogram/histograms.xml accordingly.
  COUNT
};

// There should be a call to this function whenever
// RemoteServiceGetCharacteristicsCallback is run.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE for
// getCharacteristic.
// Pass blink::mojom::WebBluetoothGATTQueryQuantity::MULTIPLE for
// getCharacteristics.
void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    UMAGetCharacteristicOutcome outcome);

// Records the outcome of the cache query for getCharacteristics. Should only be
// called if QueryCacheForService fails.
void RecordGetCharacteristicsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome);

// Records the UUID of the characteristic used when calling getCharacteristic.
void RecordGetCharacteristicsCharacteristic(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const absl::optional<device::BluetoothUUID>& characteristic);

// Records the outcome of the cache query for getDescriptors. Should only be
// called if QueryCacheForService fails.
void RecordGetDescriptorsOutcome(
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    CacheQueryOutcome outcome);

enum class UMARSSISignalStrengthLevel {
  LESS_THAN_OR_EQUAL_TO_MIN_RSSI,
  LEVEL_0,
  LEVEL_1,
  LEVEL_2,
  LEVEL_3,
  LEVEL_4,
  GREATER_THAN_OR_EQUAL_TO_MAX_RSSI,
  // Note: Add new RSSI signal strength level immediately above this line.
  COUNT
};

// Records the raw RSSI, and processed result displayed to users, when
// content::BluetoothDeviceChooserController::CalculateSignalStrengthLevel() is
// called.
void RecordRSSISignalStrength(int rssi);
void RecordRSSISignalStrengthLevel(UMARSSISignalStrengthLevel level);

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
