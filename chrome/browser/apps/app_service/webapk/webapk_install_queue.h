// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_QUEUE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_QUEUE_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/mojom/webapk.mojom-forward.h"
#include "components/arc/session/connection_observer.h"

class Profile;

namespace apps {

class WebApkInstallTask;

class WebApkInstallQueue
    : public arc::ConnectionObserver<arc::mojom::WebApkInstance> {
 public:
  explicit WebApkInstallQueue(Profile* profile);
  WebApkInstallQueue(const WebApkInstallQueue&) = delete;
  WebApkInstallQueue& operator=(const WebApkInstallQueue&) = delete;

  ~WebApkInstallQueue() override;

  void Install(const std::string& app_id);

  // arc::ConnectionObserver<arc::mojom::WebApkInstance> overrides:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  std::unique_ptr<WebApkInstallTask> PopTaskForTest();

 private:
  void PostMaybeStartNext();
  void MaybeStartNext();
  void OnInstallCompleted(bool success);

  Profile* profile_;
  base::circular_deque<std::unique_ptr<WebApkInstallTask>> pending_installs_;
  std::unique_ptr<WebApkInstallTask> current_install_;
  bool connection_ready_;

  base::WeakPtrFactory<WebApkInstallQueue> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_INSTALL_QUEUE_H_
