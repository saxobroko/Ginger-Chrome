// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_POLICY_HEADLESS_BROWSER_POLICY_CONNECTOR_H_
#define HEADLESS_LIB_BROWSER_POLICY_HEADLESS_BROWSER_POLICY_CONNECTOR_H_

#include "base/memory/ref_counted.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

// Implements Headless Chrome specific browser policy connector.
class HeadlessBrowserPolicyConnector : public BrowserPolicyConnector {
 public:
  // Builds an uninitialized connector suitable for testing.
  // Init() should be called to create and start the policy machinery.
  HeadlessBrowserPolicyConnector();
  ~HeadlessBrowserPolicyConnector() override;

  HeadlessBrowserPolicyConnector(const HeadlessBrowserPolicyConnector&) =
      delete;
  HeadlessBrowserPolicyConnector& operator=(
      const HeadlessBrowserPolicyConnector&) = delete;

  scoped_refptr<PrefStore> CreatePrefStore(policy::PolicyLevel policy_level);

 protected:
  // BrowserPolicyConnectorBase::
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

 private:
  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  bool IsEnterpriseManaged() const override;

  bool IsCommandLineSwitchSupported() const override;

  bool HasMachineLevelPolicies() override;

  ConfigurationPolicyProvider* GetPlatformProvider();

  std::unique_ptr<ConfigurationPolicyProvider> CreatePlatformProvider();

  // Owned by the base class.
  ConfigurationPolicyProvider* platform_provider_ = nullptr;
};

}  // namespace policy

#endif  // HEADLESS_LIB_BROWSER_POLICY_HEADLESS_BROWSER_POLICY_CONNECTOR_H_