// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/content_client.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/mojo_application_info.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/socket_permission_request.h"
#include "content/public/common/window_container_type.h"
#include "media/audio/audio_manager.h"
#include "net/base/mime_util.h"
#include "net/cookies/canonical_cookie.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_POSIX)
#include "content/public/browser/file_descriptor_info.h"
#endif

class GURL;

namespace base {
class CommandLine;
class DictionaryValue;
class FilePath;
}

namespace blink {
struct WebWindowFeatures;
}

namespace gfx {
class ImageSkia;
}

namespace gl {
class GLShareGroup;
}

namespace media {
class CdmFactory;
}

namespace shell {
class InterfaceRegistry;
class ShellClient;
}

namespace net {
class CookieOptions;
class NetLog;
class SSLCertRequestInfo;
class SSLInfo;
class URLRequest;
class URLRequestContext;
class URLRequestContextGetter;
class X509Certificate;
}

namespace sandbox {
class TargetPolicy;
}

namespace ui {
class SelectFilePolicy;
}

namespace url {
class Origin;
}

namespace storage {
class ExternalMountPoints;
class FileSystemBackend;
class QuotaEvictionPolicy;
}

namespace content {

enum class PermissionType;
class AccessTokenStore;
class BrowserChildProcessHost;
class BrowserContext;
class BrowserMainParts;
class BrowserPluginGuestDelegate;
class BrowserPpapiHost;
class BrowserURLHandler;
class ClientCertificateDelegate;
class DevToolsManagerDelegate;
class ExternalVideoSurfaceContainer;
class GeolocationDelegate;
class LocationProvider;
class MediaObserver;
class NavigationHandle;
class PlatformNotificationService;
class PresentationServiceDelegate;
class QuotaPermissionContext;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHost;
class ResourceContext;
class SiteInstance;
class SpeechRecognitionManagerDelegate;
class TracingDelegate;
class VpnServiceProxy;
class WebContents;
class WebContentsViewDelegate;
struct MainFunctionParams;
struct OpenURLParams;
struct Referrer;
struct WebPreferences;

// Embedder API (or SPI) for participating in browser logic, to be implemented
// by the client of the content browser. See ChromeContentBrowserClient for the
// principal implementation. The methods are assumed to be called on the UI
// thread unless otherwise specified. Use this "escape hatch" sparingly, to
// avoid the embedder interface ballooning and becoming very specific to Chrome.
// (Often, the call out to the client can happen in a different part of the code
// that either already has a hook out to the embedder, or calls out to one of
// the observer interfaces.)
class CONTENT_EXPORT ContentBrowserClient {
 public:
  virtual ~ContentBrowserClient() {}

  // Allows the embedder to set any number of custom BrowserMainParts
  // implementations for the browser startup code. See comments in
  // browser_main_parts.h.
  virtual BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters);

  // Allows the embedder to change the default behavior of
  // BrowserThread::PostAfterStartupTask to better match whatever
  // definition of "startup" the embedder has in mind. This may be
  // called on any thread.
  // Note: see related BrowserThread::PostAfterStartupTask.
  virtual void PostAfterStartupTask(
      const tracked_objects::Location& from_here,
      const scoped_refptr<base::TaskRunner>& task_runner,
      const base::Closure& task);

  // Allows the embedder to indicate whether it considers startup to be
  // complete. May be called on any thread. This should be called on a one-off
  // basis; if you need to poll this function constantly, use the above
  // PostAfterStartupTask() API instead.
  virtual bool IsBrowserStartupComplete();

