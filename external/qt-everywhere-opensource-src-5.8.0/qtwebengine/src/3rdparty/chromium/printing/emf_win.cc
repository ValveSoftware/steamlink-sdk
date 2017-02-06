// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/emf_win.h"

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

int CALLBACK IsAlphaBlendUsedEnumProc(HDC,
                                      HANDLETABLE*,
                                      const ENHMETARECORD *record,
                                      int,
                                      LPARAM data) {
  bool* result = reinterpret_cast<bool*>(data);
  if (!result)
    return 0;
  switch (record->iType) {
    case EMR_ALPHABLEND: {
      *result = true;
      return 0;
      break;
    }
  }
  return 1;
}

int CALLBACK RasterizeAlphaBlendProc(HDC metafile_dc,
                                     HANDLETABLE* handle_table,
                                     const ENHMETARECORD *record,
                                     int num_objects,
                                     LPARAM data) {
    HDC bitmap_dc = *reinterpret_cast<HDC*>(data);
    // Play this command to the bitmap DC.
    ::PlayEnhMetaFileRecord(bitmap_dc, handle_table, record, num_objects);
    switch (record->iType) {
    case EMR_ALPHABLEND: {
      const EMRALPHABLEND* alpha_blend =
          reinterpret_cast<const EMRALPHABLEND*>(record);
      // Don't modify transformation here.
      // Old implementation did reset transformations for DC to identity matrix.
      // That was not correct and cause some bugs, like unexpected cropping.
      // EMRALPHABLEND is rendered into bitmap and metafile contexts with
      // current transformation. If we don't touch them here BitBlt will copy
      // same areas.
      ::BitBlt(metafile_dc,
               alpha_blend->xDest,
               alpha_blend->yDest,
               alpha_blend->cxDest,
               alpha_blend->cyDest,
               bitmap_dc,
               alpha_blend->xDest,
               alpha_blend->yDest,
               SRCCOPY);
      break;
    }
    case EMR_CREATEBRUSHINDIRECT:
    case EMR_CREATECOLORSPACE:
    case EMR_CREATECOLORSPACEW:
    case EMR_CREATEDIBPATTERNBRUSHPT:
    case EMR_CREATEMONOBRUSH:
    case EMR_CREATEPALETTE:
    case EMR_CREATEPEN:
    case EMR_DELETECOLORSPACE:
    case EMR_DELETEOBJECT:
    case EMR_EXTCREATEFONTINDIRECTW:
      // Play object creation command only once.
      break;

    default:
      // Play this command to the metafile DC.
      ::PlayEnhMetaFileRecord(metafile_dc, handle_table, record, num_objects);
      break;
    }
    return 1;  // Continue enumeration
}

// Bitmapt for rasterization.
class RasterBitmap {
 public:
  explicit RasterBitmap(const gfx::Size& raster_size)
      : saved_object_(NULL) {
    context_.Set(::CreateCompatibleDC(NULL));
    if (!context_.IsValid()) {
      NOTREACHED() << "Bitmap DC creation failed";
      return;
    }
    ::SetGraphicsMode(context_.Get(), GM_ADVANCED);
    void* bits = NULL;
    gfx::Rect bitmap_rect(raster_size);
    gfx::CreateBitmapHeader(raster_size.width(), raster_size.height(),
                            &header_.bmiHeader);
    bitmap_.reset(CreateDIBSection(context_.Get(), &header_, DIB_RGB_COLORS,
                                   &bits, NULL, 0));
    if (!bitmap_.is_valid())
      NOTREACHED() << "Raster bitmap creation for printing failed";

    saved_object_ = ::SelectObject(context_.Get(), bitmap_.get());
    RECT rect = bitmap_rect.ToRECT();
    ::FillRect(context_.Get(), &rect,
               static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));
  }

  ~RasterBitmap() {
    ::SelectObject(context_.Get(), saved_object_);
  }

  HDC context() const {
    return context_.Get();
  }

  base::win::ScopedCreateDC context_;
  BITMAPINFO header_;
  base::win::ScopedBitmap bitmap_;
  HGDIOBJ saved_object_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RasterBitmap);
};



}  // namespace

