// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_cg_mac.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_local.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using base::ScopedCFTypeRef;

namespace {

// What is up with this ugly hack? <http://crbug.com/64641>, that's what.
// The bug: Printing certain PDFs crashes. The cause: When printing, the
// renderer process assembles pages one at a time, in PDF format, to send to the
// browser process. When printing a PDF, the PDF plugin returns output in PDF
// format. There is a bug in 10.5 and 10.6 (<rdar://9018916>,
// <http://www.openradar.me/9018916>) where reference counting is broken when
// drawing certain PDFs into PDF contexts. So at the high-level, a PdfMetafileCg
// is used to hold the destination context, and then about five layers down on
// the callstack, a PdfMetafileCg is used to hold the source PDF. If the source
// PDF is drawn into the destination PDF context and then released, accessing
// the destination PDF context will crash. So the outermost instantiation of
// PdfMetafileCg creates a pool for deeper instantiations to dump their used
// PDFs into rather than releasing them. When the top-level PDF is closed, then
// it's safe to clear the pool. A thread local is used to allow this to work in
// single-process mode. TODO(avi): This Apple bug appears fixed in 10.7; when
// 10.7 is the minimum required version for Chromium, remove this hack.

base::LazyInstance<base::ThreadLocalPointer<struct __CFSet> >::Leaky
    thread_pdf_docs = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace printing {

PdfMetafileCg::PdfMetafileCg()
    : page_is_open_(false),
      thread_pdf_docs_owned_(false) {
  if (!thread_pdf_docs.Pointer()->Get() &&
      base::mac::IsOSSnowLeopard()) {
    thread_pdf_docs_owned_ = true;
    thread_pdf_docs.Pointer()->Set(
        CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks));
  }
}

PdfMetafileCg::~PdfMetafileCg() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pdf_doc_ && thread_pdf_docs.Pointer()->Get()) {
    // Transfer ownership to the pool.
    CFSetAddValue(thread_pdf_docs.Pointer()->Get(), pdf_doc_);
  }

  if (thread_pdf_docs_owned_) {
    CFRelease(thread_pdf_docs.Pointer()->Get());
    thread_pdf_docs.Pointer()->Set(NULL);
  }
}

bool PdfMetafileCg::Init() {
  // Ensure that Init hasn't already been called.
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, 0));
  if (!pdf_data_.get()) {
    LOG(ERROR) << "Failed to create pdf data for metafile";
    return false;
  }
  ScopedCFTypeRef<CGDataConsumerRef> pdf_consumer(
      CGDataConsumerCreateWithCFData(pdf_data_));
  if (!pdf_consumer.get()) {
    LOG(ERROR) << "Failed to create data consumer for metafile";
    pdf_data_.reset(NULL);
    return false;
  }
  context_.reset(CGPDFContextCreate(pdf_consumer, NULL, NULL));
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create pdf context for metafile";
    pdf_data_.reset(NULL);
  }

  return true;
}

bool PdfMetafileCg::InitFromData(const void* src_buffer,
                                 uint32 src_buffer_size) {
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  if (!src_buffer || src_buffer_size == 0) {
    return false;
  }

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, src_buffer_size));
  CFDataAppendBytes(pdf_data_, static_cast<const UInt8*>(src_buffer),
                    src_buffer_size);

  return true;
}

SkBaseDevice* PdfMetafileCg::StartPageForVectorCanvas(
    const gfx::Size& page_size, const gfx::Rect& content_area,
    const float& scale_factor) {
  NOTIMPLEMENTED();
  return NULL;
}

bool PdfMetafileCg::StartPage(const gfx::Size& page_size,
                              const gfx::Rect& content_area,
                              const float& scale_factor) {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

  double height = page_size.height();
  double width = page_size.width();

  CGRect bounds = CGRectMake(0, 0, width, height);
  CGContextBeginPage(context_, &bounds);
  page_is_open_ = true;
  CGContextSaveGState(context_);

  // Move to the context origin.
  CGContextTranslateCTM(context_, content_area.x(), -content_area.y());

  // Flip the context.
  CGContextTranslateCTM(context_, 0, height);
  CGContextScaleCTM(context_, scale_factor, -scale_factor);

  return context_.get() != NULL;
}

bool PdfMetafileCg::FinishPage() {
  DCHECK(context_.get());
  DCHECK(page_is_open_);

  CGContextRestoreGState(context_);
  CGContextEndPage(context_);
  page_is_open_ = false;
  return true;
}

bool PdfMetafileCg::FinishDocument() {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

#ifndef NDEBUG
  // Check that the context will be torn down properly; if it's not, pdf_data_
  // will be incomplete and generate invalid PDF files/documents.
  if (context_.get()) {
    CFIndex extra_retain_count = CFGetRetainCount(context_.get()) - 1;
    if (extra_retain_count > 0) {
      LOG(ERROR) << "Metafile context has " << extra_retain_count
                 << " extra retain(s) on Close";
    }
  }
#endif
  CGPDFContextClose(context_.get());
  context_.reset(NULL);
  return true;
}

