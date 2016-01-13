// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/font_loader.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/common/mac/font_descriptor.h"

#include <map>

extern "C" {

// Work around http://crbug.com/93191, a really nasty memory smasher bug.
// On Mac OS X 10.7 ("Lion"), ATS writes to memory it doesn't own.
// SendDeactivateFontsInContainerMessage, called by ATSFontDeactivate,
// may trash memory whenever dlsym(RTLD_DEFAULT,
// "_CTFontManagerUnregisterFontForData") returns NULL. In that case, it tries
// to locate that symbol in the CoreText framework, doing some extremely
// sloppy string handling resulting in a likelihood that the string
// "Text.framework/Versions/A/CoreText" will be written over memory that it
// doesn't own. The kicker here is that Apple dlsym always inserts its own
// leading underscore, so ATS actually winds up looking up a
// __CTFontManagerUnregisterFontForData symbol, which doesn't even exist in
// CoreText. It's only got the single-underscore variant corresponding to an
// underscoreless extern "C" name.
//
// Providing a single-underscored extern "C" function by this name results in
// a __CTFontManagerUnregisterFontForData symbol that, as long as it's public
// (not private extern) and unstripped, ATS will find. If it finds it, it
// avoids making amateur string mistakes that ruin everyone else's good time.
//
// Since ATS wouldn't normally be able to call this function anyway, it's just
// left as a no-op here.
//
// This file seems as good as any other to place this function. It was chosen
// because it already interfaces with ATS for other reasons.
//
// SendDeactivateFontsInContainerMessage on 10.6 ("Snow Leopard") appears to
// share this bug but this sort of memory corruption wasn't detected until
// 10.7. The implementation in 10.5 ("Leopard") does not have this problem.
__attribute__((visibility("default")))
void _CTFontManagerUnregisterFontForData(NSUInteger, int) {
}

}  // extern "C"

namespace {

uint32 GetFontIDForFont(const base::FilePath& font_path) {
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
  typedef std::map<base::FilePath, uint32> FontIdMap;
  CR_DEFINE_STATIC_LOCAL(FontIdMap, font_ids, ());

  auto it = font_ids.find(font_path);
  if (it != font_ids.end())
    return it->second;

  uint32 font_id = font_ids.size() + 1;
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
  // Used only for logging.
  std::string font_name([[font_to_encode fontName] UTF8String]);

  // Load appropriate NSFont.
  if (!font_to_encode) {
    DLOG(ERROR) << "Failed to load font " << font_name;
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
    DLOG(ERROR) << "Failed to find font file for " << font_name;
    return;
  }

  base::FilePath font_path = base::mac::NSStringToFilePath([font_url path]);

  // Load file into shared memory buffer.
  int64 font_file_size_64 = -1;
  if (!base::GetFileSize(font_path, &font_file_size_64)) {
    DLOG(ERROR) << "Couldn't get font file size for " << font_path.value();
    return;
  }

  if (font_file_size_64 <= 0 || font_file_size_64 >= kint32max) {
    DLOG(ERROR) << "Bad size for font file " << font_path.value();
    return;
  }

  int32 font_file_size_32 = static_cast<int32>(font_file_size_64);
  if (!result->font_data.CreateAndMapAnonymous(font_file_size_32)) {
    DLOG(ERROR) << "Failed to create shmem area for " << font_name;
    return;
  }

  int32 amt_read = base::ReadFile(font_path,
      reinterpret_cast<char*>(result->font_data.memory()),
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
                                     uint32 font_data_size,
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
