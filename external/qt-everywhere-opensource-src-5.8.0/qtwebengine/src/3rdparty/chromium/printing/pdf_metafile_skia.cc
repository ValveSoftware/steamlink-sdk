// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_skia.h"

#include "base/files/file.h"
#include "base/time/time.h"
#include "printing/print_settings.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
// Note that headers in third_party/skia/src are fragile.  This is
// an experimental, fragile, and diagnostic-only document type.
#include "third_party/skia/src/utils/SkMultiPictureDocument.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_MACOSX)
#include "printing/pdf_metafile_cg_mac.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {

bool WriteAssetToBuffer(const SkStreamAsset* asset,
                        void* buffer,
                        size_t size) {
  // Calling duplicate() keeps original asset state unchanged.
  std::unique_ptr<SkStreamAsset> assetCopy(asset->duplicate());
  size_t length = assetCopy->getLength();
  if (length > size)
    return false;
  return (length == assetCopy->read(buffer, length));
}

SkTime::DateTime TimeToSkTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  SkTime::DateTime skdate;
  skdate.fTimeZoneMinutes = 0;
  skdate.fYear = exploded.year;
  skdate.fMonth = exploded.month;
  skdate.fDayOfWeek = exploded.day_of_week;
  skdate.fDay = exploded.day_of_month;
  skdate.fHour = exploded.hour;
  skdate.fMinute = exploded.minute;
  skdate.fSecond = exploded.second;
  return skdate;
}

sk_sp<SkDocument> MakePdfDocument(SkWStream* wStream) {
  SkDocument::PDFMetadata metadata;
  SkTime::DateTime now = TimeToSkTime(base::Time::Now());
  metadata.fCreation.fEnabled = true;
  metadata.fCreation.fDateTime = now;
  metadata.fModified.fEnabled = true;
  metadata.fModified.fDateTime = now;
  const std::string& agent = printing::GetAgent();
  metadata.fCreator = agent.empty() ? SkString("Chromium")
                                    : SkString(agent.c_str(), agent.size());
  return SkDocument::MakePDF(wStream, SK_ScalarDefaultRasterDPI, metadata,
                             nullptr, false);
}

}  // namespace

namespace printing {

struct Page {
  Page(SkSize s, sk_sp<SkPicture> c) : size_(s), content_(std::move(c)) {}
  Page(Page&& that) : size_(that.size_), content_(std::move(that.content_)) {}
  Page(const Page&) = default;
  Page& operator=(const Page&) = default;
  Page& operator=(Page&& that) {
    size_ = that.size_;
    content_ = std::move(that.content_);
    return *this;
  }
  SkSize size_;
  sk_sp<SkPicture> content_;
};

struct PdfMetafileSkiaData {
  SkPictureRecorder recorder_;  // Current recording

  std::vector<Page> pages_;
  std::unique_ptr<SkStreamAsset> pdf_data_;

