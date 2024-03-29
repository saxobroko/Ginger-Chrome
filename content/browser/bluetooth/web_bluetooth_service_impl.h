// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observation.h"
#include "content/browser/bad_message.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BluetoothDeviceChooserController;
class BluetoothDeviceScanningPromptController;
struct CacheQueryResult;
class FrameConnectedBluetoothDevices;
struct GATTNotifySessionAndCharacteristicClient;
class RenderFrameHost;
class RenderProcessHost;

bool HasValidFilter(
    const absl::optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters);

// Implementation of Mojo WebBluetoothService located in
// third_party/WebKit/public/platform/modules/bluetooth.
// It handles Web Bluetooth API requests coming from Blink / renderer
// process and uses the platform abstraction of device/bluetooth.
// WebBluetoothServiceImpl is not thread-safe and should be created on the
// UI thread as required by device/bluetooth.
// This class is instantiated on-demand via Mojo's ConnectToRemoteService
// from the renderer when the first Web Bluetooth API request is handled.
// RenderFrameHostImpl will create an instance of this class and keep
// ownership of it.
class CONTENT_EXPORT WebBluetoothServiceImpl
    : public blink::mojom::WebBluetoothService,
      public WebContentsObserver,
      public device::BluetoothAdapter::Observer,
      public BluetoothDelegate::FramePermissionObserver,
      public WebBluetoothPairingManagerDelegate {
 public:
  static blink::mojom::WebBluetoothResult TranslateConnectErrorAndRecord(
      device::BluetoothDevice::ConnectErrorCode error_code);

  // |render_frame_host|: The RFH that owns this instance.
  // |receiver|: The instance will be bound to this receiver's pipe.
  WebBluetoothServiceImpl(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver);
  ~WebBluetoothServiceImpl() override;

  void CrashRendererAndClosePipe(bad_message::BadMessageReason reason);

  // Sets the connection error handler for WebBluetoothServiceImpl's Binding.
  void SetClientConnectionErrorHandler(base::OnceClosure closure);

  // Checks the current requesting and embedding origins as well as the policy
  // or global Web Bluetooth block to determine if Web Bluetooth is allowed.
  // Returns |SUCCESS| if Bluetooth is allowed.
  blink::mojom::WebBluetoothResult GetBluetoothAllowed();

  // Returns whether the device is paired with the |render_frame_host_|'s
  // GetLastCommittedOrigin().
  bool IsDevicePaired(const std::string& device_address);

  // Informs each client in |scanning_clients_| of the user's permission
  // decision.
  void OnBluetoothScanningPromptEvent(
      BluetoothScanningPrompt::Event event,
      BluetoothDeviceScanningPromptController* prompt_controller);

  // BluetoothDelegate::FramePermissionObserverimplementation:
  void OnPermissionRevoked(const url::Origin& origin) override;
  content::RenderFrameHost* GetRenderFrameHost() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           ClearStateDuringRequestDevice);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           ClearStateDuringRequestScanningStart);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest, PermissionAllowed);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           PermissionPromptCanceled);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenTabHidden);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenTabOccluded);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenBlocked);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenFocusIsLost);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           ReadCharacteristicValueErrorWithValueIgnored);
  friend class FrameConnectedBluetoothDevicesTest;
  friend class WebBluetoothServiceImplTest;
  using PrimaryServicesRequestCallback =
      base::OnceCallback<void(device::BluetoothDevice*)>;
  using ScanFilters = std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>;

  class AdvertisementClient;
  class WatchAdvertisementsClient;
  class ScanningClient;

  // WebContentsObserver:
  // These functions should always check that the affected RenderFrameHost
  // is this->render_frame_host_ and not some other frame in the same tab.
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(Visibility visibility) override;
  void OnWebContentsLostFocus(RenderWidgetHost* render_widget_host) override;

  // BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceAdvertisementReceived(
      const std::string& device_address,
      const absl::optional<std::string>& device_name,
      const absl::optional<std::string>& advertisement_name,
      absl::optional<int8_t> rssi,
      absl::optional<int8_t> tx_power,
      absl::optional<uint16_t> appearance,
      const device::BluetoothDevice::UUIDList& advertised_uuids,
      const device::BluetoothDevice::ServiceDataMap& service_data_map,
      const device::BluetoothDevice::ManufacturerDataMap& manufacturer_data_map)
      override;
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  // Notifies the WebBluetoothServiceClient that characteristic
  // |characteristic_instance_id| changed it's value. We only do this for
  // characteristics that have been returned to the client in the past.
  void NotifyCharacteristicValueChanged(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value);

  // WebBluetoothService methods:
  void GetAvailability(GetAvailabilityCallback callback) override;
  void RequestDevice(blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
                     RequestDeviceCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void RemoteServerConnect(
      const blink::WebBluetoothDeviceId& device_id,
      mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothServerClient>
          client,
      RemoteServerConnectCallback callback) override;
  void RemoteServerDisconnect(
      const blink::WebBluetoothDeviceId& device_id) override;
  void RemoteServerGetPrimaryServices(
      const blink::WebBluetoothDeviceId& device_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const absl::optional<device::BluetoothUUID>& services_uuid,
      RemoteServerGetPrimaryServicesCallback callback) override;
  void RemoteServiceGetCharacteristics(
      const std::string& service_instance_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const absl::optional<device::BluetoothUUID>& characteristics_uuid,
      RemoteServiceGetCharacteristicsCallback callback) override;
  void RemoteCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      RemoteCharacteristicReadValueCallback callback) override;
  void RemoteCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothWriteType write_type,
      RemoteCharacteristicWriteValueCallback callback) override;
  void RemoteCharacteristicStartNotifications(
      const std::string& characteristic_instance_id,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothCharacteristicClient> client,
      RemoteCharacteristicStartNotificationsCallback callback) override;
  void RemoteCharacteristicStopNotifications(
      const std::string& characteristic_instance_id,
      RemoteCharacteristicStopNotificationsCallback callback) override;
  void RemoteCharacteristicGetDescriptors(
      const std::string& service_instance_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const absl::optional<device::BluetoothUUID>& characteristics_uuid,
      RemoteCharacteristicGetDescriptorsCallback callback) override;
  void RemoteDescriptorReadValue(
      const std::string& characteristic_instance_id,
      RemoteDescriptorReadValueCallback callback) override;
  void RemoteDescriptorWriteValue(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      RemoteDescriptorWriteValueCallback callback) override;
  void RequestScanningStart(
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
      RequestScanningStartCallback callback) override;
  void WatchAdvertisementsForDevice(
      const blink::WebBluetoothDeviceId& device_id,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      WatchAdvertisementsForDeviceCallback callback) override;

  void RequestDeviceImpl(
      blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
      RequestDeviceCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);

  void GetDevicesImpl(GetDevicesCallback callback,
                      scoped_refptr<device::BluetoothAdapter> adapter);

  // Callbacks for BLE scanning.
  void RequestScanningStartImpl(
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
      RequestScanningStartCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartDiscoverySessionForScanning(
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoverySessionErrorForScanning();

  // Callbacks for watch advertisements for device.
  void WatchAdvertisementsForDeviceImpl(
      const blink::WebBluetoothDeviceId& device_id,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      WatchAdvertisementsForDeviceCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartDiscoverySessionForWatchAdvertisements(
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoverySessionErrorForWatchAdvertisements();

  // Remove WatchAdvertisementsClients and ScanningClients with disconnected
  // WebBluetoothAdvertisementClients from their respective containers.
  void RemoveDisconnectedClients();

  // Stop active discovery sessions and destroy them if there aren't any active
  // AdvertisementClients.
  void MaybeStopDiscovery();

  // Should only be run after the services have been discovered for
  // |device_address|.
  void RemoteServerGetPrimaryServicesImpl(
      const blink::WebBluetoothDeviceId& device_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const absl::optional<device::BluetoothUUID>& services_uuid,
      RemoteServerGetPrimaryServicesCallback callback,
      device::BluetoothDevice* device);

  // Callback for BluetoothDeviceChooserController::GetDevice.
  void OnGetDevice(RequestDeviceCallback callback,
                   blink::mojom::WebBluetoothResult result,
                   blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
                   const std::string& device_id);

  // Callbacks for BluetoothDevice::CreateGattConnection.
  void OnCreateGATTConnectionSuccess(
      const blink::WebBluetoothDeviceId& device_id,
      base::TimeTicks start_time,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client,
      RemoteServerConnectCallback callback,
      std::unique_ptr<device::BluetoothGattConnection> connection);
  void OnCreateGATTConnectionFailed(
      base::TimeTicks start_time,
      RemoteServerConnectCallback callback,
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Callbacks for BluetoothRemoteGattCharacteristic::ReadRemoteCharacteristic.
  void OnCharacteristicReadValue(
      RemoteCharacteristicReadValueCallback callback,
      absl::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // Callbacks for BluetoothRemoteGattCharacteristic::WriteRemoteCharacteristic.
  void OnCharacteristicWriteValueSuccess(
      RemoteCharacteristicWriteValueCallback callback);
  void OnCharacteristicWriteValueFailed(
      RemoteCharacteristicWriteValueCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  // Callbacks for BluetoothRemoteGattCharacteristic::StartNotifySession.
  void OnStartNotifySessionSuccess(
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client,
      RemoteCharacteristicStartNotificationsCallback callback,
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);
  void OnStartNotifySessionFailed(
      RemoteCharacteristicStartNotificationsCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  // Callback for BluetoothGattNotifySession::Stop.
  void OnStopNotifySessionComplete(
      const std::string& characteristic_instance_id,
      RemoteCharacteristicStopNotificationsCallback callback);

  // Callbacks for BluetoothRemoteGattDescriptor::ReadRemoteDescriptor.
  void OnDescriptorReadValue(
      RemoteDescriptorReadValueCallback callback,
      absl::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // Callbacks for BluetoothRemoteGattDescriptor::WriteRemoteDescriptor.
  void OnDescriptorWriteValueSuccess(
      RemoteDescriptorWriteValueCallback callback);
  void OnDescriptorWriteValueFailed(
      RemoteDescriptorWriteValueCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  // Functions to query the platform cache for the bluetooth object.
  // result.outcome == CacheQueryOutcome::SUCCESS if the object was found in the
  // cache. Otherwise result.outcome that can used to record the outcome and
  // result.error will contain the error that should be sent to the renderer.
  // One of the possible outcomes is BAD_RENDERER. In this case we crash the
  // renderer, record the reason and close the pipe, so it's safe to drop
  // any callbacks.

  // Queries the platform cache for a Device with |device_id| for |origin|.
  // Fills in the |outcome| field and the |device| field if successful.
  CacheQueryResult QueryCacheForDevice(
      const blink::WebBluetoothDeviceId& device_id);

  // Queries the platform cache for a Service with |service_instance_id|. Fills
  // in the |outcome| field, and |device| and |service| fields if successful.
  CacheQueryResult QueryCacheForService(const std::string& service_instance_id);

  // Queries the platform cache for a characteristic with
  // |characteristic_instance_id|. Fills in the |outcome| field, and |device|,
  // |service| and |characteristic| fields if successful.
  CacheQueryResult QueryCacheForCharacteristic(
      const std::string& characteristic_instance_id);

  // Queries the platform cache for a descriptor with |descriptor_instance_id|.
  // Fills in the |outcome| field, and |device|, |service|, |characteristic|,
  // |descriptor| fields if successful.
  CacheQueryResult QueryCacheForDescriptor(
      const std::string& descriptor_instance_id);

  void RunPendingPrimaryServicesRequests(device::BluetoothDevice* device);

  RenderProcessHost* GetRenderProcessHost();
  device::BluetoothAdapter* GetAdapter();
  url::Origin GetOrigin();
  BluetoothAllowedDevices& allowed_devices();

  void StoreAllowedScanOptions(
      const blink::mojom::WebBluetoothRequestLEScanOptions& options);
  bool AreScanFiltersAllowed(const absl::optional<ScanFilters>& filters) const;

  // Clears all state (maps, sets, etc).
  void ClearState();

  // Clears state associated with Bluetooth LE Scanning.
  void ClearAdvertisementClients();

  bool IsAllowedToAccessAtLeastOneService(
      const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessService(const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service);
  bool IsAllowedToAccessManufacturerData(
      const blink::WebBluetoothDeviceId& device_id,
      uint16_t manufacturer_code);

  // Returns true if at least |ble_scan_discovery_session_| or
  // |watch_advertisements_discovery_session_| is active.
  bool HasActiveDiscoverySession();

  // WebBluetoothPairingManagerDelegate implementation:
  blink::WebBluetoothDeviceId GetCharacteristicDeviceID(
      const std::string& characteristic_instance_id) override;
  void PairDevice(
      const blink::WebBluetoothDeviceId& device_id,
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      base::OnceClosure callback,
      device::BluetoothDevice::ConnectErrorCallback error_callback) override;

  // Used to open a BluetoothChooser and start a device discovery session.
  std::unique_ptr<BluetoothDeviceChooserController> device_chooser_controller_;

  // Used to open a BluetoothScanningPrompt.
  std::unique_ptr<BluetoothDeviceScanningPromptController>
      device_scanning_prompt_controller_;

  // Maps to get the object's parent based on its instanceID.
  std::unordered_map<std::string, std::string> service_id_to_device_address_;
  std::unordered_map<std::string, std::string> characteristic_id_to_service_id_;
  std::unordered_map<std::string, std::string>
      descriptor_id_to_characteristic_id_;

  // Map to keep track of the connected Bluetooth devices.
  std::unique_ptr<FrameConnectedBluetoothDevices> connected_devices_;

  // Maps a device address to callbacks that are waiting for services to
  // be discovered for that device.
  std::unordered_map<std::string, std::vector<PrimaryServicesRequestCallback>>
      pending_primary_services_requests_;

  // Map to keep track of the characteristics' notify sessions.
  std::unordered_map<std::string,
                     std::unique_ptr<GATTNotifySessionAndCharacteristicClient>>
      characteristic_id_to_notify_session_;

  // The RFH that owns this instance.
  RenderFrameHost* render_frame_host_;

  // Keeps track of our BLE scanning session.
  std::unique_ptr<device::BluetoothDiscoverySession>
      ble_scan_discovery_session_;

  // Keeps track of our watch advertisements discovery session.
  std::unique_ptr<device::BluetoothDiscoverySession>
      watch_advertisements_discovery_session_;

  // This queues up a scanning start callback so that we only have one
  // BluetoothDiscoverySession start request at a time for a BLE scan.
  RequestScanningStartCallback request_scanning_start_callback_;

  // This queues up pending watch advertisements callbacks and clients so that
  // we only have one BluetoothDiscoverySession start request at a time for
  // watching device advertisements.
  using WatchAdvertisementsCallbackAndClient =
      std::pair<WatchAdvertisementsForDeviceCallback,
                std::unique_ptr<WatchAdvertisementsClient>>;
  std::vector<WatchAdvertisementsCallbackAndClient>
      watch_advertisements_callbacks_and_clients_;

  // List of clients that we must broadcast scan changes to.
  std::vector<std::unique_ptr<ScanningClient>> scanning_clients_;
  std::vector<std::unique_ptr<WatchAdvertisementsClient>>
      watch_advertisements_clients_;

  // Allowed Bluetooth scanning filters.
  ScanFilters allowed_scan_filters_;

  // Whether a site has been allowed to receive all Bluetooth advertisement
  // packets.
  bool accept_all_advertisements_ = false;

  // The lifetime of this instance is exclusively managed by the RFH that owns
  // it so we use a "Receiver" as opposed to a "SelfOwnedReceiver" which deletes
  // the service on pipe connection errors.
  mojo::Receiver<blink::mojom::WebBluetoothService> receiver_;

  base::ScopedObservation<BluetoothDelegate,
                          BluetoothDelegate::FramePermissionObserver,
                          &BluetoothDelegate::AddFramePermissionObserver,
                          &BluetoothDelegate::RemoveFramePermissionObserver>
      observer_{this};

  WebBluetoothPairingManager pairing_manager_;
  base::WeakPtrFactory<WebBluetoothServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebBluetoothServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_