bool PdfMetafileCg::RenderPage(unsigned int page_number,
                               CGContextRef context,
                               const CGRect rect,
                               const MacRenderPageParams& params) const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  if (!pdf_doc) {
    LOG(ERROR) << "Unable to create PDF document from data";
    return false;
  }
  CGPDFPageRef pdf_page = CGPDFDocumentGetPage(pdf_doc, page_number);
  CGRect source_rect = CGPDFPageGetBoxRect(pdf_page, kCGPDFCropBox);
  float scaling_factor = 1.0;
  const bool source_is_landscape =
        (source_rect.size.width > source_rect.size.height);
  const bool dest_is_landscape = (rect.size.width > rect.size.height);
  const bool rotate =
      params.autorotate ? (source_is_landscape != dest_is_landscape) : false;
  const float source_width =
      rotate ? source_rect.size.height : source_rect.size.width;
  const float source_height =
      rotate ? source_rect.size.width : source_rect.size.height;

  // See if we need to scale the output.
  const bool scaling_needed =
      (params.shrink_to_fit && ((source_width > rect.size.width) ||
                                (source_height > rect.size.height))) ||
      (params.stretch_to_fit && ((source_width < rect.size.width) &&
                                 (source_height < rect.size.height)));
  if (scaling_needed) {
    float x_scaling_factor = rect.size.width / source_width;
    float y_scaling_factor = rect.size.height / source_height;
    scaling_factor = std::min(x_scaling_factor, y_scaling_factor);
  }
  // Some PDFs have a non-zero origin. Need to take that into account and align
  // the PDF to the origin.
  const float x_origin_offset = -1 * source_rect.origin.x;
  const float y_origin_offset = -1 * source_rect.origin.y;

  // If the PDF needs to be centered, calculate the offsets here.
  float x_offset = params.center_horizontally ?
      ((rect.size.width - (source_width * scaling_factor)) / 2) : 0;
  if (rotate)
    x_offset = -x_offset;

  float y_offset = params.center_vertically ?
      ((rect.size.height - (source_height * scaling_factor)) / 2) : 0;

  CGContextSaveGState(context);

  // The transform operations specified here gets applied in reverse order.
  // i.e. the origin offset translation happens first.
  // Origin is at bottom-left.
  CGContextTranslateCTM(context, x_offset, y_offset);
  if (rotate) {
    // After rotating by 90 degrees with the axis at the origin, the page
    // content is now "off screen". Shift it right to move it back on screen.
    CGContextTranslateCTM(context, rect.size.width, 0);
    // Rotates counter-clockwise by 90 degrees.
    CGContextRotateCTM(context, M_PI_2);
  }
  CGContextScaleCTM(context, scaling_factor, scaling_factor);
  CGContextTranslateCTM(context, x_origin_offset, y_origin_offset);

  CGContextDrawPDFPage(context, pdf_page);
  CGContextRestoreGState(context);

  return true;
}

unsigned int PdfMetafileCg::GetPageCount() const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  return pdf_doc ? CGPDFDocumentGetNumberOfPages(pdf_doc) : 0;
}

gfx::Rect PdfMetafileCg::GetPageBounds(unsigned int page_number) const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  if (!pdf_doc) {
    LOG(ERROR) << "Unable to create PDF document from data";
    return gfx::Rect();
  }
  if (page_number > GetPageCount()) {
    LOG(ERROR) << "Invalid page number: " << page_number;
    return gfx::Rect();
  }
  CGPDFPageRef pdf_page = CGPDFDocumentGetPage(pdf_doc, page_number);
  CGRect page_rect = CGPDFPageGetBoxRect(pdf_page, kCGPDFMediaBox);
  return gfx::Rect(page_rect);
}

uint32 PdfMetafileCg::GetDataSize() const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);

  if (!pdf_data_)
    return 0;
  return static_cast<uint32>(CFDataGetLength(pdf_data_));
}

bool PdfMetafileCg::GetData(void* dst_buffer, uint32 dst_buffer_size) const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);
  DCHECK(pdf_data_);
  DCHECK(dst_buffer);
  DCHECK_GT(dst_buffer_size, 0U);

  uint32 data_size = GetDataSize();
  if (dst_buffer_size > data_size) {
    return false;
  }

  CFDataGetBytes(pdf_data_, CFRangeMake(0, dst_buffer_size),
                 static_cast<UInt8*>(dst_buffer));
  return true;
}

bool PdfMetafileCg::SaveTo(const base::FilePath& file_path) const {
  DCHECK(pdf_data_.get());
  DCHECK(!context_.get());

  std::string path_string = file_path.value();
  ScopedCFTypeRef<CFURLRef> path_url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path_string.c_str()),
      path_string.length(), false));
  SInt32 error_code;
  CFURLWriteDataAndPropertiesToResource(path_url, pdf_data_, NULL, &error_code);
  return error_code == 0;
}

CGContextRef PdfMetafileCg::context() const {
  return context_.get();
}

CGPDFDocumentRef PdfMetafileCg::GetPDFDocument() const {
  // Make sure that we have data, and that it's not being modified any more.
  DCHECK(pdf_data_.get());
  DCHECK(!context_.get());

  if (!pdf_doc_.get()) {
    ScopedCFTypeRef<CGDataProviderRef> pdf_data_provider(
        CGDataProviderCreateWithCFData(pdf_data_));
    pdf_doc_.reset(CGPDFDocumentCreateWithProvider(pdf_data_provider));
  }
  return pdf_doc_.get();
}

}  // namespace printing
