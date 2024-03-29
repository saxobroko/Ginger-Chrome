// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_GEOMETRY_CONVERSIONS_H_
#define PDF_PPAPI_MIGRATION_GEOMETRY_CONVERSIONS_H_

struct PP_FloatPoint;
struct PP_FloatRect;
struct PP_Point;
struct PP_Rect;
struct PP_Size;

namespace gfx {
class Point;
class PointF;
class Rect;
class RectF;
class Size;
}  // namespace gfx

namespace chrome_pdf {

gfx::Point PointFromPPPoint(const PP_Point& pp_point);

gfx::PointF PointFFromPPFloatPoint(const PP_FloatPoint& pp_point);

gfx::Rect RectFromPPRect(const PP_Rect& pp_rect);
PP_Rect PPRectFromRect(const gfx::Rect& rect);

PP_FloatRect PPFloatRectFromRectF(const gfx::RectF& rect);

gfx::Size SizeFromPPSize(const PP_Size& pp_size);
PP_Size PPSizeFromSize(const gfx::Size& size);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_GEOMETRY_CONVERSIONS_H_