  // If content creates the WebContentsView implementation, it will ask the
  // embedder to return an (optional) delegate to customize it. The view will
  // own the delegate.
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents);

  // Notifies that a render process will be created. This is called before
  // the content layer adds its own BrowserMessageFilters, so that the
  // embedder's IPC filters have priority.
  virtual void RenderProcessWillLaunch(RenderProcessHost* host) {}

  // Notifies that a BrowserChildProcessHost has been created.
  virtual void BrowserChildProcessHostCreated(BrowserChildProcessHost* host) {}

  // Get the effective URL for the given actual URL, to allow an embedder to
  // group different url schemes in the same SiteInstance.
  virtual GURL GetEffectiveURL(BrowserContext* browser_context,
                               const GURL& url);

  // Returns whether all instances of the specified effective URL should be
  // rendered by the same process, rather than using process-per-site-instance.
  virtual bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                       const GURL& effective_url);

  // Returns true if site isolation should be enabled for |effective_site_url|.
  // This call allows the embedder to supplement the site isolation policy
  // enforced by the content layer.
  //
  // Will only be called if both of the following happen:
  //   1. The embedder asked to be consulted, by returning true from
  //      ContentClient::IsSupplementarySiteIsolationModeEnabled().
  //   2. The content layer didn't decide to isolate |effective_site_url|
  //      according to its internal policy (e.g. because of --site-per-process).
  virtual bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                               const GURL& effective_site_url);

  // Returns true unless the effective URL is part of a site that cannot live in
  // a process restricted to just that site.  This is only called if site
  // isolation is enabled for this URL, and is a bug workaround.
  //
  // TODO(nick): Remove this function once https://crbug.com/160576 is fixed,
  // and origin lock can be applied to all URLs.
  virtual bool ShouldLockToOrigin(BrowserContext* browser_context,
                                  const GURL& effective_url);

  // Returns a list additional WebUI schemes, if any.  These additional schemes
  // act as aliases to the chrome: scheme.  The additional schemes may or may
  // not serve specific WebUI pages depending on the particular URLDataSource
  // and its override of URLDataSource::ShouldServiceRequest. For all schemes
  // returned here, view-source is allowed.
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) {}

  // Called when WebUI objects are created to get aggregate usage data (i.e. is
  // chrome://downloads used more than chrome://bookmarks?). Only internal (e.g.
  // chrome://) URLs are logged. Returns whether the URL was actually logged.
  virtual bool LogWebUIUrl(const GURL& web_ui_url) const;

  // Returns whether a specified URL is handled by the embedder's internal
  // protocol handlers.
  virtual bool IsHandledURL(const GURL& url);

  // Returns whether the given process is allowed to commit |url|.  This is a
  // more conservative check than IsSuitableHost, since it is used after a
  // navigation has committed to ensure that the process did not exceed its
  // authority.
  // This is called on the UI thread.
  virtual bool CanCommitURL(RenderProcessHost* process_host, const GURL& url);

  // Returns true if no URL within |origin| is allowed to commit in the given
  // process.  Must return false if there exists at least one URL in |origin|
  // that is allowed to commit.
  // This is called on the IO thread.
  virtual bool IsIllegalOrigin(ResourceContext* resource_context,
                               int child_process_id,
                               const GURL& origin);

  // Returns whether a URL should be allowed to open from a specific context.
  // This also applies in cases where the new URL will open in another process.
  virtual bool ShouldAllowOpenURL(SiteInstance* site_instance, const GURL& url);

  // Allows the embedder to override OpenURLParams.
  virtual void OverrideOpenURLParams(SiteInstance* site_instance,
                                     OpenURLParams* params) {}

  // Returns whether a new view for a given |site_url| can be launched in a
  // given |process_host|.
  virtual bool IsSuitableHost(RenderProcessHost* process_host,
                              const GURL& site_url);

  // Returns whether a new view for a new site instance can be added to a
  // given |process_host|.
  virtual bool MayReuseHost(RenderProcessHost* process_host);

  // Returns whether a new process should be created or an existing one should
  // be reused based on the URL we want to load. This should return false,
  // unless there is a good reason otherwise.
  virtual bool ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context, const GURL& url);

  // Called when a site instance is first associated with a process.
  virtual void SiteInstanceGotProcess(SiteInstance* site_instance) {}

  // Called from a site instance's destructor.
  virtual void SiteInstanceDeleting(SiteInstance* site_instance) {}

  // Returns true if for the navigation from |current_url| to |new_url|
  // in |site_instance|, a new SiteInstance and BrowsingInstance should be
  // created (even if we are in a process model that doesn't usually swap.)
  // This forces a process swap and severs script connections with existing
  // tabs.
  virtual bool ShouldSwapBrowsingInstancesForNavigation(
      SiteInstance* site_instance,
      const GURL& current_url,
      const GURL& new_url);

  // Returns true if the given navigation redirect should cause a renderer
  // process swap.
  // This is called on the IO thread.
  virtual bool ShouldSwapProcessesForRedirect(ResourceContext* resource_context,
                                              const GURL& current_url,
                                              const GURL& new_url);

  // Returns true if the passed in URL should be assigned as the site of the
  // current SiteInstance, if it does not yet have a site.
  virtual bool ShouldAssignSiteForURL(const GURL& url);

  // See CharacterEncoding's comment.
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name);

  // Allows the embedder to pass extra command line flags.
  // switches::kProcessType will already be set at this point.
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) {}

  // Returns the locale used by the application.
  // This is called on the UI and IO threads.
  virtual std::string GetApplicationLocale();

  // Returns the languages used in the Accept-Languages HTTP header.
  // (Not called GetAcceptLanguages so it doesn't clash with win32).
  virtual std::string GetAcceptLangs(BrowserContext* context);

  // Returns the default favicon.  The callee doesn't own the given bitmap.
  virtual const gfx::ImageSkia* GetDefaultFavicon();

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the IO thread.
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             ResourceContext* context);

  // Allow the embedder to control if a Service Worker can be associated
  // with the given scope.
  // Invalid (-1) process/frame ids indicate this is for starting a service
  // worker, which is not necessarily associated with a particular frame.
  // This is called on the IO thread.
  virtual bool AllowServiceWorker(const GURL& scope,
                                  const GURL& first_party,
                                  content::ResourceContext* context,
                                  int render_process_id,
                                  int render_frame_id);

  virtual bool IsDataSaverEnabled(BrowserContext* context);

  // Allow the embedder to control if the given cookie can be read.
  // This is called on the IO thread.
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              ResourceContext* context,
                              int render_process_id,
                              int render_frame_id);

  // Allow the embedder to control if the given cookie can be set.
  // This is called on the IO thread.
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              ResourceContext* context,
                              int render_process_id,
                              int render_frame_id,
                              const net::CookieOptions& options);

  // This is called on the IO thread.
  virtual bool AllowSaveLocalState(ResourceContext* context);

  // Allow the embedder to control if access to file system by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual void AllowWorkerFileSystem(
      const GURL& url,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames,
      base::Callback<void(bool)> callback);

  // Allow the embedder to control if access to IndexedDB by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const base::string16& name,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames);

