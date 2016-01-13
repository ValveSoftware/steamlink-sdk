// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FLASH_FILE_RESOURCE_H_
#define PPAPI_PROXY_FLASH_FILE_RESOURCE_H_

#include <string>

#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/file_path.h"
#include "ppapi/thunk/ppb_flash_file_api.h"

namespace ppapi {
namespace proxy {

class FlashFileResource
    : public PluginResource,
      public thunk::PPB_Flash_File_API {
 public:
  FlashFileResource(Connection connection, PP_Instance instance);
  virtual ~FlashFileResource();

  // Resource overrides.
  virtual thunk::PPB_Flash_File_API* AsPPB_Flash_File_API() OVERRIDE;

  // PPB_Flash_Functions_API.
  virtual int32_t OpenFile(PP_Instance instance,
                           const char* path,
                           int32_t mode,
                           PP_FileHandle* file) OVERRIDE;
  virtual int32_t RenameFile(PP_Instance instance,
                             const char* path_from,
                             const char* path_to) OVERRIDE;
  virtual int32_t DeleteFileOrDir(PP_Instance instance,
                                  const char* path,
                                  PP_Bool recursive) OVERRIDE;
  virtual int32_t CreateDir(PP_Instance instance, const char* path) OVERRIDE;
  virtual int32_t QueryFile(PP_Instance instance,
                            const char* path,
                            PP_FileInfo* info) OVERRIDE;
  virtual int32_t GetDirContents(PP_Instance instance,
                                 const char* path,
                                 PP_DirContents_Dev** contents) OVERRIDE;
  virtual void FreeDirContents(PP_Instance instance,
                               PP_DirContents_Dev* contents) OVERRIDE;
  virtual int32_t CreateTemporaryFile(PP_Instance instance,
                                      PP_FileHandle* file) OVERRIDE;
  virtual int32_t OpenFileRef(PP_Instance instance,
                              PP_Resource file_ref,
                              int32_t mode,
                              PP_FileHandle* file) OVERRIDE;
  virtual int32_t QueryFileRef(PP_Instance instance,
                               PP_Resource file_ref,
                               PP_FileInfo* info) OVERRIDE;

 private:
  int32_t OpenFileHelper(const std::string& path,
                         PepperFilePath::Domain domain_type,
                         int32_t mode,
                         PP_FileHandle* file);
  int32_t QueryFileHelper(const std::string& path,
                          PepperFilePath::Domain domain_type,
                          PP_FileInfo* info);

  DISALLOW_COPY_AND_ASSIGN(FlashFileResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FLASH_FILE_RESOURCE_H_
