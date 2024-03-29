// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_uninstall_handler.h"

#include "base/containers/flat_set.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {

namespace {

void OnRemoveStaleShillService(std::string service_path, bool success) {
  if (success) {
    NET_LOG(EVENT)
        << "Successfully removed stale Shill eSIM configuration. service_path="
        << service_path;
  } else {
    NET_LOG(ERROR)
        << "Error removing stale Shill eSIM configuration. service_path="
        << service_path;
  }
}

}  // namespace

CellularESimUninstallHandler::UninstallRequest::UninstallRequest(
    const std::string& iccid,
    const absl::optional<dbus::ObjectPath>& esim_profile_path,
    const absl::optional<dbus::ObjectPath>& euicc_path,
    UninstallRequestCallback callback)
    : iccid(iccid),
      esim_profile_path(esim_profile_path),
      euicc_path(euicc_path),
      callback(std::move(callback)) {}
CellularESimUninstallHandler::UninstallRequest::~UninstallRequest() = default;

CellularESimUninstallHandler::CellularESimUninstallHandler() = default;
CellularESimUninstallHandler::~CellularESimUninstallHandler() {
  cellular_esim_profile_handler_->RemoveObserver(this);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CellularESimUninstallHandler::Init(
    CellularInhibitor* cellular_inhibitor,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler) {
  cellular_inhibitor_ = cellular_inhibitor;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  network_configuration_handler_ = network_configuration_handler;
  network_connection_handler_ = network_connection_handler;
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);
  cellular_esim_profile_handler_->AddObserver(this);
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::UninstallESim(
    const std::string& iccid,
    const dbus::ObjectPath& esim_profile_path,
    const dbus::ObjectPath& euicc_path,
    UninstallRequestCallback callback) {
  uninstall_requests_.push_back(std::make_unique<UninstallRequest>(
      iccid, esim_profile_path, euicc_path, std::move(callback)));
  ProcessPendingUninstallRequests();
}

void CellularESimUninstallHandler::OnESimProfileListUpdated() {
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::NetworkListChanged() {
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::DevicePropertiesUpdated(
    const DeviceState* device_state) {
  CheckStaleESimServices();
}

void CellularESimUninstallHandler::ProcessPendingUninstallRequests() {
  // No requests to process.
  if (uninstall_requests_.empty())
    return;

  // Another uninstall request is in progress. Do not process a new request
  // until the previous one has completed
  if (state_ != UninstallState::kIdle)
    return;

  NET_LOG(EVENT) << "Starting eSIM uninstall. ICCID: "
                 << GetIccidForCurrentRequest();
  TransitionToUninstallState(UninstallState::kCheckingNetworkState);
  CheckNetworkState();
}

void CellularESimUninstallHandler::TransitionToUninstallState(
    UninstallState next_state) {
  NET_LOG(EVENT) << "CellularESimUninstallHandler state: " << state_ << " => "
                 << next_state;
  state_ = next_state;
}

void CellularESimUninstallHandler::CompleteCurrentRequest(bool success) {
  DCHECK(state_ != UninstallState::kIdle);

  NET_LOG(EVENT) << "Completed uninstall request for ICCID "
                 << GetIccidForCurrentRequest() << ". Success = " << success;
  std::move(uninstall_requests_.front()->callback).Run(success);
  uninstall_requests_.pop_front();

  TransitionToUninstallState(UninstallState::kIdle);
  ProcessPendingUninstallRequests();
}

const std::string& CellularESimUninstallHandler::GetIccidForCurrentRequest()
    const {
  return uninstall_requests_.front()->iccid;
}

const NetworkState*
CellularESimUninstallHandler::GetNetworkStateForCurrentRequest() const {
  for (auto* const network : GetESimCellularNetworks()) {
    if (network->iccid() == GetIccidForCurrentRequest()) {
      return network;
    }
  }

  return nullptr;
}

void CellularESimUninstallHandler::CheckNetworkState() {
  DCHECK_EQ(state_, UninstallState::kCheckingNetworkState);

  const NetworkState* network = GetNetworkStateForCurrentRequest();
  if (!network) {
    NET_LOG(ERROR) << "Unable to find eSIM network with ICCID "
                   << GetIccidForCurrentRequest();
    CompleteCurrentRequest(/*success=*/false);
    return;
  }

  // If there is no profile path in the request then this is a stale service.
  // Skip directly to configuration removal.
  if (!uninstall_requests_.front()->esim_profile_path) {
    TransitionToUninstallState(UninstallState::kRemovingShillService);
    AttemptRemoveShillService();
    return;
  }

  // If the network is connected, disconnect it before we attempt to uninstall
  // the associated profile.
  if (network->IsConnectedState()) {
    TransitionToUninstallState(UninstallState::kDisconnectingNetwork);
    AttemptNetworkDisconnect(network);
    return;
  }

  TransitionToUninstallState(UninstallState::kInhibitingShill);
  AttemptShillInhibit();
}

void CellularESimUninstallHandler::AttemptNetworkDisconnect(
    const NetworkState* network) {
  DCHECK_EQ(state_, UninstallState::kDisconnectingNetwork);

  network_connection_handler_->DisconnectNetwork(
      network->path(),
      base::BindOnce(&CellularESimUninstallHandler::OnDisconnectSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CellularESimUninstallHandler::OnDisconnectFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnDisconnectSuccess() {
  DCHECK_EQ(state_, UninstallState::kDisconnectingNetwork);

  TransitionToUninstallState(UninstallState::kInhibitingShill);
  AttemptShillInhibit();
}

void CellularESimUninstallHandler::OnDisconnectFailure(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  DCHECK_EQ(state_, UninstallState::kDisconnectingNetwork);

  NET_LOG(ERROR) << "Failed disconnecting network with ICCID "
                 << GetIccidForCurrentRequest();
  CompleteCurrentRequest(/*success=*/false);
}

void CellularESimUninstallHandler::AttemptShillInhibit() {
  DCHECK_EQ(state_, UninstallState::kInhibitingShill);

  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRemovingProfile,
      base::BindOnce(&CellularESimUninstallHandler::OnShillInhibit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnShillInhibit(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(state_, UninstallState::kInhibitingShill);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhbiting Shill during uninstall for ICCID "
                   << GetIccidForCurrentRequest();
    CompleteCurrentRequest(/*success=*/false);
    return;
  }

  // Save lock in the uninstall request so that it will be released when the
  // request is popped.
  uninstall_requests_.front()->inhibit_lock = std::move(inhibit_lock);

  TransitionToUninstallState(UninstallState::kRequestingInstalledProfiles);
  AttemptRequestInstalledProfiles();
}

void CellularESimUninstallHandler::AttemptRequestInstalledProfiles() {
  DCHECK_EQ(state_, UninstallState::kRequestingInstalledProfiles);

  cellular_esim_profile_handler_->RefreshProfileList(
      *uninstall_requests_.front()->euicc_path,
      base::BindOnce(&CellularESimUninstallHandler::OnRefreshProfileListResult,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(uninstall_requests_.front()->inhibit_lock));
}

void CellularESimUninstallHandler::OnRefreshProfileListResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK_EQ(state_, UninstallState::kRequestingInstalledProfiles);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error refreshing profile list during uninstall for "
                   << "ICCID " << GetIccidForCurrentRequest();
    CompleteCurrentRequest(/*success=*/false);
    return;
  }

  // Save lock back to the uninstall request since we will continue to perform
  // additional eSIM operations.
  uninstall_requests_.front()->inhibit_lock = std::move(inhibit_lock);

  TransitionToUninstallState(UninstallState::kDisablingProfile);
  AttemptDisableProfile();
}

void CellularESimUninstallHandler::AttemptDisableProfile() {
  DCHECK_EQ(state_, UninstallState::kDisablingProfile);
  HermesProfileClient::Get()->DisableCarrierProfile(
      *uninstall_requests_.front()->esim_profile_path,
      base::BindOnce(&CellularESimUninstallHandler::OnDisableProfile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnDisableProfile(
    HermesResponseStatus status) {
  DCHECK_EQ(state_, UninstallState::kDisablingProfile);

  bool success = status == HermesResponseStatus::kSuccess ||
                 status == HermesResponseStatus::kErrorAlreadyDisabled;
  if (!success) {
    NET_LOG(ERROR) << "Failed to disable profile for ICCID "
                   << GetIccidForCurrentRequest();
    CompleteCurrentRequest(/*success=*/false);
    return;
  }

  TransitionToUninstallState(UninstallState::kUninstallingProfile);
  AttemptUninstallProfile();
}

void CellularESimUninstallHandler::AttemptUninstallProfile() {
  DCHECK_EQ(state_, UninstallState::kUninstallingProfile);

  HermesEuiccClient::Get()->UninstallProfile(
      *uninstall_requests_.front()->euicc_path,
      *uninstall_requests_.front()->esim_profile_path,
      base::BindOnce(&CellularESimUninstallHandler::OnUninstallProfile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnUninstallProfile(
    HermesResponseStatus status) {
  DCHECK_EQ(state_, UninstallState::kUninstallingProfile);

  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Failed to uninstall profile for ICCID "
                   << GetIccidForCurrentRequest();
    CompleteCurrentRequest(/*success=*/false);
    return;
  }

  TransitionToUninstallState(UninstallState::kRemovingShillService);
  AttemptRemoveShillService();
}

void CellularESimUninstallHandler::AttemptRemoveShillService() {
  DCHECK_EQ(state_, UninstallState::kRemovingShillService);

  const NetworkState* network = GetNetworkStateForCurrentRequest();
  if (!network) {
    NET_LOG(ERROR) << "Unable to find eSIM network with ICCID "
                   << GetIccidForCurrentRequest();
    CompleteCurrentRequest(/*success=*/false);
    return;
  }

  // Return success immediately for non-shill eSIM cellular networks since we
  // don't know the actual shill service path. This stub non-shill service will
  // be removed automatically when the eSIM profile list updates.
  if (network->IsNonShillCellularNetwork()) {
    CompleteCurrentRequest(/*success=*/true);
    return;
  }

  network_configuration_handler_->RemoveConfiguration(
      network->path(), absl::nullopt,
      base::BindOnce(&CellularESimUninstallHandler::OnRemoveServiceSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CellularESimUninstallHandler::OnRemoveServiceFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CellularESimUninstallHandler::OnRemoveServiceSuccess() {
  DCHECK_EQ(state_, UninstallState::kRemovingShillService);
  CompleteCurrentRequest(/*success=*/true);
}

void CellularESimUninstallHandler::OnRemoveServiceFailure(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  DCHECK_EQ(state_, UninstallState::kRemovingShillService);
  NET_LOG(ERROR) << "Error removing service with ICCID "
                 << GetIccidForCurrentRequest() << ". Error: " << error_name;
  CompleteCurrentRequest(/*success=*/false);
}

void CellularESimUninstallHandler::CheckStaleESimServices() {
  // Find all known eSIM ICCIDs.
  base::flat_set<std::string> esim_iccids;
  std::vector<CellularESimProfile> esim_profiles =
      cellular_esim_profile_handler_->GetESimProfiles();
  for (const CellularESimProfile& esim_profile : esim_profiles) {
    // Skip pending and installing profiles since they can never have
    // a corresponding Shill service.
    if (esim_profile.state() == CellularESimProfile::State::kPending ||
        esim_profile.state() == CellularESimProfile::State::kInstalling) {
      continue;
    }

    esim_iccids.insert(esim_profile.iccid());
  }

  for (const NetworkState* network_state : GetESimCellularNetworks()) {
    if (esim_iccids.contains(network_state->iccid()))
      continue;

    // If we have not yet refreshed profiles for this EUICC, do not attempt to
    // remove the service. This ensures that we don't accidentally remove
    // services for eSIM profile which are in the process of being refreshed.
    // See b/186753024 for details.
    if (!cellular_esim_profile_handler_->HasRefreshedProfilesForEuicc(
            network_state->eid())) {
      NET_LOG(DEBUG) << "No eSIM profile for Shill service with ICCID "
                     << network_state->iccid() << ". Not counted as stale "
                     << "because we have not yet refreshed profiles from the "
                     << "associated EUICC";
      continue;
    }

    // Skip if an uninstall request is already queued for this service.
    if (HasQueuedRequest(network_state->iccid()))
      continue;

    NET_LOG(EVENT) << "Queueing removal for stale shill config. iccid="
                   << network_state->iccid() << ", "
                   << "network path=" << network_state->path();
    uninstall_requests_.push_back(std::make_unique<UninstallRequest>(
        network_state->iccid(), /*esim_profile_path=*/absl::nullopt,
        /*euicc_path=*/absl::nullopt,
        base::BindOnce(&OnRemoveStaleShillService, network_state->path())));
  }
  ProcessPendingUninstallRequests();
}

NetworkStateHandler::NetworkStateList
CellularESimUninstallHandler::GetESimCellularNetworks() const {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(), /*configured_only=*/false,
      /*visible_only=*/false, /*limit=*/0, &network_list);

  for (auto iter = network_list.begin(); iter != network_list.end();) {
    const NetworkState* network_state = *iter;
    if (network_state->eid().empty()) {
      iter = network_list.erase(iter);
    } else {
      iter++;
    }
  }
  return network_list;
}

bool CellularESimUninstallHandler::HasQueuedRequest(
    const std::string& iccid) const {
  const auto iter = std::find_if(
      uninstall_requests_.begin(), uninstall_requests_.end(),
      [&](const std::unique_ptr<UninstallRequest>& uninstall_request) {
        return uninstall_request->iccid == iccid;
      });
  return iter != uninstall_requests_.end();
}

std::ostream& operator<<(
    std::ostream& stream,
    const CellularESimUninstallHandler::UninstallState& state) {
  switch (state) {
    case CellularESimUninstallHandler::UninstallState::kIdle:
      stream << "[Idle]";
      break;
    case CellularESimUninstallHandler::UninstallState::kCheckingNetworkState:
      stream << "[Checking network state]";
      break;
    case CellularESimUninstallHandler::UninstallState::kInhibitingShill:
      stream << "[Inhibiting Shill]";
      break;
    case CellularESimUninstallHandler::UninstallState::
        kRequestingInstalledProfiles:
      stream << "[Requesting Installed Profiles]";
      break;
    case CellularESimUninstallHandler::UninstallState::kDisconnectingNetwork:
      stream << "[Disconnecting Network]";
      break;
    case CellularESimUninstallHandler::UninstallState::kDisablingProfile:
      stream << "[Disabling Profile]";
      break;
    case CellularESimUninstallHandler::UninstallState::kUninstallingProfile:
      stream << "[Uninstalling Profile]";
      break;
    case CellularESimUninstallHandler::UninstallState::kRemovingShillService:
      stream << "[Removing Shill Service]";
      break;
  }
  return stream;
}

}  // namespace chromeos