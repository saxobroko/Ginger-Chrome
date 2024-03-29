// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

class NGFragmentItem;
struct NGTextFragmentPaintInfo;

// Text painter for LayoutNG, logic shared between legacy layout and LayoutNG
// is implemented in the TextPainterBase base class.
// Operates on NGPhysicalTextFragments and only paints text and decorations.
// Border painting etc is handled by the NGTextFragmentPainter class.
// TODO(layout-dev): Does this distinction make sense?
class CORE_EXPORT NGTextPainter : public TextPainterBase {
  STACK_ALLOCATED();

 public:
  NGTextPainter(GraphicsContext& context,
                const Font& font,
                const NGTextFragmentPaintInfo& fragment_paint_info,
                const IntRect& visual_rect,
                const PhysicalOffset& text_origin,
                const PhysicalRect& text_frame_rect,
                bool horizontal)
      : TextPainterBase(context,
                        font,
                        text_origin,
                        text_frame_rect,
                        horizontal),
        fragment_paint_info_(fragment_paint_info),
        visual_rect_(visual_rect) {}
  ~NGTextPainter() = default;

  void ClipDecorationsStripe(float upper,
                             float stripe_width,
                             float dilation) override;
  void Paint(unsigned start_offset,
             unsigned end_offset,
             unsigned length,
             const TextPaintStyle&,
             DOMNodeId,
             ShadowMode = kBothShadowsAndTextProper);

  void PaintSelectedText(unsigned start_offset,
                         unsigned end_offset,
                         unsigned length,
                         const TextPaintStyle& text_style,
                         const TextPaintStyle& selection_style,
                         const PhysicalRect& selection_rect,
                         DOMNodeId node_id);

  // Based on legacy TextPainter.
  void PaintDecorationsExceptLineThrough(
      const NGFragmentItem& text_item,
      const PaintInfo& paint_info,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      const PhysicalRect& decoration_rect,
      const absl::optional<AppliedTextDecoration>& selection_decoration,
      bool* has_line_through_decoration);

  // Based on legacy TextPainter.
  void PaintDecorationsOnlyLineThrough(
      const NGFragmentItem& text_item,
      const PaintInfo& paint_info,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      const PhysicalRect& decoration_rect,
      const absl::optional<AppliedTextDecoration>& selection_decoration);

 private:
  template <PaintInternalStep step>
  void PaintInternalFragment(unsigned from, unsigned to, DOMNodeId node_id);

  template <PaintInternalStep step>
  void PaintInternal(unsigned start_offset,
                     unsigned end_offset,
                     unsigned truncation_point,
                     DOMNodeId node_id);

  NGTextFragmentPaintInfo fragment_paint_info_;
  const IntRect& visual_rect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_
