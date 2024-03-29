// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DRAW_IMAGE_H_
#define CC_PAINT_DRAW_IMAGE_H_

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

// A DrawImage is a logical snapshot in time and space of a PaintImage.  It
// includes decisions about scaling, animation frame, final colorspace, etc.
// It has not been decoded yet.  DrawImage turns into DecodedDrawImage via
// ImageDecodeCache::GetDecodedImageForDraw during playback.
class CC_PAINT_EXPORT DrawImage {
 public:
  DrawImage();
  explicit DrawImage(PaintImage image);
  DrawImage(PaintImage image,
            bool use_dark_mode,
            const SkIRect& src_rect,
            SkFilterQuality filter_quality,
            const SkM44& matrix,
            absl::optional<size_t> frame_index = absl::nullopt,
            const absl::optional<gfx::ColorSpace>& color_space = absl::nullopt,
            float sdr_white_level = gfx::ColorSpace::kDefaultSDRWhiteLevel);
  // Constructs a DrawImage from |other| by adjusting its scale and setting a
  // new color_space.
  DrawImage(const DrawImage& other,
            float scale_adjustment,
            size_t frame_index,
            const gfx::ColorSpace& color_space,
            float sdr_white_level = gfx::ColorSpace::kDefaultSDRWhiteLevel);
  DrawImage(const DrawImage& other);
  DrawImage(DrawImage&& other);
  ~DrawImage();

  DrawImage& operator=(DrawImage&& other);
  DrawImage& operator=(const DrawImage& other);

  bool operator==(const DrawImage& other) const;

  const PaintImage& paint_image() const { return paint_image_; }
  bool use_dark_mode() const { return use_dark_mode_; }
  const SkSize& scale() const { return scale_; }
  const SkIRect& src_rect() const { return src_rect_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  bool matrix_is_decomposable() const { return matrix_is_decomposable_; }
  const gfx::ColorSpace& target_color_space() const {
    DCHECK(target_color_space_.has_value());
    return *target_color_space_;
  }
  PaintImage::FrameKey frame_key() const {
    return paint_image_.GetKeyForFrame(frame_index());
  }
  size_t frame_index() const {
    DCHECK(frame_index_.has_value());
    return frame_index_.value();
  }
  float sdr_white_level() const { return sdr_white_level_; }

 private:
  PaintImage paint_image_;
  bool use_dark_mode_;
  SkIRect src_rect_;
  SkFilterQuality filter_quality_;
  SkSize scale_;
  bool matrix_is_decomposable_;
  absl::optional<size_t> frame_index_;
  absl::optional<gfx::ColorSpace> target_color_space_;

  // The SDR white level in nits for the display. Only if |target_color_space_|
  // is HDR will this have a value other than kDefaultSDRWhiteLevel. Used by the
  // ImageDecodeCache to prevent HDR images from being affected by variable SDR
  // white levels since rasterization is always treated as SDR at present.
  float sdr_white_level_ = gfx::ColorSpace::kDefaultSDRWhiteLevel;
};

}  // namespace cc

#endif  // CC_PAINT_DRAW_IMAGE_H_
