// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/pnacl_resources.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

namespace {

static const char kPnaclBaseUrl[] = "chrome://pnacl-translator/";

nacl::string GetFullUrl(const nacl::string& partial_url) {
  return nacl::string(kPnaclBaseUrl) + GetNaClInterface()->GetSandboxArch() +
         "/" + partial_url;
}

}  // namespace

PnaclResources::~PnaclResources() {
  if (llc_file_handle_ != PP_kInvalidFileHandle)
    CloseFileHandle(llc_file_handle_);
  if (ld_file_handle_ != PP_kInvalidFileHandle)
    CloseFileHandle(ld_file_handle_);
}

void PnaclResources::ReadResourceInfo(
    const pp::CompletionCallback& resource_info_read_cb) {
  PP_Var pp_llc_tool_name_var;
  PP_Var pp_ld_tool_name_var;
  if (!plugin_->nacl_interface()->GetPnaclResourceInfo(
          plugin_->pp_instance(),
          "chrome://pnacl-translator/pnacl.json",
          &pp_llc_tool_name_var,
          &pp_ld_tool_name_var)) {
    pp::Module::Get()->core()->CallOnMainThread(0,
                                                resource_info_read_cb,
                                                PP_ERROR_FAILED);
    return;
  }
  pp::Var llc_tool_name(pp::PASS_REF, pp_llc_tool_name_var);
  pp::Var ld_tool_name(pp::PASS_REF, pp_ld_tool_name_var);
  llc_tool_name_ = GetFullUrl(llc_tool_name.AsString());
  ld_tool_name_ = GetFullUrl(ld_tool_name.AsString());
  pp::Module::Get()->core()->CallOnMainThread(0, resource_info_read_cb, PP_OK);
}

PP_FileHandle PnaclResources::TakeLlcFileHandle() {
  PP_FileHandle to_return = llc_file_handle_;
  llc_file_handle_ = PP_kInvalidFileHandle;
  return to_return;
}

PP_FileHandle PnaclResources::TakeLdFileHandle() {
  PP_FileHandle to_return = ld_file_handle_;
  ld_file_handle_ = PP_kInvalidFileHandle;
  return to_return;
}

void PnaclResources::StartLoad(
    const pp::CompletionCallback& all_loaded_callback) {
  PLUGIN_PRINTF(("PnaclResources::StartLoad\n"));

  // Do a blocking load of each of the resources.
  llc_file_handle_ =
      plugin_->nacl_interface()->GetReadonlyPnaclFd(llc_tool_name_.c_str());
  ld_file_handle_ =
      plugin_->nacl_interface()->GetReadonlyPnaclFd(ld_tool_name_.c_str());

  int32_t result = PP_OK;
  if (llc_file_handle_ == PP_kInvalidFileHandle ||
      ld_file_handle_ == PP_kInvalidFileHandle) {
    result = PP_ERROR_FILENOTFOUND;
  }
  pp::Module::Get()->core()->CallOnMainThread(0, all_loaded_callback, result);
}

}  // namespace plugin
