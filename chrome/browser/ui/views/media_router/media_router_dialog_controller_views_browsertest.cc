// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace media_router {

std::unique_ptr<StartPresentationContext> CreateStartPresentationContext() {
  return std::make_unique<StartPresentationContext>(
      content::PresentationRequest(
          {1, 2}, {GURL("http://example.com"), GURL("http://example2.com")},
          url::Origin::Create(GURL("http://google.com"))),
      base::DoNothing(), base::DoNothing());
}

class MediaRouterDialogControllerViewsTest : public InProcessBrowserTest {
 public:
  MediaRouterDialogControllerViewsTest() = default;
  ~MediaRouterDialogControllerViewsTest() override = default;

  void OpenMediaRouterDialog();
  void CreateDialogController();

 protected:
  WebContents* initiator_;
  MediaRouterDialogControllerViews* dialog_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerViewsTest);
};

class GlobalMediaControlsDialogTest
    : public MediaRouterDialogControllerViewsTest {
 public:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
    feature_list_.InitWithFeatures(
        {media_router::kGlobalMediaControlsCastStartStop}, {});
  }
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

void MediaRouterDialogControllerViewsTest::CreateDialogController() {
  // Start with one window with one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator contents.
  initiator_ = browser()->tab_strip_model()->GetActiveWebContents();

  dialog_controller_ = static_cast<MediaRouterDialogControllerViews*>(
      MediaRouterDialogController::GetOrCreateForWebContents(initiator_));
  ASSERT_TRUE(dialog_controller_);
}

void MediaRouterDialogControllerViewsTest::OpenMediaRouterDialog() {
  CreateDialogController();
  // Show the media router dialog for the initiator.
  dialog_controller_->ShowMediaRouterDialog(MediaRouterDialogOpenOrigin::PAGE);
  ASSERT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());
}

// Create/Get a media router dialog for initiator.
IN_PROC_BROWSER_TEST_F(MediaRouterDialogControllerViewsTest,
                       OpenCloseMediaRouterDialog) {
  OpenMediaRouterDialog();
  views::Widget* widget = CastDialogView::GetCurrentDialogWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->HasObserver(dialog_controller_));
  dialog_controller_->CloseMediaRouterDialog();
  EXPECT_FALSE(dialog_controller_->IsShowingMediaRouterDialog());
  EXPECT_EQ(CastDialogView::GetCurrentDialogWidget(), nullptr);
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsDialogTest,
                       ActivateInitiatorBeforeDialogOpen) {
  CreateDialogController();

  // Create a new foreground tab that covers |web_contents|.
  GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_NE(initiator_, browser()->tab_strip_model()->GetActiveWebContents());

  // |initiator_| should become active after the GMC dialog is open.
  dialog_controller_->ShowMediaRouterDialogForPresentation(
      CreateStartPresentationContext());
  ASSERT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());
  ASSERT_EQ(initiator_, browser()->tab_strip_model()->GetActiveWebContents());
}

}  // namespace media_router
