// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FLASH_FONT_FILE_RESOURCE_H_
#define PPAPI_PROXY_FLASH_FONT_FILE_RESOURCE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "ppapi/c/private/pp_private_font_charset.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/thunk/ppb_flash_font_file_api.h"

struct PP_BrowserFont_Trusted_Description;

namespace ppapi {
namespace proxy {

// TODO(yzshen): write unittest and browser test.
class FlashFontFileResource : public PluginResource,
                              public thunk::PPB_Flash_FontFile_API {
 public:
  FlashFontFileResource(Connection connection,
                        PP_Instance instance,
                        const PP_BrowserFont_Trusted_Description* description,
                        PP_PrivateFontCharset charset);
  virtual ~FlashFontFileResource();

  // Resource overrides.
  virtual thunk::PPB_Flash_FontFile_API* AsPPB_Flash_FontFile_API() OVERRIDE;

  // PPB_Flash_FontFile_API.
  virtual PP_Bool GetFontTable(uint32_t table,
                               void* output,
                               uint32_t* output_length) OVERRIDE;

 private:
  // Sees if we have a cache of the font table and returns a pointer to it.
  // Returns NULL if we don't have it.
  std::string* GetFontTable(uint32_t table) const;

  std::string* AddFontTable(uint32_t table, const std::string& contents);

  typedef std::map<uint32_t, linked_ptr<std::string> > FontTableMap;
  FontTableMap font_tables_;

  SerializedFontDescription description_;
  PP_PrivateFontCharset charset_;

  DISALLOW_COPY_AND_ASSIGN(FlashFontFileResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FLASH_FONT_FILE_RESOURCE_H_
