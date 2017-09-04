// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/cdm/pepper_cdm_wrapper_impl.h"

#include <utility>

#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebHelperPlugin.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/origin.h"

namespace content {

void WebHelperPluginDeleter::operator()(blink::WebHelperPlugin* plugin) const {
  plugin->destroy();
}

std::unique_ptr<PepperCdmWrapper> PepperCdmWrapperImpl::Create(
    blink::WebLocalFrame* frame,
    const std::string& pluginType,
    const GURL& security_origin) {
  DCHECK(frame);

  // The frame security origin could be different from the origin where the CDM
  // creation was initiated, e.g. due to navigation.
  // Note: The code will continue after navigation to the "same" origin, even
  // though the CDM is no longer necessary.
  // TODO: Consider avoiding this possibility entirely. http://crbug.com/575236
  GURL frame_security_origin(url::Origin(frame->getSecurityOrigin()).GetURL());
  if (frame_security_origin != security_origin) {
    LOG(ERROR) << "Frame has a different origin than the EME call.";
    return std::unique_ptr<PepperCdmWrapper>();
  }

  ScopedHelperPlugin helper_plugin(blink::WebHelperPlugin::create(
      blink::WebString::fromUTF8(pluginType), frame));
  if (!helper_plugin)
    return std::unique_ptr<PepperCdmWrapper>();

  blink::WebPlugin* plugin = helper_plugin->getPlugin();
  DCHECK(!plugin->isPlaceholder());  // Prevented by Blink.

  // Only Pepper plugins are supported, so it must ultimately be a ppapi object.
  PepperWebPluginImpl* ppapi_plugin = static_cast<PepperWebPluginImpl*>(plugin);
  scoped_refptr<PepperPluginInstanceImpl> plugin_instance =
      ppapi_plugin->instance();
  if (!plugin_instance.get())
    return std::unique_ptr<PepperCdmWrapper>();

  GURL plugin_url(plugin_instance->container()->document().url());
  GURL plugin_security_origin = plugin_url.GetOrigin();
  CHECK_EQ(security_origin, plugin_security_origin)
      << "Pepper instance has a different origin than the EME call.";

  if (!plugin_instance->GetContentDecryptorDelegate())
    return std::unique_ptr<PepperCdmWrapper>();

  return std::unique_ptr<PepperCdmWrapper>(
      new PepperCdmWrapperImpl(std::move(helper_plugin), plugin_instance));
}

PepperCdmWrapperImpl::PepperCdmWrapperImpl(
    ScopedHelperPlugin helper_plugin,
    const scoped_refptr<PepperPluginInstanceImpl>& plugin_instance)
    : helper_plugin_(std::move(helper_plugin)),
      plugin_instance_(plugin_instance) {
  DCHECK(helper_plugin_);
  DCHECK(plugin_instance_.get());
  // Plugin must be a CDM.
  DCHECK(plugin_instance_->GetContentDecryptorDelegate());
}

PepperCdmWrapperImpl::~PepperCdmWrapperImpl() {
  // Destroy the nested objects in reverse order.
  plugin_instance_ = NULL;
  helper_plugin_.reset();
}

ContentDecryptorDelegate* PepperCdmWrapperImpl::GetCdmDelegate() {
  return plugin_instance_->GetContentDecryptorDelegate();
}

}  // namespace content

#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)
