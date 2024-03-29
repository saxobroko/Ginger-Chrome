// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps_base.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

namespace web_app {

class WebAppsBaseBrowserTest : public InProcessBrowserTest {
 public:
  WebAppsBaseBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kIntentHandlingSharing);
  }
  ~WebAppsBaseBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, LaunchWithIntent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/charts.html"));
  Profile* const profile = browser()->profile();
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  base::RunLoop run_loop;
  WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting(
          [&run_loop](apps::AppLaunchParams&& params) -> content::WebContents* {
            EXPECT_EQ(*params.intent->action, apps_util::kIntentActionSend);
            EXPECT_EQ(*params.intent->mime_type, "text/csv");
            EXPECT_EQ(params.intent->file_urls->size(), 1U);
            run_loop.Quit();
            return nullptr;
          }));

  std::vector<base::FilePath> file_paths(
      {chromeos::CrosDisksClient::GetArchiveMountPoint().Append(
          "numbers.csv")});
  std::vector<std::string> content_types({"text/csv"});
  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile, std::move(file_paths), std::move(content_types));
  const int32_t event_flags =
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      app_id, event_flags, std::move(intent),
      apps::mojom::LaunchSource::kFromSharesheet,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, IntentWithoutFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/poster.html"));
  Profile* const profile = browser()->profile();
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  base::RunLoop run_loop;
  WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting(
          [&run_loop](apps::AppLaunchParams&& params) -> content::WebContents* {
            EXPECT_EQ(*params.intent->action,
                      apps_util::kIntentActionSendMultiple);
            EXPECT_EQ(*params.intent->mime_type, "*/*");
            EXPECT_EQ(params.intent->file_urls->size(), 0U);
            run_loop.Quit();
            return nullptr;
          }));

  apps::mojom::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile, /*file_paths=*/std::vector<base::FilePath>(),
      /*mime_types=*/std::vector<std::string>(),
      /*share_text=*/"Message",
      /*share_title=*/"Subject");

  const int32_t event_flags =
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      app_id, event_flags, std::move(intent),
      apps::mojom::LaunchSource::kFromSharesheet,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, ExposeAppServicePublisherId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL("/web_apps/basic.html"));

  // Install file handling web app.
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  const WebAppRegistrar* registrar = WebAppProvider::Get(browser()->profile())
                                         ->registrar()
                                         .AsWebAppRegistrar();
  const WebApp* web_app = registrar->GetAppById(app_id);
  ASSERT_TRUE(web_app);

  // Check the publisher_id is the app's start url.
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&](const apps::AppUpdate& update) {
        EXPECT_EQ(web_app->start_url().spec(), update.PublisherId());
      });
}

IN_PROC_BROWSER_TEST_F(WebAppsBaseBrowserTest, LaunchAppIconKeyUnchanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL("/web_apps/basic.html"));
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());

  apps::mojom::IconKeyPtr original_key;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&original_key](const apps::AppUpdate& update) {
        original_key = update.IconKey().Clone();
      });

  const int32_t event_flags =
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  proxy->Launch(app_id, event_flags, apps::mojom::LaunchSource::kUnknown,
                apps::MakeWindowInfo(display::kDefaultDisplayId));
  proxy->FlushMojoCallsForTesting();

  proxy->AppRegistryCache().ForOneApp(
      app_id, [&original_key](const apps::AppUpdate& update) {
        ASSERT_EQ(original_key, update.IconKey());
      });
}

}  // namespace web_app
