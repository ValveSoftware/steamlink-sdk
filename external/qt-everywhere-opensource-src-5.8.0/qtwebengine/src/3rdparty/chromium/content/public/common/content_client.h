// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ui/base/layout.h"
#include "url/url_util.h"

class GURL;

namespace base {
class RefCountedMemory;
}

namespace IPC {
class Message;
}

namespace gfx {
class Image;
}

namespace gpu {
struct GPUInfo;
}

namespace media {
class MediaClientAndroid;
}

namespace sandbox {
class TargetPolicy;
}

namespace content {

class ContentBrowserClient;
class ContentClient;
class ContentGpuClient;
class ContentRendererClient;
class ContentUtilityClient;
class OriginTrialPolicy;
struct CdmInfo;
struct PepperPluginInfo;

// Setter and getter for the client.  The client should be set early, before any
// content code is called.
CONTENT_EXPORT void SetContentClient(ContentClient* client);

#if defined(CONTENT_IMPLEMENTATION)
// Content's embedder API should only be used by content.
ContentClient* GetContentClient();
#endif

// Used for tests to override the relevant embedder interfaces. Each method
// returns the old value.
CONTENT_EXPORT ContentBrowserClient* SetBrowserClientForTesting(
    ContentBrowserClient* b);
CONTENT_EXPORT ContentRendererClient* SetRendererClientForTesting(
    ContentRendererClient* r);
CONTENT_EXPORT ContentUtilityClient* SetUtilityClientForTesting(
    ContentUtilityClient* u);

// Interface that the embedder implements.
class CONTENT_EXPORT ContentClient {
 public:
  ContentClient();
  virtual ~ContentClient();

  ContentBrowserClient* browser() { return browser_; }
  ContentGpuClient* gpu() { return gpu_; }
  ContentRendererClient* renderer() { return renderer_; }
  ContentUtilityClient* utility() { return utility_; }

  // Sets the currently active URL.  Use GURL() to clear the URL.
  virtual void SetActiveURL(const GURL& url) {}

  // Sets the data on the current gpu.
  virtual void SetGpuInfo(const gpu::GPUInfo& gpu_info) {}

  // Gives the embedder a chance to register its own pepper plugins.
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) {}

  // Gives the embedder a chance to register the content decryption
  // modules it supports.
  virtual void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms) {}

  // Gives the embedder a chance to register its own standard, referrer and
  // saveable url schemes early on in the startup sequence.
  virtual void AddAdditionalSchemes(
      std::vector<url::SchemeWithType>* standard_schemes,
      std::vector<url::SchemeWithType>* referrer_schemes,
      std::vector<std::string>* savable_schemes) {}

  // Returns whether the given message should be sent in a swapped out renderer.
  virtual bool CanSendWhileSwappedOut(const IPC::Message* message);

  // Returns a string describing the embedder product name and version,
  // of the form "productname/version", with no other slashes.
  // Used as part of the user agent string.
  virtual std::string GetProduct() const;

  // Returns the user agent.  Content may cache this value.
  virtual std::string GetUserAgent() const;

  // Returns a string resource given its id.
  virtual base::string16 GetLocalizedString(int message_id) const;

  // Return the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const;

  // Returns the raw bytes of a scale independent data resource.
  virtual base::RefCountedMemory* GetDataResourceBytes(
      int resource_id) const;

  // Returns a native image given its id.
  virtual gfx::Image& GetNativeImageNamed(int resource_id) const;

  // Called by content::GetProcessTypeNameInEnglish for process types that it
  // doesn't know about because they're from the embedder.
  virtual std::string GetProcessTypeNameInEnglish(int type);

#if defined(OS_MACOSX)
  // Allows the embedder to define a new |sandbox_type| by mapping it to the
  // resource ID corresponding to the sandbox profile to use. The legal values
  // for |sandbox_type| are defined by the embedder and should start with
  // SandboxType::SANDBOX_TYPE_AFTER_LAST_TYPE. Returns false if no sandbox
  // profile for the given |sandbox_type| exists. Otherwise,
  // |sandbox_profile_resource_id| is set to the resource ID corresponding to
  // the sandbox profile to use and true is returned.
  virtual bool GetSandboxProfileForSandboxType(
      int sandbox_type,
      int* sandbox_profile_resource_id) const;
#endif

  // Gives the embedder a chance to register additional schemes and origins
  // that need to be considered trustworthy.
  // See https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
  virtual void AddSecureSchemesAndOrigins(std::set<std::string>* schemes,
                                          std::set<GURL>* origins) {}

  // Gives the embedder a chance to register additional schemes that
  // should be allowed to register service workers. Only secure and
  // trustworthy schemes should be added.
  virtual void AddServiceWorkerSchemes(std::set<std::string>* schemes) {}

  // Returns whether or not V8 script extensions should be allowed for a
  // service worker.
  virtual bool AllowScriptExtensionForServiceWorker(const GURL& script_url);

  // Returns true if the embedder wishes to supplement the site isolation policy
  // used by the content layer. Returning true enables the infrastructure for
  // out-of-process iframes, and causes the content layer to consult
  // ContentBrowserClient::DoesSiteRequireDedicatedProcess() when making process
  // model decisions.
  virtual bool IsSupplementarySiteIsolationModeEnabled();

  // Returns the origin trial policy, or nullptr if origin trials are not
  // supported by the embedder.
  virtual OriginTrialPolicy* GetOriginTrialPolicy();

#if defined(OS_ANDROID)
  // Returns true for clients like Android WebView that uses synchronous
  // compositor. Note setting this to true will permit synchronous IPCs from
  // the browser UI thread.
  virtual bool UsingSynchronousCompositing();

  // Returns the MediaClientAndroid to be used by media code on Android.
  virtual media::MediaClientAndroid* GetMediaClientAndroid();
#endif  // OS_ANDROID

 private:
  friend class ContentClientInitializer;  // To set these pointers.
  friend class InternalTestInitializer;

  // The embedder API for participating in browser logic.
  ContentBrowserClient* browser_;
  // The embedder API for participating in gpu logic.
  ContentGpuClient* gpu_;
  // The embedder API for participating in renderer logic.
  ContentRendererClient* renderer_;
  // The embedder API for participating in utility logic.
  ContentUtilityClient* utility_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
