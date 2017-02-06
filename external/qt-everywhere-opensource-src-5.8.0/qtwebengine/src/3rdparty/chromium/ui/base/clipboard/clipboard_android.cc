// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "jni/Clipboard_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

// TODO:(andrewhayden) Support additional formats in Android: Bitmap, URI, HTML,
// HTML+text now that Android's clipboard system supports them, then nuke the
// legacy implementation note below.

// Legacy implementation note:
// The Android clipboard system used to only support text format. So we used the
// Android system when some text was added or retrieved from the system. For
// anything else, we STILL store the value in some process wide static
// variable protected by a lock. So the (non-text) clipboard will only work
// within the same process.

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace ui {

namespace {
// Various formats we support.
const char kURLFormat[] = "url";
const char kPlainTextFormat[] = "text";
const char kHTMLFormat[] = "html";
const char kRTFFormat[] = "rtf";
const char kBitmapFormat[] = "bitmap";
const char kWebKitSmartPasteFormat[] = "webkit_smart";
const char kBookmarkFormat[] = "bookmark";

class ClipboardMap {
 public:
  ClipboardMap();
  std::string Get(const std::string& format);
  bool HasFormat(const std::string& format);
  void Set(const std::string& format, const std::string& data);
  void CommitToAndroidClipboard();
  void Clear();

 private:
  void UpdateFromAndroidClipboard();
  std::map<std::string, std::string> map_;
  base::Lock lock_;

  // Java class and methods for the Android ClipboardManager.
  ScopedJavaGlobalRef<jobject> clipboard_manager_;
};
base::LazyInstance<ClipboardMap>::Leaky g_map = LAZY_INSTANCE_INITIALIZER;

ClipboardMap::ClipboardMap() {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Get the context.
  jobject context = base::android::GetApplicationContext();
  DCHECK(context);

  clipboard_manager_.Reset(Java_Clipboard_create(env, context));
  DCHECK(clipboard_manager_.obj());
}

std::string ClipboardMap::Get(const std::string& format) {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  std::map<std::string, std::string>::const_iterator it = map_.find(format);
  return it == map_.end() ? std::string() : it->second;
}

bool ClipboardMap::HasFormat(const std::string& format) {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  return ContainsKey(map_, format);
}

void ClipboardMap::Set(const std::string& format, const std::string& data) {
  base::AutoLock lock(lock_);
  map_[format] = data;
}

void ClipboardMap::CommitToAndroidClipboard() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  if (ContainsKey(map_, kHTMLFormat)) {
    // Android's API for storing HTML content on the clipboard requires a plain-
    // text representation to be available as well.
    if (!ContainsKey(map_, kPlainTextFormat))
      return;

    ScopedJavaLocalRef<jstring> html =
        ConvertUTF8ToJavaString(env, map_[kHTMLFormat].c_str());
    ScopedJavaLocalRef<jstring> text =
        ConvertUTF8ToJavaString(env, map_[kPlainTextFormat].c_str());

    DCHECK(html.obj() && text.obj());
    Java_Clipboard_setHTMLText(env, clipboard_manager_.obj(), html.obj(),
                               text.obj());
  } else if (ContainsKey(map_, kPlainTextFormat)) {
    ScopedJavaLocalRef<jstring> str =
        ConvertUTF8ToJavaString(env, map_[kPlainTextFormat].c_str());
    DCHECK(str.obj());
    Java_Clipboard_setText(env, clipboard_manager_.obj(), str.obj());
  } else {
    Java_Clipboard_clear(env, clipboard_manager_.obj());
    NOTIMPLEMENTED();
  }
}

void ClipboardMap::Clear() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  map_.clear();
  Java_Clipboard_clear(env, clipboard_manager_.obj());
}

// Add a key:jstr pair to map, but only if jstr is not null, and also
// not empty.
void AddMapEntry(JNIEnv* env,
                 std::map<std::string, std::string>* map,
                 const char* key,
                 const ScopedJavaLocalRef<jstring>& jstr) {
  if (!jstr.is_null()) {
    std::string str = ConvertJavaStringToUTF8(env, jstr.obj());
    if (!str.empty())
      (*map)[key] = str;
  }
}

// Return true if all the key-value pairs in map1 are also in map2.
bool MapIsSubset(const std::map<std::string, std::string>& map1,
                 const std::map<std::string, std::string>& map2) {
  for (const auto& val : map1) {
    auto iter = map2.find(val.first);
    if (iter == map2.end() || iter->second != val.second)
      return false;
  }
  return true;
}

void ClipboardMap::UpdateFromAndroidClipboard() {
  // Fetch the current Android clipboard state. Replace our state with
  // the Android state if the Android state has been changed.
  lock_.AssertAcquired();
  JNIEnv* env = AttachCurrentThread();

  std::map<std::string, std::string> android_clipboard_state;

  ScopedJavaLocalRef<jstring> jtext =
      Java_Clipboard_getCoercedText(env, clipboard_manager_.obj());
  ScopedJavaLocalRef<jstring> jhtml =
      Java_Clipboard_getHTMLText(env, clipboard_manager_.obj());

  AddMapEntry(env, &android_clipboard_state, kPlainTextFormat, jtext);
  AddMapEntry(env, &android_clipboard_state, kHTMLFormat, jhtml);

  if (!MapIsSubset(android_clipboard_state, map_))
    android_clipboard_state.swap(map_);
}

}  // namespace

// Clipboard::FormatType implementation.
Clipboard::FormatType::FormatType() {
}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {
}

