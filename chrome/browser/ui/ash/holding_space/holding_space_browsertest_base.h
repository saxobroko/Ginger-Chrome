// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"

class Profile;

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceTestApi;

// Base class for holding space browser tests. Subclasses
// SystemWebAppBrowserTestBase for the ability to test with the Media App, which
// is the default handler for files opened from the holding space.
class HoldingSpaceBrowserTestBase
    : public web_app::SystemWebAppBrowserTestBase {
 public:
  HoldingSpaceBrowserTestBase();
  ~HoldingSpaceBrowserTestBase() override;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  // Returns the root window that newly created windows should be added to.
  static aura::Window* GetRootWindowForNewWindows();

  // Returns the currently active profile.
  Profile* GetProfile();

  // Adds and returns an arbitrary download file to the holding space.
  HoldingSpaceItem* AddDownloadFile();

  // Adds and returns an arbitrary nearby share file to the holding space.
  HoldingSpaceItem* AddNearbyShareFile();

  // Adds and returns an arbitrary pinned file to the holding space.
  HoldingSpaceItem* AddPinnedFile();

  // Adds and returns an arbitrary screenshot file to the holding space.
  HoldingSpaceItem* AddScreenshotFile();

  // Adds and returns an arbitrary screen recording file to the holding space.
  HoldingSpaceItem* AddScreenRecordingFile();

  // Adds and returns a holding space item of the specified `type` backed by the
  // file at the specified `file_path`.
  HoldingSpaceItem* AddItem(Profile* profile,
                            HoldingSpaceItem::Type type,
                            const base::FilePath& file_path);

  // Removes the specified holding space `item`.
  void RemoveItem(const HoldingSpaceItem* item);

  // Creates a file at the root of the Downloads mount point with the specified
  // extension. If extension is omitted, the created file will have an extension
  // of `.txt`. Returns the file path of the created file.
  base::FilePath CreateFile(
      const absl::optional<std::string>& extension = absl::nullopt);

  // Requests lock screen, waiting to return until session state is locked.
  void RequestAndAwaitLockScreen();

  // Returns the holding space test API.
  HoldingSpaceTestApi& test_api() { return *test_api_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<HoldingSpaceTestApi> test_api_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_BROWSERTEST_BASE_H_
