// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_
#define PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "pdf/pdf_engine.h"

namespace chrome_pdf {

class PDFiumEngineExports : public PDFEngineExports {
 public:
  PDFiumEngineExports();
  PDFiumEngineExports(const PDFiumEngineExports&) = delete;
  PDFiumEngineExports& operator=(const PDFiumEngineExports&) = delete;
  ~PDFiumEngineExports() override;

// PDFEngineExports:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::vector<uint8_t> CreateFlattenedPdf(
      base::span<const uint8_t> input_buffer) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_WIN)
  bool RenderPDFPageToDC(base::span<const uint8_t> pdf_buffer,
                         int page_number,
                         const RenderingSettings& settings,
                         HDC dc) override;
  void SetPDFEnsureTypefaceCharactersAccessible(
      PDFEnsureTypefaceCharactersAccessible func) override;

  void SetPDFUseGDIPrinting(bool enable) override;
  void SetPDFUsePrintMode(int mode) override;
#endif  // defined(OS_WIN)
  bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                             int page_number,
                             const RenderingSettings& settings,
                             void* bitmap_buffer) override;
  std::vector<uint8_t> ConvertPdfPagesToNupPdf(
      std::vector<base::span<const uint8_t>> input_buffers,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area) override;
  std::vector<uint8_t> ConvertPdfDocumentToNupPdf(
      base::span<const uint8_t> input_buffer,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area) override;
  bool GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                     int* page_count,
                     float* max_page_width) override;
  absl::optional<bool> IsPDFDocTagged(
      base::span<const uint8_t> pdf_buffer) override;
  base::Value GetPDFStructTreeForPage(base::span<const uint8_t> pdf_buffer,
                                      int page_index) override;
  absl::optional<gfx::SizeF> GetPDFPageSizeByIndex(
      base::span<const uint8_t> pdf_buffer,
      int page_number) override;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_
