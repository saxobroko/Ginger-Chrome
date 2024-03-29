// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_

#include "base/run_loop.h"
#include "chrome/browser/web_applications/components/externally_managed_app_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

class WebAppRegistrationWaiter {
 public:
  explicit WebAppRegistrationWaiter(ExternallyManagedAppManager* manager);
  ~WebAppRegistrationWaiter();

  void AwaitNextRegistration(const GURL& install_url,
                             RegistrationResultCode code);

  void AwaitNextNonFailedRegistration(const GURL& install_url);

  void AwaitRegistrationsComplete();

 private:
  ExternallyManagedAppManager* const manager_;
  base::RunLoop run_loop_;
  GURL install_url_;
  // If unset then check for any non failure result.
  absl::optional<RegistrationResultCode> code_;

  base::RunLoop complete_run_loop_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_
