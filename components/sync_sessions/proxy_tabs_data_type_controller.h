// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_PROXY_TABS_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SESSIONS_PROXY_TABS_DATA_TYPE_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/sync/driver/data_type_controller.h"

namespace sync_sessions {

// Controller for PROXY_TABS. Proxy tabs have no representation in sync, and
// therefore processor or worker.
class ProxyTabsDataTypeController : public syncer::DataTypeController {
 public:
  // |state_changed_cb| can be used to listen to state changes.
  explicit ProxyTabsDataTypeController(
      const base::RepeatingCallback<void(State)>& state_changed_cb);
  ~ProxyTabsDataTypeController() override;

  // DataTypeController interface.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  ActivateDataTypeResult ActivateDataType(
      syncer::ModelTypeConfigurer* configurer) override;
  void Stop(syncer::ShutdownReason shutdown_reason,
            StopCallback callback) override;
  State state() const override;
  bool ShouldRunInTransportOnlyMode() const override;
  void DeactivateDataType(syncer::ModelTypeConfigurer* configurer) override;
  void GetAllNodes(AllNodesCallback callback) override;
  void GetTypeEntitiesCount(
      base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;

 private:
  const base::RepeatingCallback<void(State)> state_changed_cb_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(ProxyTabsDataTypeController);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_PROXY_TABS_DATA_TYPE_CONTROLLER_H_
