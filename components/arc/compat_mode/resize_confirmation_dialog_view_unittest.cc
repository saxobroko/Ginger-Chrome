// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_confirmation_dialog_view.h"

#include <memory>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

namespace arc {
namespace {

class ResizeConfirmationDialogViewTest : public views::ViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    widget_->SetBounds(gfx::Rect(800, 800));
    dialog_view_ =
        widget_->SetContentsView(std::make_unique<ResizeConfirmationDialogView>(
            base::BindOnce(&ResizeConfirmationDialogViewTest::OnClicked,
                           base::Unretained(this))));
    widget_->Show();
  }

  void TearDown() override {
    widget_->Close();
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void ClickDialogButton(bool accept, bool with_checkbox) {
    ResizeConfirmationDialogView::TestApi dialog_view_test(dialog_view_);
    if (with_checkbox)
      dialog_view_test.do_not_ask_checkbox()->SetChecked(true);

    auto* target_button = accept ? dialog_view_test.accept_button()
                                 : dialog_view_test.cancel_button();

    ui::test::EventGenerator event_generator(GetRootWindow(widget_.get()));
    event_generator.MoveMouseTo(
        target_button->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  }

  bool callback_called() { return callback_called_; }
  bool callback_accepted() { return callback_accepted_; }
  bool callback_do_not_ask_again() { return callback_do_not_ask_again_; }

 private:
  void OnClicked(bool accepted, bool do_not_ask_again) {
    callback_called_ = true;
    callback_accepted_ = accepted;
    callback_do_not_ask_again_ = do_not_ask_again;
  }

  // For callback checks.
  bool callback_called_{false};
  bool callback_accepted_{false};
  bool callback_do_not_ask_again_{false};

  // A LayoutProvider must exist in scope in order to set up views.
  views::LayoutProvider layout_provider;

  ResizeConfirmationDialogView* dialog_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(ResizeConfirmationDialogViewTest, ClickAcceptWithCheckbox) {
  ClickDialogButton(/*accept=*/true, /*with_checkbox=*/true);
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(callback_accepted());
  EXPECT_TRUE(callback_do_not_ask_again());
}

TEST_F(ResizeConfirmationDialogViewTest, ClickCancelWithCheckbox) {
  ClickDialogButton(/*accept=*/false, /*with_checkbox=*/true);
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(callback_accepted());
  EXPECT_TRUE(callback_do_not_ask_again());
}

TEST_F(ResizeConfirmationDialogViewTest, ClickAcceptWithoutCheckbox) {
  ClickDialogButton(/*accept=*/true, /*with_checkbox=*/false);
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(callback_accepted());
  EXPECT_FALSE(callback_do_not_ask_again());
}

TEST_F(ResizeConfirmationDialogViewTest, ClickCancelWithoutCheckbox) {
  ClickDialogButton(/*accept=*/false, /*with_checkbox=*/false);
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(callback_accepted());
  EXPECT_FALSE(callback_do_not_ask_again());
}

}  // namespace
}  // namespace arc