namespace printing {

bool DIBFormatNativelySupported(HDC dc, uint32_t escape, const BYTE* bits,
                                int size) {
  BOOL supported = FALSE;
  if (ExtEscape(dc, QUERYESCSUPPORT, sizeof(escape),
                reinterpret_cast<LPCSTR>(&escape), 0, 0) > 0) {
    ExtEscape(dc, escape, size, reinterpret_cast<LPCSTR>(bits),
              sizeof(supported), reinterpret_cast<LPSTR>(&supported));
  }
  return !!supported;
}

Emf::Emf() : emf_(NULL), hdc_(NULL) {
}

Emf::~Emf() {
  Close();
}

void Emf::Close() {
  DCHECK(!hdc_);
  if (emf_)
    DeleteEnhMetaFile(emf_);
  emf_ = NULL;
}

bool Emf::InitToFile(const base::FilePath& metafile_path) {
  DCHECK(!emf_ && !hdc_);
  hdc_ = CreateEnhMetaFile(NULL, metafile_path.value().c_str(), NULL, NULL);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

bool Emf::InitFromFile(const base::FilePath& metafile_path) {
  DCHECK(!emf_ && !hdc_);
  emf_ = GetEnhMetaFile(metafile_path.value().c_str());
  DCHECK(emf_);
  return emf_ != NULL;
}

bool Emf::Init() {
  DCHECK(!emf_ && !hdc_);
  hdc_ = CreateEnhMetaFile(NULL, NULL, NULL, NULL);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

bool Emf::InitFromData(const void* src_buffer, uint32_t src_buffer_size) {
  DCHECK(!emf_ && !hdc_);
  emf_ = SetEnhMetaFileBits(src_buffer_size,
                            reinterpret_cast<const BYTE*>(src_buffer));
  return emf_ != NULL;
}

bool Emf::FinishDocument() {
  DCHECK(!emf_ && hdc_);
  emf_ = CloseEnhMetaFile(hdc_);
  DCHECK(emf_);
  hdc_ = NULL;
  return emf_ != NULL;
}

bool Emf::Playback(HDC hdc, const RECT* rect) const {
  DCHECK(emf_ && !hdc_);
  RECT bounds;
  if (!rect) {
    // Get the natural bounds of the EMF buffer.
    bounds = GetPageBounds(1).ToRECT();
    rect = &bounds;
  }
  return PlayEnhMetaFile(hdc, emf_, rect) != 0;
}

bool Emf::SafePlayback(HDC context) const {
  DCHECK(emf_ && !hdc_);
  XFORM base_matrix;
  if (!GetWorldTransform(context, &base_matrix)) {
    NOTREACHED();
    return false;
  }
  Emf::EnumerationContext playback_context;
  playback_context.base_matrix = &base_matrix;
  gfx::Rect bound = GetPageBounds(1);
  RECT rect = bound.ToRECT();
  return bound.IsEmpty() ||
         EnumEnhMetaFile(context,
                         emf_,
                         &Emf::SafePlaybackProc,
                         reinterpret_cast<void*>(&playback_context),
                         &rect) != 0;
}

gfx::Rect Emf::GetPageBounds(unsigned int page_number) const {
  DCHECK(emf_ && !hdc_);
  DCHECK_EQ(1U, page_number);
  ENHMETAHEADER header;
  if (GetEnhMetaFileHeader(emf_, sizeof(header), &header) != sizeof(header)) {
    NOTREACHED();
    return gfx::Rect();
  }
  // Add 1 to right and bottom because it's inclusive rectangle.
  // See ENHMETAHEADER.
  return gfx::Rect(header.rclBounds.left,
                   header.rclBounds.top,
                   header.rclBounds.right - header.rclBounds.left + 1,
                   header.rclBounds.bottom - header.rclBounds.top + 1);
}

unsigned int Emf::GetPageCount() const {
  return 1;
}

HDC Emf::context() const {
  return hdc_;
}

uint32_t Emf::GetDataSize() const {
  DCHECK(emf_ && !hdc_);
  return GetEnhMetaFileBits(emf_, 0, NULL);
}

bool Emf::GetData(void* buffer, uint32_t size) const {
  DCHECK(emf_ && !hdc_);
  DCHECK(buffer && size);
  uint32_t size2 =
      GetEnhMetaFileBits(emf_, size, reinterpret_cast<BYTE*>(buffer));
  DCHECK(size2 == size);
  return size2 == size && size2 != 0;
}

int CALLBACK Emf::SafePlaybackProc(HDC hdc,
                                   HANDLETABLE* handle_table,
                                   const ENHMETARECORD* record,
                                   int objects_count,
                                   LPARAM param) {
  Emf::EnumerationContext* context =
      reinterpret_cast<Emf::EnumerationContext*>(param);
  context->handle_table = handle_table;
  context->objects_count = objects_count;
  context->hdc = hdc;
  Record record_instance(record);
  bool success = record_instance.SafePlayback(context);
  DCHECK(success);
  return 1;
}

Emf::EnumerationContext::EnumerationContext() {
  memset(this, 0, sizeof(*this));
}

Emf::Record::Record(const ENHMETARECORD* record)
    : record_(record) {
  DCHECK(record_);
}

bool Emf::Record::Play(Emf::EnumerationContext* context) const {
  return 0 != PlayEnhMetaFileRecord(context->hdc,
                                    context->handle_table,
                                    record_,
                                    context->objects_count);
}

bool Emf::Record::SafePlayback(Emf::EnumerationContext* context) const {
  // For EMF field description, see [MS-EMF] Enhanced Metafile Format
  // Specification.
  //
  // This is the second major EMF breakage I get; the first one being
  // SetDCBrushColor/SetDCPenColor/DC_PEN/DC_BRUSH being silently ignored.
  //
  // This function is the guts of the fix for bug 1186598. Some printer drivers
  // somehow choke on certain EMF records, but calling the corresponding
  // function directly on the printer HDC is fine. Still, playing the EMF record
  // fails. Go figure.
  //
  // The main issue is that SetLayout is totally unsupported on these printers
  // (HP 4500/4700). I used to call SetLayout and I stopped. I found out this is
  // not sufficient because GDI32!PlayEnhMetaFile internally calls SetLayout(!)
  // Damn.
  //
  // So I resorted to manually parse the EMF records and play them one by one.
  // The issue with this method compared to using PlayEnhMetaFile to play back
  // an EMF buffer is that the later silently fixes the matrix to take in
  // account the matrix currently loaded at the time of the call.
  // The matrix magic is done transparently when using PlayEnhMetaFile but since
  // I'm processing one field at a time, I need to do the fixup myself. Note
  // that PlayEnhMetaFileRecord doesn't fix the matrix correctly even when
  // called inside an EnumEnhMetaFile loop. Go figure (bis).
  //
  // So when I see a EMR_SETWORLDTRANSFORM and EMR_MODIFYWORLDTRANSFORM, I need
  // to fix the matrix according to the matrix previously loaded before playing
  // back the buffer. Otherwise, the previously loaded matrix would be ignored
  // and the EMF buffer would always be played back at its native resolution.
  // Duh.
  //
  // I also use this opportunity to skip over eventual EMR_SETLAYOUT record that
  // could remain.
  //
  // Another tweak we make is for JPEGs/PNGs in calls to StretchDIBits.
  // (Our Pepper plugin code uses a JPEG). If the printer does not support
  // JPEGs/PNGs natively we decompress the JPEG/PNG and then set it to the
  // device.
  // TODO(sanjeevr): We should also add JPEG/PNG support for SetSIBitsToDevice
  //
  // We also process any custom EMR_GDICOMMENT records which are our
  // placeholders for StartPage and EndPage.
  // Note: I should probably care about view ports and clipping, eventually.
  bool res = false;
  const XFORM* base_matrix = context->base_matrix;
  switch (record()->iType) {
    case EMR_STRETCHDIBITS: {
      const EMRSTRETCHDIBITS * sdib_record =
          reinterpret_cast<const EMRSTRETCHDIBITS*>(record());
      const BYTE* record_start = reinterpret_cast<const BYTE *>(record());
      const BITMAPINFOHEADER *bmih =
          reinterpret_cast<const BITMAPINFOHEADER *>(record_start +
                                                     sdib_record->offBmiSrc);
      const BYTE* bits = record_start + sdib_record->offBitsSrc;
      bool play_normally = true;
      res = false;
      HDC hdc = context->hdc;
      std::unique_ptr<SkBitmap> bitmap;
      if (bmih->biCompression == BI_JPEG) {
        if (!DIBFormatNativelySupported(hdc, CHECKJPEGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          bitmap = gfx::JPEGCodec::Decode(bits, bmih->biSizeImage);
        }
      } else if (bmih->biCompression == BI_PNG) {
        if (!DIBFormatNativelySupported(hdc, CHECKPNGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          bitmap.reset(new SkBitmap());
          gfx::PNGCodec::Decode(bits, bmih->biSizeImage, bitmap.get());
        }
      }
      if (!play_normally) {
        DCHECK(bitmap.get());
        if (bitmap.get()) {
          SkAutoLockPixels lock(*bitmap.get());
          DCHECK_EQ(bitmap->colorType(), kN32_SkColorType);
          const uint32_t* pixels =
              static_cast<const uint32_t*>(bitmap->getPixels());
          if (pixels == NULL) {
            NOTREACHED();
            return false;
          }
          BITMAPINFOHEADER bmi = {0};
          gfx::CreateBitmapHeader(bitmap->width(), bitmap->height(), &bmi);
          res = (0 != StretchDIBits(hdc, sdib_record->xDest, sdib_record->yDest,
                                    sdib_record->cxDest,
                                    sdib_record->cyDest, sdib_record->xSrc,
                                    sdib_record->ySrc,
                                    sdib_record->cxSrc, sdib_record->cySrc,
                                    pixels,
                                    reinterpret_cast<const BITMAPINFO *>(&bmi),
                                    sdib_record->iUsageSrc,
                                    sdib_record->dwRop));
        }
      } else {
        res = Play(context);
      }
      break;
    }
    case EMR_SETWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize, sizeof(DWORD) * 2 + sizeof(XFORM));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      HDC hdc = context->hdc;
      if (base_matrix) {
        res = 0 != SetWorldTransform(hdc, base_matrix) &&
                   ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
      } else {
        res = 0 != SetWorldTransform(hdc, xform);
      }
      break;
    }
    case EMR_MODIFYWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize,
                sizeof(DWORD) * 2 + sizeof(XFORM) + sizeof(DWORD));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      const DWORD* option = reinterpret_cast<const DWORD*>(xform + 1);
      HDC hdc = context->hdc;
      switch (*option) {
        case MWT_IDENTITY:
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix);
          } else {
            res = 0 != ModifyWorldTransform(hdc, xform, MWT_IDENTITY);
          }
          break;
        case MWT_LEFTMULTIPLY:
        case MWT_RIGHTMULTIPLY:
          res = 0 != ModifyWorldTransform(hdc, xform, *option);
          break;
        case 4:  // MWT_SET
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix) &&
                       ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
          } else {
            res = 0 != SetWorldTransform(hdc, xform);
          }
          break;
        default:
          res = false;
          break;
      }
      break;
    }
    case EMR_SETLAYOUT:
      // Ignore it.
      res = true;
      break;
    default: {
      res = Play(context);
      break;
    }
  }
  return res;
}

