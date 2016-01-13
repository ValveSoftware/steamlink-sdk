// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BINDINGS_H__
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BINDINGS_H__

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/shared_impl/resource.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace content {

class BrowserPlugin;
class BrowserPluginMethodBinding;
class BrowserPluginPropertyBinding;

class BrowserPluginBindings {
 public:
  // BrowserPluginNPObject is a simple struct that adds a pointer back to a
  // BrowserPluginBindings instance.  This way, we can use an NPObject to allow
  // JavaScript interactions without forcing BrowserPluginBindings to inherit
  // from NPObject.
  struct BrowserPluginNPObject : public NPObject {
    BrowserPluginNPObject();
    ~BrowserPluginNPObject();

    base::WeakPtr<BrowserPluginBindings> message_channel;
  };

  explicit BrowserPluginBindings(BrowserPlugin* instance);
  ~BrowserPluginBindings();

  NPObject* np_object() const { return np_object_; }

  BrowserPlugin* instance() const { return instance_; }

  bool HasMethod(NPIdentifier name) const;

  bool InvokeMethod(NPIdentifier name,
                    const NPVariant* args,
                    uint32 arg_count,
                    NPVariant* result);

  bool HasProperty(NPIdentifier name) const;
  bool SetProperty(NPObject* np_obj,
                   NPIdentifier name,
                   const NPVariant* variant);
  bool GetProperty(NPIdentifier name, NPVariant* result);
  bool RemoveProperty(NPObject *np_obj, NPIdentifier name);
 private:
  BrowserPlugin* instance_;
  // The NPObject we use to expose postMessage to JavaScript.
  BrowserPluginNPObject* np_object_;

  typedef ScopedVector<BrowserPluginMethodBinding> BindingList;
  BindingList method_bindings_;
  typedef ScopedVector<BrowserPluginPropertyBinding> PropertyBindingList;
  PropertyBindingList property_bindings_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<BrowserPluginBindings> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindings);
};

}  // namespace content

#endif  //  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BINDINGS_H__