#if defined(ENABLE_WEBRTC)
  // Allow the embedder to control if WebRTC identities are allowed to be cached
  // and potentially reused for future requests (within the same origin).
  // This is called on the IO thread.
  virtual bool AllowWebRTCIdentityCache(const GURL& url,
                                        const GURL& first_party_url,
                                        ResourceContext* context);
#endif  // defined(ENABLE_WEBRTC)

  // Allow the embedder to control whether we can use <keygen>.
  virtual bool AllowKeygen(const GURL& url, content::ResourceContext* context);

  // Allow the embedder to control whether we can use Web Bluetooth.
  // TODO(crbug.com/589228): Replace this with a use of the permission system.
  enum class AllowWebBluetoothResult {
    ALLOW,
    BLOCK_POLICY,
    BLOCK_GLOBALLY_DISABLED,
  };
  virtual AllowWebBluetoothResult AllowWebBluetooth(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin);

  // Returns a blacklist of UUIDs that have restrictions when accessed
  // via Web Bluetooth. Parsed by BluetoothBlacklist::Add().
  //
  // The blacklist string must be a comma-separated list of UUID:exclusion
  // pairs. The pairs may be separated by whitespace. Pair components are
  // colon-separated and must not have whitespace around the colon.
  //
  // UUIDs are a string that BluetoothUUID can parse (See BluetoothUUID
  // constructor comment). Exclusion values are a single lower case character
  // string "e", "r", or "w" for EXCLUDE, EXCLUDE_READS, or EXCLUDE_WRITES.
  //
  // Example:
  // "1812:e, 00001800-0000-1000-8000-00805f9b34fb:w, ignored:1, alsoignored."
  virtual std::string GetWebBluetoothBlacklist();

  // Allow the embedder to override the request context based on the URL for
  // certain operations, like cookie access. Returns nullptr to indicate the
  // regular request context should be used.
  // This is called on the IO thread.
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, ResourceContext* context);

  // Allow the embedder to specify a string version of the storage partition
  // config with a site.
  virtual std::string GetStoragePartitionIdForSite(
      BrowserContext* browser_context,
      const GURL& site);

  // Allows the embedder to provide a validation check for |partition_id|s.
  // This domain of valid entries should match the range of outputs for
  // GetStoragePartitionIdForChildProcess().
  virtual bool IsValidStoragePartitionId(BrowserContext* browser_context,
                                         const std::string& partition_id);

  // Allows the embedder to provide a storage parititon configuration for a
  // site. A storage partition configuration includes a domain of the embedder's
  // choice, an optional name within that domain, and whether the partition is
  // in-memory only.
  //
  // If |can_be_default| is false, the caller is telling the embedder that the
  // |site| is known to not be in the default partition. This is useful in
  // some shutdown situations where the bookkeeping logic that maps sites to
  // their partition configuration are no longer valid.
  //
  // The |partition_domain| is [a-z]* UTF-8 string, specifying the domain in
  // which partitions live (similar to namespace). Within a domain, partitions
  // can be uniquely identified by the combination of |partition_name| and
  // |in_memory| values. When a partition is not to be persisted, the
  // |in_memory| value must be set to true.
  virtual void GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site,
      bool can_be_default,
      std::string* partition_domain,
      std::string* partition_name,
      bool* in_memory);

  // Create and return a new quota permission context.
  virtual QuotaPermissionContext* CreateQuotaPermissionContext();

  // Gives the embedder a chance to register a custom QuotaEvictionPolicy for
  // temporary storage.
  virtual std::unique_ptr<storage::QuotaEvictionPolicy>
  GetTemporaryStorageEvictionPolicy(BrowserContext* context);

  // Informs the embedder that a certificate error has occured.  If
  // |overridable| is true and if |strict_enforcement| is false, the user
  // can ignore the error and continue. The embedder can call the callback
  // asynchronously. If |result| is not set to
  // CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE, the request will be cancelled
  // or denied immediately, and the callback won't be run.
  virtual void AllowCertificateError(WebContents* web_contents,
                                     int cert_error,
                                     const net::SSLInfo& ssl_info,
                                     const GURL& request_url,
                                     ResourceType resource_type,
                                     bool overridable,
                                     bool strict_enforcement,
                                     bool expired_previous_decision,
                                     const base::Callback<void(bool)>& callback,
                                     CertificateRequestResultType* result) {}

  // Selects a SSL client certificate and returns it to the |delegate|. Note:
  // |delegate| may be called synchronously or asynchronously.
  //
  // TODO(davidben): Move this hook to WebContentsDelegate.
  virtual void SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<ClientCertificateDelegate> delegate);

  // Adds a new installable certificate or private key.
  // Typically used to install an X.509 user certificate.
  // Note that it's up to the embedder to verify that the data is
  // well-formed. |cert_data| will be nullptr if |cert_size| is 0.
  virtual void AddCertificate(net::CertificateMimeType cert_type,
                              const void* cert_data,
                              size_t cert_size,
                              int render_process_id,
                              int render_frame_id) {}

  // Returns a class to get notifications about media event. The embedder can
  // return nullptr if they're not interested.
  virtual MediaObserver* GetMediaObserver();

  // Returns the platform notification service, capable of displaying Web
  // Notifications to the user. The embedder can return a nullptr if they don't
  // support this functionality. May be called from any thread.
  virtual PlatformNotificationService* GetPlatformNotificationService();

  // Returns true if the given page is allowed to open a window of the given
  // type. If true is returned, |no_javascript_access| will indicate whether
  // the window that is created should be scriptable/in the same process.
  // This is called on the IO thread.
  virtual bool CanCreateWindow(const GURL& opener_url,
                               const GURL& opener_top_level_frame_url,
                               const GURL& source_origin,
                               WindowContainerType container_type,
                               const GURL& target_url,
                               const Referrer& referrer,
                               WindowOpenDisposition disposition,
                               const blink::WebWindowFeatures& features,
                               bool user_gesture,
                               bool opener_suppressed,
                               ResourceContext* context,
                               int render_process_id,
                               int opener_render_view_id,
                               int opener_render_frame_id,
                               bool* no_javascript_access);

  // Notifies the embedder that the ResourceDispatcherHost has been created.
  // This is when it can optionally add a delegate.
  virtual void ResourceDispatcherHostCreated() {}

  // Allows the embedder to return a delegate for the SpeechRecognitionManager.
  // The delegate will be owned by the manager. It's valid to return nullptr.
  virtual SpeechRecognitionManagerDelegate*
      CreateSpeechRecognitionManagerDelegate();

  // Getters for common objects.
  virtual net::NetLog* GetNetLog();

  // Allows the embedder to provide a Delegate for Geolocation to override some
  // functionality of the API (e.g. AccessTokenStore, LocationProvider).
  virtual GeolocationDelegate* CreateGeolocationDelegate();

  // Returns true if fast shutdown is possible.
  virtual bool IsFastShutdownPossible();

  // Called by WebContents to override the WebKit preferences that are used by
  // the renderer. The content layer will add its own settings, and then it's up
  // to the embedder to update it if it wants.
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                                   WebPreferences* prefs) {}

  // Notifies that BrowserURLHandler has been created, so that the embedder can
  // optionally add their own handlers.
  virtual void BrowserURLHandlerCreated(BrowserURLHandler* handler) {}

  // Clears browser cache.
  virtual void ClearCache(RenderFrameHost* rfh) {}

  // Clears browser cookies.
  virtual void ClearCookies(RenderFrameHost* rfh) {}

  // Returns the default download directory.
  // This can be called on any thread.
  virtual base::FilePath GetDefaultDownloadDirectory();

  // Returns the default filename used in downloads when we have no idea what
  // else we should do with the file.
  virtual std::string GetDefaultDownloadName();

  // Returns the path to the browser shader disk cache root.
  virtual base::FilePath GetShaderDiskCacheDirectory();

  // Notification that a pepper plugin has just been spawned. This allows the
  // embedder to add filters onto the host to implement interfaces.
  // This is called on the IO thread.
  virtual void DidCreatePpapiPlugin(BrowserPpapiHost* browser_host) {}

  // Gets the host for an external out-of-process plugin.
  virtual BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_child_id);

  // Returns true if the socket operation specified by |params| is allowed from
  // the given |browser_context| and |url|. If |params| is nullptr, this method
  // checks the basic "socket" permission, which is for those operations that
  // don't require a specific socket permission rule.
  // |private_api| indicates whether this permission check is for the private
  // Pepper socket API or the public one.
  virtual bool AllowPepperSocketAPI(BrowserContext* browser_context,
                                    const GURL& url,
                                    bool private_api,
                                    const SocketPermissionRequest* params);

  // Returns true if the "vpnProvider" permission is allowed from the given
  // |browser_context| and |url|.
  virtual bool IsPepperVpnProviderAPIAllowed(BrowserContext* browser_context,
                                             const GURL& url);

  // Creates a new VpnServiceProxy. The caller owns the returned value. It's
  // valid to return nullptr.
  virtual std::unique_ptr<VpnServiceProxy> GetVpnServiceProxy(
      BrowserContext* browser_context);

  // Returns an implementation of a file selecition policy. Can return nullptr.
  virtual ui::SelectFilePolicy* CreateSelectFilePolicy(
      WebContents* web_contents);

  // Returns additional allowed scheme set which can access files in
  // FileSystem API.
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) {}

  // |schemes| is a return value parameter that gets a whitelist of schemes that
  // should bypass the Is Privileged Context check.
  // See http://www.w3.org/TR/powerful-features/#settings-privileged
  virtual void GetSchemesBypassingSecureContextCheckWhitelist(
      std::set<std::string>* schemes) {}

  // Returns auto mount handlers for URL requests for FileSystem APIs.
  virtual void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) {}

  // Returns additional file system backends for FileSystem API.
  // |browser_context| is needed in the additional FileSystemBackends.
  // It has mount points to create objects returned by additional
  // FileSystemBackends, and SpecialStoragePolicy for permission granting.
  virtual void GetAdditionalFileSystemBackends(
      BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      ScopedVector<storage::FileSystemBackend>* additional_backends) {}

  // Allow an embedder to provide a share group reimplementation to connect renderer
  // GL contexts with the root compositor.
  virtual gl::GLShareGroup* GetInProcessGpuShareGroup() { return 0; }

  // Creates a new DevToolsManagerDelegate. The caller owns the returned value.
  // It's valid to return nullptr.
  virtual DevToolsManagerDelegate* GetDevToolsManagerDelegate();

  // Creates a new TracingDelegate. The caller owns the returned value.
  // It's valid to return nullptr.
  virtual TracingDelegate* GetTracingDelegate();

  // Returns true if plugin referred to by the url can use
  // pp::FileIO::RequestOSFileHandle.
  virtual bool IsPluginAllowedToCallRequestOSFileHandle(
      BrowserContext* browser_context,
      const GURL& url);

  // Returns true if dev channel APIs are available for plugins.
  virtual bool IsPluginAllowedToUseDevChannelAPIs(
      BrowserContext* browser_context,
      const GURL& url);

  // Generate a Shell user-id for the supplied browser context. Defaults to
  // returning a random GUID.
  virtual std::string GetShellUserIdForBrowserContext(
      BrowserContext* browser_context);

  // Allows to register browser Mojo interfaces exposed through the
  // RenderProcessHost.
  virtual void ExposeInterfacesToRenderer(
      shell::InterfaceRegistry* registry,
      RenderProcessHost* render_process_host) {}

  // Allows to register browser Mojo interfaces exposed through the
  // FrameMojoShell.
  virtual void RegisterFrameMojoShellInterfaces(
      shell::InterfaceRegistry* registry,
      RenderFrameHost* render_frame_host) {}

  // Allows to register browser Mojo interfaces exposed through the
  // RenderFrameHost.
  virtual void RegisterRenderFrameMojoInterfaces(
      shell::InterfaceRegistry* registry,
      RenderFrameHost* render_frame_host) {}

  using StaticMojoApplicationMap = std::map<std::string, MojoApplicationInfo>;

  // Registers Mojo applications to be loaded in the browser process by the
  // browser's global Mojo shell.
  virtual void RegisterInProcessMojoApplications(
      StaticMojoApplicationMap* apps) {}

  using OutOfProcessMojoApplicationMap = std::map<std::string, base::string16>;

  // Registers Mojo applications to be loaded out of the browser process, in a
  // sandboxed utility process. The value of each map entry should be the
  // process name to use for the application's host process when launched.
  virtual void RegisterOutOfProcessMojoApplications(
      OutOfProcessMojoApplicationMap* apps) {}

  // Registers Mojo applications to be loaded out of the browser process (in
  // a utility process) without the sandbox.
  //
  // WARNING: This path is NOT recommended! If a Mojo application needs a
  // service that is only available out of the sandbox, it could ask the browser
  // process to provide it (e.g. through OverrideFrameMojoShellServices()). Only
  // use this method when that approach does not work.
  virtual void RegisterUnsandboxedOutOfProcessMojoApplications(
      OutOfProcessMojoApplicationMap* apps) {}

  // A map of Mojo application names to corresponding manifest contents.
  using MojoApplicationManifestMap = std::map<std::string, std::string>;

  // Registers manifest contents for Mojo applications.
  // See "services/shell/manifest.json" for an example Mojo app manifest.
  //
  // TODO(rockot): http://crbug.com/610426. Add more documentations about
  //  Mojo app manifest.
  virtual void RegisterMojoApplicationManifests(
      MojoApplicationManifestMap* manifests) {}

  // Allows to override the visibility state of a RenderFrameHost.
  // |visibility_state| should not be null. It will only be set if needed.
  virtual void OverridePageVisibilityState(
      RenderFrameHost* render_frame_host,
      blink::WebPageVisibilityState* visibility_state) {}

  // Allows an embedder to provide its own PresentationServiceDelegate
  // implementation. Returns nullptr if unavailable.
  virtual PresentationServiceDelegate* GetPresentationServiceDelegate(
      WebContents* web_contents);

  // Allows programmatic opening of a new tab/window without going through
  // another WebContents. For example, from a Worker. |callback| will be
  // invoked with the appropriate WebContents* when available.
  virtual void OpenURL(BrowserContext* browser_context,
                       const OpenURLParams& params,
                       const base::Callback<void(WebContents*)>& callback);

  // Allows the embedder to record |metric| for a specific |url|.
  virtual void RecordURLMetric(const std::string& metric, const GURL& url) {}

  // Allows the embedder to register one or more NavigationThrottles for the
  // navigation indicated by |navigation_handle|.  A NavigationThrottle is used
  // to control the flow of a navigation on the UI thread. The embedder is
  // guaranteed that the throttles will be executed in the order they were
  // provided.
  virtual ScopedVector<NavigationThrottle> CreateThrottlesForNavigation(
      NavigationHandle* navigation_handle);

  // Allows the embedder to provide its own AudioManager implementation.
  // If this function returns nullptr, a default platform implementation
  // will be used.
  virtual media::ScopedAudioManagerPtr CreateAudioManager(
      media::AudioLogFactory* audio_log_factory);
  // Creates and returns a factory used for creating CDM instances for playing
  // protected content.
  virtual std::unique_ptr<media::CdmFactory> CreateCdmFactory();

  // Populates |mappings| with all files that need to be mapped before launching
  // a child process.