Clipboard::FormatType::~FormatType() {
}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::operator<(const FormatType& other) const {
  return data_ < other.data_;
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

// Various predefined FormatTypes.
// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kURLFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kPlainTextFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kPlainTextFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kWebKitSmartPasteFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kHTMLFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kRTFFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kBitmapFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
  return type;
}

// Clipboard factory method.
// static
Clipboard* Clipboard::Create() {
  return new ClipboardAndroid;
}

// ClipboardAndroid implementation.
ClipboardAndroid::ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
}

ClipboardAndroid::~ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
}

uint64_t ClipboardAndroid::GetSequenceNumber(ClipboardType /* type */) const {
  DCHECK(CalledOnValidThread());
  // TODO: implement this. For now this interface will advertise
  // that the clipboard never changes. That's fine as long as we
  // don't rely on this signal.
  return 0;
}

bool ClipboardAndroid::IsFormatAvailable(const Clipboard::FormatType& format,
                                         ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  return g_map.Get().HasFormat(format.ToString());
}

void ClipboardAndroid::Clear(ClipboardType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  g_map.Get().Clear();
}

void ClipboardAndroid::ReadAvailableTypes(ClipboardType type,
                                          std::vector<base::string16>* types,
                                          bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);

  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  types->clear();

  // would be nice to ask the ClipboardMap to enumerate the types it supports,
  // rather than hardcode the list here.
  if (IsFormatAvailable(Clipboard::GetPlainTextFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(Clipboard::GetHtmlFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));

  // these formats aren't supported by the ClipboardMap currently, but might
  // be one day?
  if (IsFormatAvailable(Clipboard::GetRtfFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (IsFormatAvailable(Clipboard::GetBitmapFormatType(), type))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  *contains_filenames = false;
}

void ClipboardAndroid::ReadText(ClipboardType type,
                                base::string16* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  std::string utf8;
  ReadAsciiText(type, &utf8);
  *result = base::UTF8ToUTF16(utf8);
}

void ClipboardAndroid::ReadAsciiText(ClipboardType type,
                                     std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  *result = g_map.Get().Get(kPlainTextFormat);
}

// Note: |src_url| isn't really used. It is only implemented in Windows
void ClipboardAndroid::ReadHTML(ClipboardType type,
                                base::string16* markup,
                                std::string* src_url,
                                uint32_t* fragment_start,
                                uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  if (src_url)
    src_url->clear();

  std::string input = g_map.Get().Get(kHTMLFormat);
  *markup = base::UTF8ToUTF16(input);

  *fragment_start = 0;
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardAndroid::ReadRTF(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

SkBitmap ClipboardAndroid::ReadImage(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  std::string input = g_map.Get().Get(kBitmapFormat);

  SkBitmap bmp;
  if (!input.empty()) {
    DCHECK_LE(sizeof(gfx::Size), input.size());
    const gfx::Size* size = reinterpret_cast<const gfx::Size*>(input.data());

    bmp.allocN32Pixels(size->width(), size->height());

    DCHECK_EQ(sizeof(gfx::Size) + bmp.getSize(), input.size());

    memcpy(bmp.getPixels(), input.data() + sizeof(gfx::Size), bmp.getSize());
  }
  return bmp;
}

void ClipboardAndroid::ReadCustomData(ClipboardType clipboard_type,
                                      const base::string16& type,
                                      base::string16* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardAndroid::ReadBookmark(base::string16* title,
                                    std::string* url) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ClipboardAndroid::ReadData(const Clipboard::FormatType& format,
                                std::string* result) const {
  DCHECK(CalledOnValidThread());
  *result = g_map.Get().Get(format.ToString());
}

// Main entry point used to write several values in the clipboard.
void ClipboardAndroid::WriteObjects(ClipboardType type,
                                    const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, CLIPBOARD_TYPE_COPY_PASTE);
  g_map.Get().Clear();

  for (ObjectMap::const_iterator iter = objects.begin(); iter != objects.end();
       ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }

  g_map.Get().CommitToAndroidClipboard();
}

void ClipboardAndroid::WriteText(const char* text_data, size_t text_len) {
  g_map.Get().Set(kPlainTextFormat, std::string(text_data, text_len));
}

void ClipboardAndroid::WriteHTML(const char* markup_data,
                                 size_t markup_len,
                                 const char* url_data,
                                 size_t url_len) {
  g_map.Get().Set(kHTMLFormat, std::string(markup_data, markup_len));
}

void ClipboardAndroid::WriteRTF(const char* rtf_data, size_t data_len) {
  NOTIMPLEMENTED();
}

// Note: according to other platforms implementations, this really writes the
// URL spec.
void ClipboardAndroid::WriteBookmark(const char* title_data,
                                     size_t title_len,
                                     const char* url_data,
                                     size_t url_len) {
  g_map.Get().Set(kBookmarkFormat, std::string(url_data, url_len));
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardAndroid::WriteWebSmartPaste() {
  g_map.Get().Set(kWebKitSmartPasteFormat, std::string());
}

// Note: we implement this to pass all unit tests but it is currently unclear
// how some code would consume this.
void ClipboardAndroid::WriteBitmap(const SkBitmap& bitmap) {
  gfx::Size size(bitmap.width(), bitmap.height());

  std::string packed(reinterpret_cast<const char*>(&size), sizeof(size));
  {
    SkAutoLockPixels bitmap_lock(bitmap);
    packed += std::string(static_cast<const char*>(bitmap.getPixels()),
                          bitmap.getSize());
  }
  g_map.Get().Set(kBitmapFormat, packed);
}

void ClipboardAndroid::WriteData(const Clipboard::FormatType& format,
                                 const char* data_data,
                                 size_t data_len) {
  g_map.Get().Set(format.ToString(), std::string(data_data, data_len));
}

bool RegisterClipboardAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace ui