void Emf::StartPage(const gfx::Size& /*page_size*/,
                    const gfx::Rect& /*content_area*/,
                    const float& /*scale_factor*/) {
}

bool Emf::FinishPage() {
  return true;
}

Emf::Enumerator::Enumerator(const Emf& emf, HDC context, const RECT* rect) {
  items_.clear();
  if (!EnumEnhMetaFile(context,
                       emf.emf(),
                       &Emf::Enumerator::EnhMetaFileProc,
                       reinterpret_cast<void*>(this),
                       rect)) {
    NOTREACHED();
    items_.clear();
  }
  DCHECK_EQ(context_.hdc, context);
}

Emf::Enumerator::~Enumerator() {
}

Emf::Enumerator::const_iterator Emf::Enumerator::begin() const {
  return items_.begin();
}

Emf::Enumerator::const_iterator Emf::Enumerator::end() const {
  return items_.end();
}

int CALLBACK Emf::Enumerator::EnhMetaFileProc(HDC hdc,
                                              HANDLETABLE* handle_table,
                                              const ENHMETARECORD* record,
                                              int objects_count,
                                              LPARAM param) {
  Enumerator& emf = *reinterpret_cast<Enumerator*>(param);
  if (!emf.context_.handle_table) {
    DCHECK(!emf.context_.handle_table);
    DCHECK(!emf.context_.objects_count);
    emf.context_.handle_table = handle_table;
    emf.context_.objects_count = objects_count;
    emf.context_.hdc = hdc;
  } else {
    DCHECK_EQ(emf.context_.handle_table, handle_table);
    DCHECK_EQ(emf.context_.objects_count, objects_count);
    DCHECK_EQ(emf.context_.hdc, hdc);
  }
  emf.items_.push_back(Record(record));
  return 1;
}

