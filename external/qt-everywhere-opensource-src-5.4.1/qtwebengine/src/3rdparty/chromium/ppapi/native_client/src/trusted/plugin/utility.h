/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A collection of debugging related interfaces.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/c/private/ppb_nacl_private.h"

#define SRPC_PLUGIN_DEBUG 1

namespace plugin {

// Tests that a string is a valid JavaScript identifier.  According to the
// ECMAScript spec, this should be done in terms of unicode character
// categories.  For now, we are simply limiting identifiers to the ASCII
// subset of that spec.  If successful, it returns the length of the
// identifier in the location pointed to by length (if it is not NULL).
// TODO(sehr): add Unicode identifier support.
bool IsValidIdentifierString(const char* strval, uint32_t* length);

const PPB_NaCl_Private* GetNaClInterface();
void SetNaClInterface(const PPB_NaCl_Private* nacl_interface);

void CloseFileHandle(PP_FileHandle file_handle);

// Converts a PP_FileHandle to a POSIX file descriptor.
int32_t ConvertFileDescriptor(PP_FileHandle handle, bool read_only);

// Debugging print utility
extern int gNaClPluginDebugPrintEnabled;
extern int NaClPluginPrintLog(const char *format, ...);
extern int NaClPluginDebugPrintCheckEnv();
#if SRPC_PLUGIN_DEBUG
#define INIT_PLUGIN_LOGGING() do {                                    \
    if (-1 == ::plugin::gNaClPluginDebugPrintEnabled) {               \
      ::plugin::gNaClPluginDebugPrintEnabled =                        \
          ::plugin::NaClPluginDebugPrintCheckEnv();                   \
    }                                                                 \
} while (0)

#define PLUGIN_PRINTF(args) do {                                      \
    INIT_PLUGIN_LOGGING();                                            \
    if (0 != ::plugin::gNaClPluginDebugPrintEnabled) {                \
      ::plugin::NaClPluginPrintLog("PLUGIN %" NACL_PRIu64 ": ",       \
                                   NaClGetTimeOfDayMicroseconds());   \
      ::plugin::NaClPluginPrintLog args;                              \
    }                                                                 \
  } while (0)

// MODULE_PRINTF is used in the module because PLUGIN_PRINTF uses a
// a timer that may not yet be initialized.
#define MODULE_PRINTF(args) do {                                      \
    INIT_PLUGIN_LOGGING();                                            \
    if (0 != ::plugin::gNaClPluginDebugPrintEnabled) {                \
      ::plugin::NaClPluginPrintLog("MODULE: ");                       \
      ::plugin::NaClPluginPrintLog args;                              \
    }                                                                 \
  } while (0)
#else
#  define PLUGIN_PRINTF(args) do { if (0) { printf args; } } while (0)
#  define MODULE_PRINTF(args) do { if (0) { printf args; } } while (0)
/* allows DCE but compiler can still do format string checks */
#endif

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_
