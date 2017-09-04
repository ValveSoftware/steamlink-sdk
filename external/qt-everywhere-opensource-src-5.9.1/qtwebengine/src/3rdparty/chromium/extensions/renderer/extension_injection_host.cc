// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_injection_host.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

ExtensionInjectionHost::ExtensionInjectionHost(
    const Extension* extension)
    : InjectionHost(HostID(HostID::EXTENSIONS, extension->id())),
      extension_(extension) {
}

ExtensionInjectionHost::~ExtensionInjectionHost() {
}

// static
std::unique_ptr<const InjectionHost> ExtensionInjectionHost::Create(
    const std::string& extension_id) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension)
    return std::unique_ptr<const ExtensionInjectionHost>();
  return std::unique_ptr<const ExtensionInjectionHost>(
      new ExtensionInjectionHost(extension));
}

std::string ExtensionInjectionHost::GetContentSecurityPolicy() const {
  return CSPInfo::GetContentSecurityPolicy(extension_);
}

const GURL& ExtensionInjectionHost::url() const {
  return extension_->url();
}

const std::string& ExtensionInjectionHost::name() const {
  return extension_->name();
}

PermissionsData::AccessType ExtensionInjectionHost::CanExecuteOnFrame(
    const GURL& document_url,
    content::RenderFrame* render_frame,
    int tab_id,
    bool is_declarative) const {
  blink::WebSecurityOrigin top_frame_security_origin =
      render_frame->GetWebFrame()->top()->getSecurityOrigin();
  // Only whitelisted extensions may run scripts on another extension's page.
  if (top_frame_security_origin.protocol().utf8() == kExtensionScheme &&
      top_frame_security_origin.host().utf8() != extension_->id() &&
      !PermissionsData::CanExecuteScriptEverywhere(extension_))
    return PermissionsData::ACCESS_DENIED;

  // Declarative user scripts use "page access" (from "permissions" section in
  // manifest) whereas non-declarative user scripts use custom
  // "content script access" logic.
  PermissionsData::AccessType access = PermissionsData::ACCESS_ALLOWED;
  if (is_declarative) {
    access = extension_->permissions_data()->GetPageAccess(
        extension_,
        document_url,
        tab_id,
        nullptr /* ignore error */);
  } else {
    access = extension_->permissions_data()->GetContentScriptAccess(
        extension_,
        document_url,
        tab_id,
        nullptr /* ignore error */);
  }
  if (access == PermissionsData::ACCESS_WITHHELD &&
      (tab_id == -1 || render_frame->GetWebFrame()->parent())) {
    // Note: we don't consider ACCESS_WITHHELD for child frames or for frames
    // outside of tabs because there is nowhere to surface a request.
    // TODO(devlin): We should ask for permission somehow. crbug.com/491402.
    access = PermissionsData::ACCESS_DENIED;
  }
  return access;
}

}  // namespace extensions
