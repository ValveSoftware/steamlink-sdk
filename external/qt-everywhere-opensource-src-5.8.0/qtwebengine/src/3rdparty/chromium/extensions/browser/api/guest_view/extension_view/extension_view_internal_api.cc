// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/extension_view/extension_view_internal_api.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "extensions/browser/guest_view/extension_view/whitelist/extension_view_whitelist.h"
#include "extensions/common/api/extension_view_internal.h"
#include "extensions/common/constants.h"

namespace extensionview = extensions::api::extension_view_internal;

namespace extensions {

bool ExtensionViewInternalExtensionFunction::RunAsync() {
  int instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &instance_id));
  ExtensionViewGuest* guest = ExtensionViewGuest::From(
      render_frame_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;
  return RunAsyncSafe(guest);
}

// Checks the validity of |src|, including that it follows the chrome extension
// scheme and that its extension ID is valid.
// Returns true if |src| is valid.
bool IsSrcValid(GURL src) {
  // Check if src is valid and matches the extension scheme.
  if (!src.is_valid() || !src.SchemeIs(kExtensionScheme))
    return false;

  // Get the extension id and check if it is valid.
  std::string extension_id = src.host();
  if (!crx_file::id_util::IdIsValid(extension_id) ||
      !IsExtensionIdWhitelisted(extension_id))
    return false;

  return true;
}

bool ExtensionViewInternalLoadSrcFunction::RunAsyncSafe(
    ExtensionViewGuest* guest) {
  std::unique_ptr<extensionview::LoadSrc::Params> params(
      extensionview::LoadSrc::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string src = params->src;
  GURL url(src);
  bool has_load_succeeded = false;
  bool is_src_valid = IsSrcValid(url);

  if (is_src_valid)
    has_load_succeeded = guest->NavigateGuest(src, true /* force_navigation */);

  // Return whether load is successful.
  SetResult(base::MakeUnique<base::FundamentalValue>(has_load_succeeded));
  SendResponse(true);
  return true;
}

bool ExtensionViewInternalParseSrcFunction::RunAsync() {
  std::unique_ptr<extensionview::ParseSrc::Params> params(
      extensionview::ParseSrc::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  GURL url(params->src);
  bool is_src_valid = IsSrcValid(url);

  // Return whether the src is valid and the current extension ID to
  // the callback.
  std::unique_ptr<base::ListValue> result_list(new base::ListValue());
  result_list->AppendBoolean(is_src_valid);
  result_list->AppendString(url.host());
  SetResultList(std::move(result_list));
  SendResponse(true);
  return true;
}

}  // namespace extensions
