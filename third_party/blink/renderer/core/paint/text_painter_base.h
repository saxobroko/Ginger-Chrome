// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ComputedStyle;
class Document;
class GraphicsContext;
class GraphicsContextStateSaver;
class Node;
class SVGLengthContext;
struct PaintInfo;

// Base class for text painting. Has no dependencies on the layout tree and thus
// provides functionality and definitions that can be shared between both legacy
// layout and LayoutNG.
class CORE_EXPORT TextPainterBase {
  STACK_ALLOCATED();

 public:
  TextPainterBase(GraphicsContext&,
                  const Font&,
                  const PhysicalOffset& text_origin,
                  const PhysicalRect& text_frame_rect,
                  bool horizontal);
  ~TextPainterBase();

  virtual void ClipDecorationsStripe(float upper,
                                     float stripe_width,
                                     float dilation) = 0;

  void SetEmphasisMark(const AtomicString&, TextEmphasisPosition);
  void SetEllipsisOffset(int offset) { ellipsis_offset_ = offset; }

  enum ShadowMode { kBothShadowsAndTextProper, kShadowsOnly, kTextProperOnly };
  static void UpdateGraphicsContext(GraphicsContext&,
                                    const TextPaintStyle&,
                                    bool horizontal,
                                    GraphicsContextStateSaver&,
                                    ShadowMode = kBothShadowsAndTextProper);
  static sk_sp<SkDrawLooper> CreateDrawLooper(
      const ShadowList* shadow_list,
      DrawLooperBuilder::ShadowAlphaMode,
      const Color& current_color,
      mojom::blink::ColorScheme color_scheme,
      bool is_horizontal = true,
      ShadowMode = kBothShadowsAndTextProper);

  void PaintDecorationUnderOrOverLine(GraphicsContext&,
                                      TextDecorationInfo&,
                                      TextDecoration line);

  static Color TextColorForWhiteBackground(Color);
  static TextPaintStyle TextPaintingStyle(const Document&,
                                          const ComputedStyle&,
                                          const PaintInfo&);
  static TextPaintStyle SvgTextPaintingStyle(const Document&,
                                             const SVGLengthContext&,
                                             const ComputedStyle&,
                                             const PaintInfo&);
  static TextPaintStyle SelectionPaintingStyle(
      const Document&,
      const ComputedStyle&,
      Node*,
      const PaintInfo&,
      const TextPaintStyle& text_style);

  enum RotationDirection { kCounterclockwise, kClockwise };
  static AffineTransform Rotation(const PhysicalRect& box_rect,
                                  RotationDirection);
  static AffineTransform Rotation(const PhysicalRect& box_rect, WritingMode);

 protected:
  static void AdjustTextStyleForClip(TextPaintStyle&);
  static void AdjustTextStyleForPrint(const Document&,
                                      const ComputedStyle&,
                                      TextPaintStyle&);
  void UpdateGraphicsContext(const TextPaintStyle& style,
                             GraphicsContextStateSaver& saver) {
    UpdateGraphicsContext(graphics_context_, style, horizontal_, saver);
  }
  void DecorationsStripeIntercepts(
      float upper,
      float stripe_width,
      float dilation,
      const Vector<Font::TextIntercept>& text_intercepts);

  enum PaintInternalStep { kPaintText, kPaintEmphasisMark };

  GraphicsContext& graphics_context_;
  const Font& font_;
  const PhysicalOffset text_origin_;
  const PhysicalRect text_frame_rect_;
  AtomicString emphasis_mark_;
  int emphasis_mark_offset_ = 0;
  int ellipsis_offset_ = 0;
  const bool horizontal_;
};

inline AffineTransform TextPainterBase::Rotation(
    const PhysicalRect& box_rect,
    RotationDirection rotation_direction) {
  // Why this matrix is correct: consider the case of a clockwise rotation.

  // Let the corner points that define |boxRect| be ABCD, where A is top-left
  // and B is bottom-left.

  // 1. We want B to end up at the same pixel position after rotation as A is
  //    before rotation.
  // 2. Before rotation, B is at (x(), maxY())
  // 3. Rotating clockwise by 90 degrees places B at the coordinates
  //    (-maxY(), x()).
  // 4. Point A before rotation is at (x(), y())
  // 5. Therefore the translation from (3) to (4) is (x(), y()) - (-maxY(), x())
  //    = (x() + maxY(), y() - x())

  // A similar argument derives the counter-clockwise case.
  return rotation_direction == kClockwise
             ? AffineTransform(0, 1, -1, 0, box_rect.X() + box_rect.Bottom(),
                               box_rect.Y() - box_rect.X())
             : AffineTransform(0, -1, 1, 0, box_rect.X() - box_rect.Y(),
                               box_rect.X() + box_rect.Bottom());
}

inline AffineTransform TextPainterBase::Rotation(const PhysicalRect& box_rect,
                                                 WritingMode writing_mode) {
  return Rotation(box_rect, writing_mode != WritingMode::kSidewaysLr
                                ? TextPainterBase::kClockwise
                                : TextPainterBase::kCounterclockwise);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_BASE_H_
