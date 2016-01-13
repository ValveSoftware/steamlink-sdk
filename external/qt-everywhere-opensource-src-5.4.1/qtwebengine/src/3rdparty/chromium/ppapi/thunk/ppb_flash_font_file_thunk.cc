// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/private/ppb_flash_font_file.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_flash_font_file_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   const PP_BrowserFont_Trusted_Description* description,
                   PP_PrivateFontCharset charset) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFlashFontFile(instance, description, charset);
}

PP_Bool IsFlashFontFile(PP_Resource resource) {
  EnterResource<PPB_Flash_FontFile_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool GetFontTable(PP_Resource font_file,
                     uint32_t table,
                     void* output,
                     uint32_t* output_length) {
  EnterResource<PPB_Flash_FontFile_API> enter(font_file, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetFontTable(table, output, output_length);
}

const PPB_Flash_FontFile g_ppb_flash_fontfile_thunk = {
  &Create,
  &IsFlashFontFile,
  &GetFontTable
};

}  // namespace

const PPB_Flash_FontFile_0_1* GetPPB_Flash_FontFile_0_1_Thunk() {
  return &g_ppb_flash_fontfile_thunk;
}

}  // namespace thunk
}  // namespace ppapi