#if defined(OS_ANDROID)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings,
      std::map<int, base::MemoryMappedFile::Region>* regions) {}
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings) {}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  // Returns the name of the dll that contains cursors and other resources.
  virtual const wchar_t* GetResourceDllName();

  // This is called on the PROCESS_LAUNCHER thread before the renderer process
  // is launched. It gives the embedder a chance to add loosen the sandbox
  // policy.
  virtual bool PreSpawnRenderer(sandbox::TargetPolicy* policy);

  // Returns the AppContainer SID for the specified sandboxed process type, or
  // empty string if this sandboxed process type does not support living inside
  // an AppContainer.
  virtual base::string16 GetAppContainerSidForSandboxType(
      int sandbox_type) const;

  // Returns whether the Win32k lockdown process mitigation should be applied to
  // a process hosting a plugin with the specified |mime_type|.
  virtual bool IsWin32kLockdownEnabledForMimeType(
      const std::string& mime_type) const;
#endif

#if defined(VIDEO_HOLE)
  // Allows an embedder to provide its own ExternalVideoSurfaceContainer
  // implementation.  Return nullptr to disable external surface video.
  virtual ExternalVideoSurfaceContainer*
  OverrideCreateExternalVideoSurfaceContainer(WebContents* web_contents);
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
