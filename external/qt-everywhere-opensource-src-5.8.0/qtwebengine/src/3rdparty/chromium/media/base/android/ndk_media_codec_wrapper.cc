// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file defines the symbols from NDKMediaCodec that android is using. It
// then loads the library dynamically on first use.

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"

#define LOOKUP_FUNC(func_name, return_type, args, arg_names)                   \
  return_type func_name args {                                                 \
    typedef return_type(*signature) args;                                      \
    static signature g_##func_name =                                           \
        reinterpret_cast<signature>(base::GetFunctionPointerFromNativeLibrary( \
            LibraryHandle(), #func_name));                                     \
    return g_##func_name arg_names;                                            \
  }

// The constants used in chromium. Those symbols are defined as extern symbols
// in the NdkMediaFormat headers. They will be initialized to their correct
// values when the library is loaded.
const char* AMEDIAFORMAT_KEY_CHANNEL_COUNT;
const char* AMEDIAFORMAT_KEY_HEIGHT;
const char* AMEDIAFORMAT_KEY_SAMPLE_RATE;
const char* AMEDIAFORMAT_KEY_WIDTH;

namespace {

// The name of the library to load.
const char kMediaNDKLibraryName[] = "libmediandk.so";

// Loads the OpenSLES library, and initializes all the proxies.
base::NativeLibrary IntializeLibraryHandle() {
  base::NativeLibrary handle =
      base::LoadNativeLibrary(base::FilePath(kMediaNDKLibraryName), NULL);
  DCHECK(handle) << "Unable to load " << kMediaNDKLibraryName;

  // Setup the proxy for each symbol.
  // Attach the symbol name to the proxy address.
  struct SymbolDefinition {
    const char* name;
    const char** value;
  };

  // The list of defined symbols.
  const SymbolDefinition kSymbols[] = {
      {"AMEDIAFORMAT_KEY_CHANNEL_COUNT", &AMEDIAFORMAT_KEY_CHANNEL_COUNT},
      {"AMEDIAFORMAT_KEY_HEIGHT", &AMEDIAFORMAT_KEY_HEIGHT},
      {"AMEDIAFORMAT_KEY_SAMPLE_RATE", &AMEDIAFORMAT_KEY_SAMPLE_RATE},
      {"AMEDIAFORMAT_KEY_WIDTH", &AMEDIAFORMAT_KEY_WIDTH},
  };

  for (size_t i = 0; i < sizeof(kSymbols) / sizeof(kSymbols[0]); ++i) {
    memcpy(kSymbols[i].value,
           base::GetFunctionPointerFromNativeLibrary(handle, kSymbols[i].name),
           sizeof(char*));
    DCHECK(*kSymbols[i].value) << "Unable to find symbol for "
                               << kSymbols[i].name;
  }
  return handle;
}

// Returns the handler to the shared library. The library itself will be lazily
// loaded during the first call to this function.
base::NativeLibrary LibraryHandle() {
  // The handle is lazily initialized on the first call.
  static base::NativeLibrary g_mediandk_LibraryHandle =
      IntializeLibraryHandle();
  return g_mediandk_LibraryHandle;
}
}

LOOKUP_FUNC(AMediaCodec_createDecoderByType,
            AMediaCodec*,
            (const char* mime_type),
            (mime_type));
LOOKUP_FUNC(AMediaCodec_createEncoderByType,
            AMediaCodec*,
            (const char* mime_type),
            (mime_type));
LOOKUP_FUNC(AMediaCodec_delete, media_status_t, (AMediaCodec * codec), (codec));
LOOKUP_FUNC(AMediaCodec_configure,
            media_status_t,
            (AMediaCodec * codec,
             const AMediaFormat* format,
             ANativeWindow* surface,
             AMediaCrypto* crypto,
             uint32_t flags),
            (codec, format, surface, crypto, flags));
LOOKUP_FUNC(AMediaCodec_start, media_status_t, (AMediaCodec * codec), (codec));
LOOKUP_FUNC(AMediaCodec_stop, media_status_t, (AMediaCodec * codec), (codec));
LOOKUP_FUNC(AMediaCodec_flush, media_status_t, (AMediaCodec * codec), (codec));
LOOKUP_FUNC(AMediaCodec_getInputBuffer,
            uint8_t*,
            (AMediaCodec * codec, size_t idx, size_t* out_size),
            (codec, idx, out_size));
LOOKUP_FUNC(AMediaCodec_getOutputBuffer,
            uint8_t*,
            (AMediaCodec * codec, size_t idx, size_t* out_size),
            (codec, idx, out_size));
LOOKUP_FUNC(AMediaCodec_dequeueInputBuffer,
            ssize_t,
            (AMediaCodec * codec, int64_t timeoutUs),
            (codec, timeoutUs));
LOOKUP_FUNC(AMediaCodec_queueInputBuffer,
            media_status_t,
            (AMediaCodec * codec,
             size_t idx,
             off_t offset,
             size_t size,
             uint64_t time,
             uint32_t flags),
            (codec, idx, offset, size, time, flags));
LOOKUP_FUNC(AMediaCodec_queueSecureInputBuffer,
            media_status_t,
            (AMediaCodec * codec,
             size_t idx,
             off_t offset,
             AMediaCodecCryptoInfo* cryto,
             uint64_t time,
             uint32_t flags),
            (codec, idx, offset, cryto, time, flags));
LOOKUP_FUNC(AMediaCodec_dequeueInputBuffer,
            ssize_t,
            (AMediaCodec * codec,
             AMediaCodecBufferInfo* info,
             int64_t timeoutUs),
            (codec, info, timeoutUs));
LOOKUP_FUNC(AMediaCodec_dequeueOutputBuffer,
            ssize_t,
            (AMediaCodec * codec,
             AMediaCodecBufferInfo* info,
             int64_t timeoutUs),
            (codec, info, timeoutUs));
LOOKUP_FUNC(AMediaCodec_getOutputFormat,
            AMediaFormat*,
            (AMediaCodec * codec),
            (codec));
LOOKUP_FUNC(AMediaCodec_releaseOutputBuffer,
            media_status_t,
            (AMediaCodec * codec, size_t idx, bool render),
            (codec, idx, render));
LOOKUP_FUNC(AMediaCodecCryptoInfo_new,
            AMediaCodecCryptoInfo*,
            (int num_subsamples,
             uint8_t key[16],
             uint8_t iv[16],
             cryptoinfo_mode_t mode,
             size_t* clear_bytes,
             size_t* encrypted_bytes),
            (num_subsamples, key, iv, mode, clear_bytes, encrypted_bytes));
LOOKUP_FUNC(AMediaCodecCryptoInfo_delete,
            media_status_t,
            (AMediaCodecCryptoInfo * info),
            (info));
LOOKUP_FUNC(AMediaFormat_delete,
            media_status_t,
            (AMediaFormat * format),
            (format));
LOOKUP_FUNC(AMediaFormat_getInt32,
            bool,
            (AMediaFormat * format, const char* name, int32_t* out),
            (format, name, out));
