// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_TEMPORARY_FILE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_TEMPORARY_FILE_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "ppapi/c/private/pp_file_handle.h"

namespace plugin {

class Plugin;

// Translation creates two temporary files.  The first temporary file holds
// the object file created by llc.  The second holds the nexe produced by
// the linker.  Both of these temporary files are used to both write and
// read according to the following matrix:
//
// PnaclCoordinator::obj_file_:
//     written by: llc     (passed in explicitly through SRPC)
//     read by:    ld      (returned via lookup service from SRPC)
// PnaclCoordinator::nexe_file_:
//     written by: ld      (passed in explicitly through SRPC)
//     read by:    sel_ldr (passed in explicitly to command channel)
//

// TempFile represents a file used as a temporary between stages in
// translation.  It is automatically deleted when all handles are closed
// (or earlier -- immediately unlinked on POSIX systems).  The file is only
// opened, once, but because both reading and writing are necessary (and in
// different processes), the user should reset / seek back to the beginning
// of the file between sessions.
class TempFile {
 public:
  // Create a TempFile.
  explicit TempFile(Plugin* plugin);
  ~TempFile();

  // Opens a temporary file object and descriptor wrapper referring to the file.
  // If |writeable| is true, the descriptor will be opened for writing, and
  // write_wrapper will return a valid pointer, otherwise it will return NULL.
  int32_t Open(bool writeable);
  // Resets file position of the handle, for reuse.
  bool Reset();

  // Accessors.
  // The nacl::DescWrapper* for the writeable version of the file.
  nacl::DescWrapper* write_wrapper() { return write_wrapper_.get(); }
  nacl::DescWrapper* read_wrapper() { return read_wrapper_.get(); }

  // Returns the handle to the file repesented and resets the internal handle
  // and all wrappers.
  PP_FileHandle TakeFileHandle();

  // Used by GetNexeFd() to set the underlying internal handle.
  PP_FileHandle* internal_handle() { return &internal_handle_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(TempFile);

  Plugin* plugin_;
  nacl::scoped_ptr<nacl::DescWrapper> read_wrapper_;
  nacl::scoped_ptr<nacl::DescWrapper> write_wrapper_;
  PP_FileHandle internal_handle_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_TEMPORARY_FILE_H_
