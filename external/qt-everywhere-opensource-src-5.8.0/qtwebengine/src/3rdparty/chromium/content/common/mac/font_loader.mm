// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/font_loader.h"

#import <Cocoa/Cocoa.h>

#include <limits>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/common/mac/font_descriptor.h"

#include <map>

namespace {

uint32_t GetFontIDForFont(const base::FilePath& font_path) {
  // content/common can't depend on content/browser, so this cannot call
  // BrowserThread::CurrentlyOn(). Check this is always called on the same
  // thread.
  static pthread_t thread_id = pthread_self();
  DCHECK_EQ(pthread_self(), thread_id);

  // Font loading used to call ATSFontGetContainer()
  // and used that as font id.
  // ATS is deprecated and CTFont doesn't seem to have a obvious fixed id for a
  // font. Since this function is only called from a single thread, use a static
  // map to store ids.
  typedef std::map<base::FilePath, uint32_t> FontIdMap;
  CR_DEFINE_STATIC_LOCAL(FontIdMap, font_ids, ());

  auto it = font_ids.find(font_path);
  if (it != font_ids.end())
    return it->second;

  uint32_t font_id = font_ids.size() + 1;
  font_ids[font_path] = font_id;
  return font_id;
}

}  // namespace

// static
void FontLoader::LoadFont(const FontDescriptor& font,
                          FontLoader::Result* result) {
  base::ThreadRestrictions::AssertIOAllowed();

  DCHECK(result);
  result->font_data_size = 0;
  result->font_id = 0;

  NSFont* font_to_encode = font.ToNSFont();

  // Load appropriate NSFont.
  if (!font_to_encode) {
    DLOG(ERROR) << "Failed to load font " << font.font_name;
    return;
  }

  // NSFont -> File path.
  // Warning: Calling this function on a font activated from memory will result
  // in failure with a -50 - paramErr.  This may occur if
  // CreateCGFontFromBuffer() is called in the same process as this function
  // e.g. when writing a unit test that exercises these two functions together.
  // If said unit test were to load a system font and activate it from memory
  // it becomes impossible for the system to the find the original file ref
  // since the font now lives in memory as far as it's concerned.
  CTFontRef ct_font_to_encode = (CTFontRef)font_to_encode;
  base::scoped_nsobject<NSURL> font_url(
      base::mac::CFToNSCast(base::mac::CFCastStrict<CFURLRef>(
          CTFontCopyAttribute(ct_font_to_encode, kCTFontURLAttribute))));
  if (![font_url isFileURL]) {
    DLOG(ERROR) << "Failed to find font file for " << font.font_name;
    return;
  }

  base::FilePath font_path = base::mac::NSStringToFilePath([font_url path]);

  // Load file into shared memory buffer.
  int64_t font_file_size_64 = -1;
  if (!base::GetFileSize(font_path, &font_file_size_64)) {
    DLOG(ERROR) << "Couldn't get font file size for " << font_path.value();
    return;
  }

  if (font_file_size_64 <= 0 ||
      font_file_size_64 >= std::numeric_limits<int32_t>::max()) {
    DLOG(ERROR) << "Bad size for font file " << font_path.value();
    return;
  }

  int32_t font_file_size_32 = static_cast<int32_t>(font_file_size_64);
  if (!result->font_data.CreateAndMapAnonymous(font_file_size_32)) {
    DLOG(ERROR) << "Failed to create shmem area for " << font.font_name;
    return;
  }

  int32_t amt_read = base::ReadFile(
      font_path, reinterpret_cast<char*>(result->font_data.memory()),
      font_file_size_32);
  if (amt_read != font_file_size_32) {
    DLOG(ERROR) << "Failed to read font data for " << font_path.value();
    return;
  }

  result->font_data_size = font_file_size_32;
  result->font_id = GetFontIDForFont(font_path);
}

// static
bool FontLoader::CGFontRefFromBuffer(base::SharedMemoryHandle font_data,
                                     uint32_t font_data_size,
                                     CGFontRef* out) {
  *out = NULL;

  using base::SharedMemory;
  DCHECK(SharedMemory::IsHandleValid(font_data));
  DCHECK_GT(font_data_size, 0U);

  SharedMemory shm(font_data, /*read_only=*/true);
  if (!shm.Map(font_data_size))
    return false;

  NSData* data = [NSData dataWithBytes:shm.memory()
                                length:font_data_size];
  base::ScopedCFTypeRef<CGDataProviderRef> provider(
      CGDataProviderCreateWithCFData(base::mac::NSToCFCast(data)));
  if (!provider)
    return false;

  *out = CGFontCreateWithDataProvider(provider.get());

  if (*out == NULL)
    return false;

  return true;
}