  // The scale factor is used because Blink occasionally calls
  // SkCanvas::getTotalMatrix() even though the total matrix is not as
  // meaningful for a vector canvas as for a raster canvas.
  float scale_factor_;
  SkSize size_;
  SkiaDocumentType type_;

#if defined(OS_MACOSX)
  PdfMetafileCg pdf_cg_;
#endif
};

PdfMetafileSkia::~PdfMetafileSkia() {}

bool PdfMetafileSkia::Init() {
  return true;
}

// TODO(halcanary): Create a Metafile class that only stores data.
// Metafile::InitFromData is orthogonal to what the rest of
// PdfMetafileSkia does.
bool PdfMetafileSkia::InitFromData(const void* src_buffer,
                                   uint32_t src_buffer_size) {
  data_->pdf_data_.reset(new SkMemoryStream(src_buffer, src_buffer_size, true));
  return true;
}

void PdfMetafileSkia::StartPage(const gfx::Size& page_size,
                                const gfx::Rect& content_area,
                                const float& scale_factor) {
  DCHECK_GT(page_size.width(), 0);
  DCHECK_GT(page_size.height(), 0);
  DCHECK_GT(scale_factor, 0.0f);
  if (data_->recorder_.getRecordingCanvas())
    FinishPage();
  DCHECK(!data_->recorder_.getRecordingCanvas());

  float inverse_scale = 1.0 / scale_factor;
  SkCanvas* canvas = data_->recorder_.beginRecording(
      inverse_scale * page_size.width(), inverse_scale * page_size.height());
  // Recording canvas is owned by the data_->recorder_.  No ref() necessary.
  if (content_area != gfx::Rect(page_size)) {
    canvas->scale(inverse_scale, inverse_scale);
    SkRect sk_content_area = gfx::RectToSkRect(content_area);
    canvas->clipRect(sk_content_area);
    canvas->translate(sk_content_area.x(), sk_content_area.y());
    canvas->scale(scale_factor, scale_factor);
  }

  data_->size_ = gfx::SizeFToSkSize(gfx::SizeF(page_size));
  data_->scale_factor_ = scale_factor;
  // We scale the recording canvas's size so that
  // canvas->getTotalMatrix() returns a value that ignores the scale
  // factor.  We store the scale factor and re-apply it later.
  // http://crbug.com/469656
}

SkCanvas* PdfMetafileSkia::GetVectorCanvasForNewPage(
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const float& scale_factor) {
  StartPage(page_size, content_area, scale_factor);
  return data_->recorder_.getRecordingCanvas();
}

bool PdfMetafileSkia::FinishPage() {
  if (!data_->recorder_.getRecordingCanvas())
    return false;

  sk_sp<SkPicture> pic = data_->recorder_.finishRecordingAsPicture();
  if (data_->scale_factor_ != 1.0f) {
    SkCanvas* canvas = data_->recorder_.beginRecording(data_->size_.width(),
                                                       data_->size_.height());
    canvas->scale(data_->scale_factor_, data_->scale_factor_);
    canvas->drawPicture(pic);
    pic = data_->recorder_.finishRecordingAsPicture();
  }
  data_->pages_.emplace_back(data_->size_, std::move(pic));
  return true;
}

bool PdfMetafileSkia::FinishDocument() {
  // If we've already set the data in InitFromData, leave it be.
  if (data_->pdf_data_)
    return false;

  if (data_->recorder_.getRecordingCanvas())
    FinishPage();

  SkDynamicMemoryWStream stream;
  sk_sp<SkDocument> doc;
  switch (data_->type_) {
    case PDF_SKIA_DOCUMENT_TYPE:
      doc = MakePdfDocument(&stream);
      break;
    case MSKP_SKIA_DOCUMENT_TYPE:
      doc = SkMakeMultiPictureDocument(&stream);
      break;
  }

  for (const Page& page : data_->pages_) {
    SkCanvas* canvas = doc->beginPage(page.size_.width(), page.size_.height());
    canvas->drawPicture(page.content_);
    doc->endPage();
  }
  if (!doc->close())
    return false;

  data_->pdf_data_.reset(stream.detachAsStream());
  return true;
}

uint32_t PdfMetafileSkia::GetDataSize() const {
  if (!data_->pdf_data_)
    return 0;
  return base::checked_cast<uint32_t>(data_->pdf_data_->getLength());
}

bool PdfMetafileSkia::GetData(void* dst_buffer,
                              uint32_t dst_buffer_size) const {
  if (!data_->pdf_data_)
    return false;
  return WriteAssetToBuffer(data_->pdf_data_.get(), dst_buffer,
                            base::checked_cast<size_t>(dst_buffer_size));
}

gfx::Rect PdfMetafileSkia::GetPageBounds(unsigned int page_number) const {
  if (page_number < data_->pages_.size()) {
    SkSize size = data_->pages_[page_number].size_;
    return gfx::Rect(gfx::ToRoundedInt(size.width()),
                     gfx::ToRoundedInt(size.height()));
  }
  return gfx::Rect();
}

unsigned int PdfMetafileSkia::GetPageCount() const {
  return base::checked_cast<unsigned int>(data_->pages_.size());
}

gfx::NativeDrawingContext PdfMetafileSkia::context() const {
  NOTREACHED();
  return nullptr;
}


#if defined(OS_WIN)
bool PdfMetafileSkia::Playback(gfx::NativeDrawingContext hdc,
                               const RECT* rect) const {
  NOTREACHED();
  return false;
}

bool PdfMetafileSkia::SafePlayback(gfx::NativeDrawingContext hdc) const {
  NOTREACHED();
  return false;
}

#elif defined(OS_MACOSX)
/* TODO(caryclark): The set up of PluginInstance::PrintPDFOutput may result in
   rasterized output.  Even if that flow uses PdfMetafileCg::RenderPage,
   the drawing of the PDF into the canvas may result in a rasterized output.
   PDFMetafileSkia::RenderPage should be not implemented as shown and instead
   should do something like the following CL in PluginInstance::PrintPDFOutput:
http://codereview.chromium.org/7200040/diff/1/webkit/plugins/ppapi/ppapi_plugin_instance.cc
*/
bool PdfMetafileSkia::RenderPage(unsigned int page_number,
                                 CGContextRef context,
                                 const CGRect rect,
                                 const MacRenderPageParams& params) const {
  DCHECK_GT(GetDataSize(), 0U);
  if (data_->pdf_cg_.GetDataSize() == 0) {
    if (GetDataSize() == 0)
      return false;
    size_t length = data_->pdf_data_->getLength();
    std::vector<uint8_t> buffer(length);
    (void)WriteAssetToBuffer(data_->pdf_data_.get(), &buffer[0], length);
    data_->pdf_cg_.InitFromData(&buffer[0],
                                base::checked_cast<uint32_t>(length));
  }
  return data_->pdf_cg_.RenderPage(page_number, context, rect, params);
}
#endif

bool PdfMetafileSkia::SaveTo(base::File* file) const {
  if (GetDataSize() == 0U)
    return false;

  // Calling duplicate() keeps original asset state unchanged.
  std::unique_ptr<SkStreamAsset> asset(data_->pdf_data_->duplicate());

  const size_t kMaximumBufferSize = 1024 * 1024;
  std::vector<char> buffer(std::min(kMaximumBufferSize, asset->getLength()));
  do {
    size_t read_size = asset->read(&buffer[0], buffer.size());
    if (read_size == 0)
      break;
    DCHECK_GE(buffer.size(), read_size);
    if (!file->WriteAtCurrentPos(&buffer[0],
                                 base::checked_cast<int>(read_size))) {
      return false;
    }
  } while (!asset->isAtEnd());

  return true;
}

PdfMetafileSkia::PdfMetafileSkia(SkiaDocumentType type)
    : data_(new PdfMetafileSkiaData) {
  data_->type_ = type;
}

std::unique_ptr<PdfMetafileSkia> PdfMetafileSkia::GetMetafileForCurrentPage(
    SkiaDocumentType type) {
  // If we only ever need the metafile for the last page, should we
  // only keep a handle on one SkPicture?
  std::unique_ptr<PdfMetafileSkia> metafile(new PdfMetafileSkia(type));

  if (data_->pages_.size() == 0)
    return metafile;

  if (data_->recorder_.getRecordingCanvas())  // page outstanding
    return metafile;

  metafile->data_->pages_.push_back(data_->pages_.back());

  if (!metafile->FinishDocument())  // Generate PDF.
    metafile.reset();

  return metafile;
}

}  // namespace printing
