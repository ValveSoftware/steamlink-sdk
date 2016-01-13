// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "third_party/WebKit/public/platform/WebImage.h"

using blink::WebCursorInfo;

static const int kMaxCursorDimension = 1024;

namespace content {

WebCursor::WebCursor()
    : type_(WebCursorInfo::TypePointer),
      custom_scale_(1) {
#if defined(OS_WIN)
  external_cursor_ = NULL;
#endif
  InitPlatformData();
}

WebCursor::WebCursor(const CursorInfo& cursor_info)
    : type_(WebCursorInfo::TypePointer) {
#if defined(OS_WIN)
  external_cursor_ = NULL;
#endif
  InitPlatformData();
  InitFromCursorInfo(cursor_info);
}

WebCursor::~WebCursor() {
  Clear();
}

WebCursor::WebCursor(const WebCursor& other) {
  InitPlatformData();
  Copy(other);
}

const WebCursor& WebCursor::operator=(const WebCursor& other) {
  if (this == &other)
    return *this;

  Clear();
  Copy(other);
  return *this;
}

void WebCursor::InitFromCursorInfo(const CursorInfo& cursor_info) {
  Clear();

#if defined(OS_WIN)
  if (cursor_info.external_handle) {
    InitFromExternalCursor(cursor_info.external_handle);
    return;
  }
#endif

  type_ = cursor_info.type;
  hotspot_ = cursor_info.hotspot;
  if (IsCustom())
    SetCustomData(cursor_info.custom_image);
  custom_scale_ = cursor_info.image_scale_factor;
  CHECK(custom_scale_ > 0);
  ClampHotspot();
}

void WebCursor::GetCursorInfo(CursorInfo* cursor_info) const {
  cursor_info->type = static_cast<WebCursorInfo::Type>(type_);
  cursor_info->hotspot = hotspot_;
  ImageFromCustomData(&cursor_info->custom_image);
  cursor_info->image_scale_factor = custom_scale_;

#if defined(OS_WIN)
  cursor_info->external_handle = external_cursor_;
#endif
}

bool WebCursor::Deserialize(PickleIterator* iter) {
  int type, hotspot_x, hotspot_y, size_x, size_y, data_len;
  float scale;
  const char* data;

  // Leave |this| unmodified unless we are going to return success.
  if (!iter->ReadInt(&type) ||
      !iter->ReadInt(&hotspot_x) ||
      !iter->ReadInt(&hotspot_y) ||
      !iter->ReadLength(&size_x) ||
      !iter->ReadLength(&size_y) ||
      !iter->ReadFloat(&scale) ||
      !iter->ReadData(&data, &data_len))
    return false;

  // Ensure the size is sane, and there is enough data.
  if (size_x > kMaxCursorDimension ||
      size_y > kMaxCursorDimension)
    return false;

  // Ensure scale isn't ridiculous, and the scaled image size is still sane.
  if (scale < 0.01 || scale > 100 ||
      size_x / scale > kMaxCursorDimension ||
      size_y / scale > kMaxCursorDimension)
    return false;

  type_ = type;

  if (type == WebCursorInfo::TypeCustom) {
    if (size_x > 0 && size_y > 0) {
      // The * 4 is because the expected format is an array of RGBA pixel
      // values.
      if (size_x * size_y * 4 > data_len)
        return false;

      hotspot_.set_x(hotspot_x);
      hotspot_.set_y(hotspot_y);
      custom_size_.set_width(size_x);
      custom_size_.set_height(size_y);
      custom_scale_ = scale;
      ClampHotspot();

      custom_data_.clear();
      if (data_len > 0) {
        custom_data_.resize(data_len);
        memcpy(&custom_data_[0], data, data_len);
      }
    }
  }
  return DeserializePlatformData(iter);
}

bool WebCursor::Serialize(Pickle* pickle) const {
  if (!pickle->WriteInt(type_) ||
      !pickle->WriteInt(hotspot_.x()) ||
      !pickle->WriteInt(hotspot_.y()) ||
      !pickle->WriteInt(custom_size_.width()) ||
      !pickle->WriteInt(custom_size_.height()) ||
      !pickle->WriteFloat(custom_scale_))
    return false;

  const char* data = NULL;
  if (!custom_data_.empty())
    data = &custom_data_[0];
  if (!pickle->WriteData(data, custom_data_.size()))
    return false;

  return SerializePlatformData(pickle);
}

bool WebCursor::IsCustom() const {
  return type_ == WebCursorInfo::TypeCustom;
}

bool WebCursor::IsEqual(const WebCursor& other) const {
  if (type_ != other.type_)
    return false;

  if (!IsPlatformDataEqual(other))
    return false;

  return hotspot_ == other.hotspot_ &&
         custom_size_ == other.custom_size_ &&
         custom_scale_ == other.custom_scale_ &&
         custom_data_ == other.custom_data_;
}

#if defined(OS_WIN)

static WebCursorInfo::Type ToCursorType(HCURSOR cursor) {
  static struct {
    HCURSOR cursor;
    WebCursorInfo::Type type;
  } kStandardCursors[] = {
    { LoadCursor(NULL, IDC_ARROW),       WebCursorInfo::TypePointer },
    { LoadCursor(NULL, IDC_CROSS),       WebCursorInfo::TypeCross },
    { LoadCursor(NULL, IDC_HAND),        WebCursorInfo::TypeHand },
    { LoadCursor(NULL, IDC_IBEAM),       WebCursorInfo::TypeIBeam },
    { LoadCursor(NULL, IDC_WAIT),        WebCursorInfo::TypeWait },
    { LoadCursor(NULL, IDC_HELP),        WebCursorInfo::TypeHelp },
    { LoadCursor(NULL, IDC_SIZENESW),    WebCursorInfo::TypeNorthEastResize },
    { LoadCursor(NULL, IDC_SIZENWSE),    WebCursorInfo::TypeNorthWestResize },
    { LoadCursor(NULL, IDC_SIZENS),      WebCursorInfo::TypeNorthSouthResize },
    { LoadCursor(NULL, IDC_SIZEWE),      WebCursorInfo::TypeEastWestResize },
    { LoadCursor(NULL, IDC_SIZEALL),     WebCursorInfo::TypeMove },
    { LoadCursor(NULL, IDC_APPSTARTING), WebCursorInfo::TypeProgress },
    { LoadCursor(NULL, IDC_NO),          WebCursorInfo::TypeNotAllowed },
  };
  for (int i = 0; i < arraysize(kStandardCursors); ++i) {
    if (cursor == kStandardCursors[i].cursor)
      return kStandardCursors[i].type;
  }
  return WebCursorInfo::TypeCustom;
}

void WebCursor::InitFromExternalCursor(HCURSOR cursor) {
  WebCursorInfo::Type cursor_type = ToCursorType(cursor);

  InitFromCursorInfo(CursorInfo(cursor_type));

  if (cursor_type == WebCursorInfo::TypeCustom)
    external_cursor_ = cursor;
}

#endif  // defined(OS_WIN)

void WebCursor::Clear() {
  type_ = WebCursorInfo::TypePointer;
  hotspot_.set_x(0);
  hotspot_.set_y(0);
  custom_size_.set_width(0);
  custom_size_.set_height(0);
  custom_scale_ = 1;
  custom_data_.clear();
  CleanupPlatformData();
}

void WebCursor::Copy(const WebCursor& other) {
  type_ = other.type_;
  hotspot_ = other.hotspot_;
  custom_size_ = other.custom_size_;
  custom_scale_ = other.custom_scale_;
  custom_data_ = other.custom_data_;
  CopyPlatformData(other);
}

void WebCursor::SetCustomData(const SkBitmap& bitmap) {
  if (bitmap.empty())
    return;

  // Fill custom_data_ directly with the NativeImage pixels.
  SkAutoLockPixels bitmap_lock(bitmap);
  custom_data_.resize(bitmap.getSize());
  if (!custom_data_.empty())
    memcpy(&custom_data_[0], bitmap.getPixels(), bitmap.getSize());
  custom_size_.set_width(bitmap.width());
  custom_size_.set_height(bitmap.height());
}

void WebCursor::ImageFromCustomData(SkBitmap* image) const {
  if (custom_data_.empty())
    return;

  image->setConfig(SkBitmap::kARGB_8888_Config,
                   custom_size_.width(),
                   custom_size_.height());
  if (!image->allocPixels())
    return;
  memcpy(image->getPixels(), &custom_data_[0], custom_data_.size());
}

void WebCursor::ClampHotspot() {
  if (!IsCustom())
    return;

  // Clamp the hotspot to the custom image's dimensions.
  hotspot_.set_x(std::max(0,
                          std::min(custom_size_.width() - 1, hotspot_.x())));
  hotspot_.set_y(std::max(0,
                          std::min(custom_size_.height() - 1, hotspot_.y())));
}

}  // namespace content
