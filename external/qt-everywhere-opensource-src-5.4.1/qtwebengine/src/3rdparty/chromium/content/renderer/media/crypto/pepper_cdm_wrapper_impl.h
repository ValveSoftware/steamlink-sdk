// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_PEPPER_CDM_WRAPPER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_PEPPER_CDM_WRAPPER_IMPL_H_

#if !defined(ENABLE_PEPPER_CDMS)
#error This file should only be included when ENABLE_PEPPER_CDMS is defined
#endif

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"

namespace blink {
class WebHelperPlugin;
class WebLocalFrame;
}

namespace content {

class ContentDecryptorDelegate;
class PepperPluginInstanceImpl;

// Deleter for blink::WebHelperPlugin.
struct WebHelperPluginDeleter {
  void operator()(blink::WebHelperPlugin* plugin) const;
};

// Implements a wrapper on blink::WebHelperPlugin so that the plugin gets
// destroyed properly. It owns all the objects derived from WebHelperPlugin
// (WebPlugin, PepperPluginInstanceImpl, ContentDecryptionDelegate), and will
// free them as necessary when this wrapper is destroyed. In particular, it
// takes a reference to PepperPluginInstanceImpl so it won't go away until
// this object is destroyed.
//
// Implemented so that lower layers in Chromium don't need to be aware of
// blink:: objects.
class PepperCdmWrapperImpl : public PepperCdmWrapper {
 public:
  static scoped_ptr<PepperCdmWrapper> Create(blink::WebLocalFrame* frame,
                                             const std::string& pluginType,
                                             const GURL& security_origin);

  virtual ~PepperCdmWrapperImpl();

  // Returns the ContentDecryptorDelegate* associated with this plugin.
  virtual ContentDecryptorDelegate* GetCdmDelegate() OVERRIDE;

 private:
  typedef scoped_ptr<blink::WebHelperPlugin, WebHelperPluginDeleter>
      ScopedHelperPlugin;

  // Takes ownership of |helper_plugin| and |plugin_instance|.
  PepperCdmWrapperImpl(
      ScopedHelperPlugin helper_plugin,
      const scoped_refptr<PepperPluginInstanceImpl>& plugin_instance);

  ScopedHelperPlugin helper_plugin_;
  scoped_refptr<PepperPluginInstanceImpl> plugin_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperCdmWrapperImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_PEPPER_CDM_WRAPPER_IMPL_H_
