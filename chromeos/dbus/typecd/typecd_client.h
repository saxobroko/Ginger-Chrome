// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_TYPECD_TYPECD_CLIENT_H_
#define CHROMEOS_DBUS_TYPECD_TYPECD_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// TypecdClient is responsible receiving D-bus signals from the TypeCDaemon
// service. The TypeCDaemon is the underlying service that informs us whenever
// a new Thunderbolt/USB4 peripheral has been plugged in.
class COMPONENT_EXPORT(TYPECD) TypecdClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnThunderboltDeviceConnected(bool is_thunderbolt_only) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Creates and initializes a global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static TypecdClient* Get();

 protected:
  // Initialize/Shutdown should be used instead.
  TypecdClient();

  TypecdClient(const TypecdClient&) = delete;
  TypecdClient& operator=(const TypecdClient&) = delete;
  virtual ~TypecdClient();

  void NotifyOnThunderboltDeviceConnected(bool is_thunderbolt_only);

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_TYPECD_TYPECD_CLIENT_H_