bool Emf::IsAlphaBlendUsed() const {
  bool result = false;
  ::EnumEnhMetaFile(NULL,
                    emf(),
                    &IsAlphaBlendUsedEnumProc,
                    &result,
                    NULL);
  return result;
}

std::unique_ptr<Emf> Emf::RasterizeMetafile(int raster_area_in_pixels) const {
  gfx::Rect page_bounds = GetPageBounds(1);
  gfx::Size page_size(page_bounds.size());
  if (page_size.GetArea() <= 0) {
    NOTREACHED() << "Metafile is empty";
    page_bounds = gfx::Rect(1, 1);
  }

  float scale = sqrt(
      static_cast<float>(raster_area_in_pixels) / page_size.GetArea());
  page_size.set_width(std::max<int>(1, page_size.width() * scale));
  page_size.set_height(std::max<int>(1, page_size.height() * scale));


  RasterBitmap bitmap(page_size);

  gfx::Rect bitmap_rect(page_size);
  RECT rect = bitmap_rect.ToRECT();
  Playback(bitmap.context(), &rect);

  std::unique_ptr<Emf> result(new Emf);
  result->Init();
  HDC hdc = result->context();
  DCHECK(hdc);
  skia::InitializeDC(hdc);

  // Params are ignored.
  result->StartPage(page_bounds.size(), page_bounds, 1);

  ::ModifyWorldTransform(hdc, NULL, MWT_IDENTITY);
  XFORM xform = {
    static_cast<float>(page_bounds.width()) / bitmap_rect.width(),
    0,
    0,
    static_cast<float>(page_bounds.height()) / bitmap_rect.height(),
    static_cast<float>(page_bounds.x()),
    static_cast<float>(page_bounds.y()),
  };
  ::SetWorldTransform(hdc, &xform);
  ::BitBlt(hdc, 0, 0, bitmap_rect.width(), bitmap_rect.height(),
           bitmap.context(), bitmap_rect.x(), bitmap_rect.y(), SRCCOPY);

  result->FinishPage();
  result->FinishDocument();

  return result;
}

std::unique_ptr<Emf> Emf::RasterizeAlphaBlend() const {
  gfx::Rect page_bounds = GetPageBounds(1);
  if (page_bounds.size().GetArea() <= 0) {
    NOTREACHED() << "Metafile is empty";
    page_bounds = gfx::Rect(1, 1);
  }

  RasterBitmap bitmap(page_bounds.size());

  // Map metafile page_bounds.x(), page_bounds.y() to bitmap 0, 0.
  XFORM xform = {1,
                 0,
                 0,
                 1,
                 static_cast<float>(-page_bounds.x()),
                 static_cast<float>(-page_bounds.y())};
  ::SetWorldTransform(bitmap.context(), &xform);

  std::unique_ptr<Emf> result(new Emf);
  result->Init();
  HDC hdc = result->context();
  DCHECK(hdc);
  skia::InitializeDC(hdc);

  HDC bitmap_dc = bitmap.context();
  RECT rect = page_bounds.ToRECT();
  ::EnumEnhMetaFile(hdc, emf(), &RasterizeAlphaBlendProc, &bitmap_dc, &rect);

  result->FinishDocument();

  return result;
}


}  // namespace printing
