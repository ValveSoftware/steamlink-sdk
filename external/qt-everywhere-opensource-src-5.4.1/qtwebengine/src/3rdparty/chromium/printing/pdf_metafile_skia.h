// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_SKIA_H_
#define PRINTING_PDF_METAFILE_SKIA_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "printing/metafile.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace printing {

struct PdfMetafileSkiaData;

// This class uses Skia graphics library to generate a PDF document.
class PRINTING_EXPORT PdfMetafileSkia : public Metafile {
 public:
  PdfMetafileSkia();
  virtual ~PdfMetafileSkia();

  // Metafile methods.
  virtual bool Init() OVERRIDE;
  virtual bool InitFromData(const void* src_buffer,
                            uint32 src_buffer_size) OVERRIDE;

  virtual SkBaseDevice* StartPageForVectorCanvas(
      const gfx::Size& page_size,
      const gfx::Rect& content_area,
      const float& scale_factor) OVERRIDE;

  virtual bool StartPage(const gfx::Size& page_size,
                         const gfx::Rect& content_area,
                         const float& scale_factor) OVERRIDE;
  virtual bool FinishPage() OVERRIDE;
  virtual bool FinishDocument() OVERRIDE;

  virtual uint32 GetDataSize() const OVERRIDE;
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const OVERRIDE;

  virtual bool SaveTo(const base::FilePath& file_path) const OVERRIDE;

  virtual gfx::Rect GetPageBounds(unsigned int page_number) const OVERRIDE;
  virtual unsigned int GetPageCount() const OVERRIDE;

  virtual gfx::NativeDrawingContext context() const OVERRIDE;

#if defined(OS_WIN)
  virtual bool Playback(gfx::NativeDrawingContext hdc,
                        const RECT* rect) const OVERRIDE;
  virtual bool SafePlayback(gfx::NativeDrawingContext hdc) const OVERRIDE;
  virtual HENHMETAFILE emf() const OVERRIDE;
#elif defined(OS_MACOSX)
  virtual bool RenderPage(unsigned int page_number,
                          gfx::NativeDrawingContext context,
                          const CGRect rect,
                          const MacRenderPageParams& params) const OVERRIDE;
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  virtual bool SaveToFD(const base::FileDescriptor& fd) const OVERRIDE;
#endif  // if defined(OS_CHROMEOS) || defined(OS_ANDROID)

  // Return a new metafile containing just the current page in draft mode.
  PdfMetafileSkia* GetMetafileForCurrentPage();

 private:
  scoped_ptr<PdfMetafileSkiaData> data_;

  // True when finish page is outstanding for current page.
  bool page_outstanding_;

  DISALLOW_COPY_AND_ASSIGN(PdfMetafileSkia);
};

}  // namespace printing

#endif  // PRINTING_PDF_METAFILE_SKIA_H_
