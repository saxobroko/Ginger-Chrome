// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

constexpr int kColumnSetId = 0;
constexpr int kIconSize = 16;
constexpr int kValuesLabelWidth = 190;

const gfx::VectorIcon& GetVectorIconForType(ServerFieldType type) {
  switch (type) {
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
      return kAccountCircleIcon;
    case ADDRESS_HOME_ADDRESS:
      return vector_icons::kLocationOnIcon;
    case EMAIL_ADDRESS:
      return vector_icons::kEmailIcon;
    case PHONE_HOME_WHOLE_NUMBER:
      return vector_icons::kCallIcon;
    default:
      NOTREACHED();
      return vector_icons::kLocationOnIcon;
  }
}

// Creates a view that displays all values in `diff`. `are_new_values`
// decides which set of values from `diff` are displayed.
std::unique_ptr<views::View> CreateValuesView(
    const std::vector<ProfileValueDifference>& diff,
    bool are_new_values,
    ui::NativeTheme::ColorId icon_color) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(
              /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL),
              /*horizontal=*/0));

  for (const ProfileValueDifference& diff_entry : diff) {
    const std::u16string& value =
        are_new_values ? diff_entry.first_value : diff_entry.second_value;
    // Don't add rows for empty original values.
    if (value.empty())
      continue;
    views::View* value_row =
        view->AddChildView(std::make_unique<views::View>());
    value_row->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets(
                /*vertical=*/0,
                /*horizontal=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

    auto icon_view = std::make_unique<views::ImageView>();
    icon_view->SetImage(ui::ImageModel::FromVectorIcon(
        GetVectorIconForType(diff_entry.type), icon_color, kIconSize));

    value_row->AddChildView(std::move(icon_view));
    auto label_view =
        std::make_unique<views::Label>(value, views::style::CONTEXT_LABEL);
    label_view->SetMultiLine(true);
    label_view->SizeToFit(kValuesLabelWidth);
    label_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    value_row->AddChildView(std::move(label_view));
  }
  return view;
}

// Add a row in `layout` that contains a label and a view displays all the
// values in `values`. Labels are added only if `show_row_label` is true.
void AddValuesRow(views::GridLayout* layout,
                  const std::vector<ProfileValueDifference>& diff,
                  bool show_row_label,
                  views::Button::PressedCallback edit_button_callback) {
  bool are_new_values = !!edit_button_callback;
  layout->StartRow(/*vertical_resize=*/views::GridLayout::kFixedSize,
                   kColumnSetId);

  if (show_row_label) {
    std::unique_ptr<views::Label> label(new views::Label(
        l10n_util::GetStringUTF16(
            are_new_values
                ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL
                : IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL),
        views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    layout->AddView(std::move(label), /*col_span=*/1, /*row_span=*/1,
                    /*h_align=*/views::GridLayout::LEADING,
                    /*v_align=*/views::GridLayout::LEADING);
  }
  ui::NativeTheme::ColorId icon_color =
      are_new_values ? ui::NativeTheme::kColorId_ProminentButtonColor
                     : ui::NativeTheme::kColorId_SecondaryIconColor;
  layout->AddView(CreateValuesView(diff, are_new_values, icon_color),
                  /*col_span=*/1,
                  /*row_span=*/1,
                  /*h_align=*/views::GridLayout::FILL,
                  /*v_align=*/views::GridLayout::FILL);
  if (are_new_values) {
    std::unique_ptr<views::ImageButton> edit_button =
        views::CreateVectorImageButtonWithNativeTheme(
            std::move(edit_button_callback), vector_icons::kEditIcon,
            kIconSize);

    edit_button->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
    edit_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
    layout->AddView(std::move(edit_button), /*col_span=*/1, /*row_span=*/1,
                    /*h_align=*/views::GridLayout::LEADING,
                    /*v_align=*/views::GridLayout::LEADING);
  }
}

// Returns true if there is there is at least one entry in `diff` with
// non-empty second value.
bool HasNonEmptySecondValues(const std::vector<ProfileValueDifference>& diff) {
  return base::ranges::any_of(diff, [](const ProfileValueDifference& entry) {
    return !entry.second_value.empty();
  });
}

// Returns true if there is an entry coressponding to type ADDRESS_HOME_ADDRESS.
bool HasAddressEntry(const std::vector<ProfileValueDifference>& diff) {
  return base::ranges::any_of(diff, [](const ProfileValueDifference& entry) {
    return entry.type == ADDRESS_HOME_ADDRESS;
  });
}

}  // namespace

UpdateAddressProfileView::UpdateAddressProfileView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveUpdateAddressProfileBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
  // Since this is an update prompt, original profile must be set. Otherwise, it
  // would have been a save prompt.
  DCHECK(controller_->GetOriginalProfile());

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  SetAcceptCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
  SetCancelCallback(base::BindOnce(
      &SaveUpdateAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));

  SetTitle(controller_->GetWindowTitle());
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL));

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(
              /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL),
              /*horizontal=*/0));

  std::vector<ProfileValueDifference> profile_diff = GetProfileDifferenceForUi(
      controller_->GetProfileToSave(), *controller_->GetOriginalProfile(),
      g_browser_process->GetApplicationLocale());

  std::u16string subtitle = GetProfileDescription(
      *controller_->GetOriginalProfile(),
      g_browser_process->GetApplicationLocale(),
      /*include_address_and_contacts=*/!HasAddressEntry(profile_diff));
  if (!subtitle.empty()) {
    views::Label* subtitle_label = AddChildView(std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    subtitle_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
  }

  views::View* main_content_view =
      AddChildView(std::make_unique<views::View>());

  bool has_non_empty_original_values = HasNonEmptySecondValues(profile_diff);

  // Build the GridLayout column set.
  views::GridLayout* layout = main_content_view->SetLayoutManager(
      std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  if (has_non_empty_original_values) {
    // Label column exists only if there is a section for original values.
    column_set->AddColumn(
        /*h_align=*/views::GridLayout::LEADING,
        /*v_align=*/views::GridLayout::LEADING,
        /*resize_percent=*/views::GridLayout::kFixedSize,
        /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
        /*fixed_width=*/0, /*min_width=*/0);
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize, column_divider);
  }
  column_set->AddColumn(
      /*h_align=*/views::GridLayout::FILL,
      /*v_align=*/views::GridLayout::FILL,
      /*resize_percent=*/1.0,
      /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0, /*min_width=*/0);
  column_set->AddColumn(
      /*h_align=*/views::GridLayout::LEADING,
      /*v_align=*/views::GridLayout::LEADING,
      /*resize_percent=*/views::GridLayout::kFixedSize,
      /*size_type=*/views::GridLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0, /*min_width=*/0);

  AddValuesRow(
      layout, profile_diff,
      /*show_row_label=*/has_non_empty_original_values,
      /*edit_button_callback=*/
      base::BindRepeating(
          &SaveUpdateAddressProfileBubbleController::OnEditButtonClicked,
          base::Unretained(controller_)));

  if (has_non_empty_original_values) {
    layout->AddPaddingRow(views::GridLayout::kFixedSize,
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));
    AddValuesRow(layout, profile_diff, /*show_row_label=*/true,
                 /*edit_button_callback=*/{});
  }
}

bool UpdateAddressProfileView::ShouldShowCloseButton() const {
  return true;
}

std::u16string UpdateAddressProfileView::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void UpdateAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void UpdateAddressProfileView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void UpdateAddressProfileView::Hide() {
  CloseBubble();

  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_)
    controller_->OnBubbleClosed();

  controller_ = nullptr;
}

}  // namespace autofill
