// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_CG_MAC_H_
#define PRINTING_PDF_METAFILE_CG_MAC_H_

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_checker.h"
#include "printing/metafile.h"

namespace base {
class FilePath;
}

namespace gfx {
class Rect;
class Size;
}

namespace printing {

// This class creates a graphics context that renders into a PDF data stream.
class PRINTING_EXPORT PdfMetafileCg : public Metafile {
 public:
  PdfMetafileCg();
  virtual ~PdfMetafileCg();

  // Metafile methods.
  virtual bool Init() OVERRIDE;
  virtual bool InitFromData(const void* src_buffer,
                            uint32 src_buffer_size) OVERRIDE;

  // Not implemented on mac.
  virtual SkBaseDevice* StartPageForVectorCanvas(
      const gfx::Size& page_size, const gfx::Rect& content_area,
      const float& scale_factor) OVERRIDE;
  virtual bool StartPage(const gfx::Size& page_size,
                         const gfx::Rect& content_area,
                         const float& scale_factor) OVERRIDE;
  virtual bool FinishPage() OVERRIDE;
  virtual bool FinishDocument() OVERRIDE;

  virtual uint32 GetDataSize() const OVERRIDE;
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const OVERRIDE;

  // For testing purposes only.
  virtual bool SaveTo(const base::FilePath& file_path) const OVERRIDE;

  virtual gfx::Rect GetPageBounds(unsigned int page_number) const OVERRIDE;
  virtual unsigned int GetPageCount() const OVERRIDE;

  // Note: The returned context *must not be retained* past Close(). If it is,
  // the data returned from GetData will not be valid PDF data.
  virtual CGContextRef context() const OVERRIDE;

  virtual bool RenderPage(unsigned int page_number,
                          gfx::NativeDrawingContext context,
                          const CGRect rect,
                          const MacRenderPageParams& params) const OVERRIDE;

 private:
  // Returns a CGPDFDocumentRef version of pdf_data_.
  CGPDFDocumentRef GetPDFDocument() const;

  base::ThreadChecker thread_checker_;

  // Context for rendering to the pdf.
  base::ScopedCFTypeRef<CGContextRef> context_;

  // PDF backing store.
  base::ScopedCFTypeRef<CFMutableDataRef> pdf_data_;

  // Lazily-created CGPDFDocument representation of pdf_data_.
  mutable base::ScopedCFTypeRef<CGPDFDocumentRef> pdf_doc_;

  // Whether or not a page is currently open.
  bool page_is_open_;

  // Whether this instantiation of the PdfMetafileCg owns the thread_pdf_docs.
  bool thread_pdf_docs_owned_;

  DISALLOW_COPY_AND_ASSIGN(PdfMetafileCg);
};

}  // namespace printing

#endif  // PRINTING_PDF_METAFILE_CG_MAC_H_
