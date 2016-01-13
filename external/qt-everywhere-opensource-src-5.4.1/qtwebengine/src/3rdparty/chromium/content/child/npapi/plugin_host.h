// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Need mechanism to cleanup the static instance

#ifndef CONTENT_CHILD_NPAPI_PLUGIN_HOST_H_
#define CONTENT_CHILD_NPAPI_PLUGIN_HOST_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace content {

// The Plugin Host implements the NPN_xxx functions for NPAPI plugins.
// These are the functions exposed from the Plugin Host for use
// by the Plugin.
//
// The PluginHost is managed as a singleton.  This isn't strictly
// necessary, but since the callback functions are all global C
// functions, there is really no point in having per-instance PluginHosts.
class PluginHost : public base::RefCounted<PluginHost> {
 public:
  // Access the single PluginHost instance.  Callers
  // must call deref() when finished with the object.
  static PluginHost* Singleton();

  // The table of functions provided to the plugin.
  NPNetscapeFuncs* host_functions() { return &host_funcs_; }

  // Helper function for parsing post headers, and applying attributes
  // to the stream.  NPAPI post data include headers + data combined.
  // This function parses it out and adds it to the stream in a WebKit
  // style.
  static bool SetPostData(const char *buf,
                          uint32 length,
                          std::vector<std::string>* names,
                          std::vector<std::string>* values,
                          std::vector<char>* body);

  void PatchNPNetscapeFuncs(NPNetscapeFuncs* overrides);

 private:
  friend class base::RefCounted<PluginHost>;

  virtual ~PluginHost();

  PluginHost();
  void InitializeHostFuncs();
  NPNetscapeFuncs host_funcs_;
  DISALLOW_COPY_AND_ASSIGN(PluginHost);
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_PLUGIN_HOST_H_
