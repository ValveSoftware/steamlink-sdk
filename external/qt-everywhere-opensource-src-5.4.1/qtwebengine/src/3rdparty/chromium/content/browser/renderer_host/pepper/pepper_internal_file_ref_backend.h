// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_INTERNAL_FILE_REF_BACKEND_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_INTERNAL_FILE_REF_BACKEND_H_

#include <string>

#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/ppapi_host.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace content {

class PepperFileSystemBrowserHost;

// Implementations of FileRef operations for internal filesystems.
class PepperInternalFileRefBackend : public PepperFileRefBackend {
 public:
  PepperInternalFileRefBackend(
      ppapi::host::PpapiHost* host,
      int render_process_id,
      base::WeakPtr<PepperFileSystemBrowserHost> fs_host,
      const std::string& path);
  virtual ~PepperInternalFileRefBackend();

  // PepperFileRefBackend overrides.
  virtual int32_t MakeDirectory(ppapi::host::ReplyMessageContext context,
                                int32_t make_directory_flags) OVERRIDE;
  virtual int32_t Touch(ppapi::host::ReplyMessageContext context,
                        PP_Time last_accessed_time,
                        PP_Time last_modified_time) OVERRIDE;
  virtual int32_t Delete(ppapi::host::ReplyMessageContext context) OVERRIDE;
  virtual int32_t Rename(ppapi::host::ReplyMessageContext context,
                         PepperFileRefHost* new_file_ref) OVERRIDE;
  virtual int32_t Query(ppapi::host::ReplyMessageContext context) OVERRIDE;
  virtual int32_t ReadDirectoryEntries(ppapi::host::ReplyMessageContext context)
      OVERRIDE;
  virtual int32_t GetAbsolutePath(ppapi::host::ReplyMessageContext context)
      OVERRIDE;
  virtual fileapi::FileSystemURL GetFileSystemURL() const OVERRIDE;
  virtual base::FilePath GetExternalFilePath() const OVERRIDE;

  virtual int32_t CanRead() const OVERRIDE;
  virtual int32_t CanWrite() const OVERRIDE;
  virtual int32_t CanCreate() const OVERRIDE;
  virtual int32_t CanReadWrite() const OVERRIDE;

 private:
  // Generic reply callback.
  void DidFinish(ppapi::host::ReplyMessageContext reply_context,
                 const IPC::Message& msg,
                 base::File::Error error);

  // Operation specific callbacks.
  void GetMetadataComplete(ppapi::host::ReplyMessageContext reply_context,
                           base::File::Error error,
                           const base::File::Info& file_info);
  void ReadDirectoryComplete(
      ppapi::host::ReplyMessageContext context,
      fileapi::FileSystemOperation::FileEntryList* accumulated_file_list,
      base::File::Error error,
      const fileapi::FileSystemOperation::FileEntryList& file_list,
      bool has_more);

  scoped_refptr<fileapi::FileSystemContext> GetFileSystemContext() const;

  ppapi::host::PpapiHost* host_;
  int render_process_id_;
  base::WeakPtr<PepperFileSystemBrowserHost> fs_host_;
  PP_FileSystemType fs_type_;
  std::string path_;

  mutable fileapi::FileSystemURL fs_url_;

  base::WeakPtrFactory<PepperInternalFileRefBackend> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperInternalFileRefBackend);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_INTERNAL_FILE_REF_BACKEND_H_
