// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/common/content_client.h"
#include "content/public/common/socket_permission_request.h"
#include "content/public/common/window_container_type.h"
#include "net/base/mime_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"
#include "third_party/WebKit/public/web/WebNotificationPresenter.h"
#include "ui/base/window_open_disposition.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/common/resource_type.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_POSIX)
#include "content/public/browser/file_descriptor_info.h"
#endif

class GURL;
struct WebPreferences;

namespace base {
class CommandLine;
class DictionaryValue;
class FilePath;
}

namespace blink {
struct WebWindowFeatures;
}

namespace gfx {
class GLShareGroup;
class ImageSkia;
}

namespace net {
class CookieOptions;
class CookieStore;
class HttpNetworkSession;
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

namespace fileapi {
class ExternalMountPoints;
class FileSystemBackend;
}

namespace content {

class AccessTokenStore;
class BrowserChildProcessHost;
class BrowserContext;
class BrowserMainParts;
class BrowserPluginGuestDelegate;
class BrowserPpapiHost;
class BrowserURLHandler;
class DesktopNotificationDelegate;
class DevToolsManagerDelegate;
class ExternalVideoSurfaceContainer;
class LocationProvider;
class MediaObserver;
class QuotaPermissionContext;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHost;
class ResourceContext;
class SiteInstance;
class SpeechRecognitionManagerDelegate;
class VibrationProvider;
class WebContents;
class WebContentsViewDelegate;
struct MainFunctionParams;
struct Referrer;
struct ShowDesktopNotificationHostMsgParams;

// A mapping from the scheme name to the protocol handler that services its
// content.
typedef std::map<
  std::string, linked_ptr<net::URLRequestJobFactory::ProtocolHandler> >
    ProtocolHandlerMap;

// A scoped vector of protocol interceptors.
typedef ScopedVector<net::URLRequestInterceptor>
    URLRequestInterceptorScopedVector;

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

  // If content creates the WebContentsView implementation, it will ask the
  // embedder to return an (optional) delegate to customize it. The view will
  // own the delegate.
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents);

  // Notifies that a guest WebContents has been created. A guest WebContents
  // represents a renderer that's hosted within a BrowserPlugin. Creation can
  // occur an arbitrary length of time before attachment. If the new guest has
  // an |opener_web_contents|, then it's a new window created by that opener.
  // If the guest was created via navigation, then |extra_params| will be
  // non-NULL. |extra_params| are parameters passed to the BrowserPlugin object
  // element by the content embedder. These parameters may include the API to
  // enable for the given guest. |guest_delegate| is a return parameter of
  // the delegate in the content embedder that will service the guest in the
  // content layer. The content layer takes ownership of the |guest_delegate|.
  virtual void GuestWebContentsCreated(
      int guest_instance_id,
      SiteInstance* guest_site_instance,
      WebContents* guest_web_contents,
      WebContents* opener_web_contents,
      BrowserPluginGuestDelegate** guest_delegate,
      scoped_ptr<base::DictionaryValue> extra_params) {}

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

  // Returns a list additional WebUI schemes, if any.  These additional schemes
  // act as aliases to the chrome: scheme.  The additional schemes may or may
  // not serve specific WebUI pages depending on the particular URLDataSource
  // and its override of URLDataSource::ShouldServiceRequest. For all schemes
  // returned here, view-source is allowed.
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) {}

  // Returns a list of webUI hosts to ignore the storage partition check in
  // URLRequestChromeJob::CheckStoragePartitionMatches.
  virtual void GetAdditionalWebUIHostsToIgnoreParititionCheck(
      std::vector<std::string>* hosts) {}

  // Creates the main net::URLRequestContextGetter. Should only be called once
  // per ContentBrowserClient object.
  // TODO(ajwong): Remove once http://crbug.com/159193 is resolved.
  virtual net::URLRequestContextGetter* CreateRequestContext(
      BrowserContext* browser_context,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);

  // Creates the net::URLRequestContextGetter for a StoragePartition. Should
  // only be called once per partition_path per ContentBrowserClient object.
  // TODO(ajwong): Remove once http://crbug.com/159193 is resolved.
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);

  // Returns whether a specified URL is handled by the embedder's internal
  // protocol handlers.
  virtual bool IsHandledURL(const GURL& url);

  // Returns whether the given process is allowed to commit |url|.  This is a
  // more conservative check than IsSuitableHost, since it is used after a
  // navigation has committed to ensure that the process did not exceed its
  // authority.
  virtual bool CanCommitURL(RenderProcessHost* process_host, const GURL& url);

  // Returns whether a URL should be allowed to open from a specific context.
  // This also applies in cases where the new URL will open in another process.
  virtual bool ShouldAllowOpenURL(SiteInstance* site_instance, const GURL& url);

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

  // Called when a worker process is created.
  virtual void WorkerProcessCreated(SiteInstance* site_instance,
                                    int worker_process_id) {}

  // Called when a worker process is terminated.
  virtual void WorkerProcessTerminated(SiteInstance* site_instance,
                                       int worker_process_id) {}

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
                              net::CookieOptions* options);

  // This is called on the IO thread.
  virtual bool AllowSaveLocalState(ResourceContext* context);

  // Allow the embedder to control if access to web database by a shared worker
  // is allowed. |render_frame| is a vector of pairs of
  // RenderProcessID/RenderFrameID of RenderFrame that are using this worker.
  // This is called on the IO thread.
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const base::string16& name,
      const base::string16& display_name,
      unsigned long estimated_size,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames);

  // Allow the embedder to control if access to file system by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerFileSystem(
      const GURL& url,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames);

  // Allow the embedder to control if access to IndexedDB by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const base::string16& name,
      ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames);

  // Allow the embedder to override the request context based on the URL for
  // certain operations, like cookie access. Returns NULL to indicate the
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

  // Informs the embedder that a certificate error has occured.  If
  // |overridable| is true and if |strict_enforcement| is false, the user
  // can ignore the error and continue. The embedder can call the callback
  // asynchronously. If |result| is not set to
  // CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE, the request will be cancelled
  // or denied immediately, and the callback won't be run.
  virtual void AllowCertificateError(
      int render_process_id,
      int render_frame_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      ResourceType::Type resource_type,
      bool overridable,
      bool strict_enforcement,
      const base::Callback<void(bool)>& callback,
      CertificateRequestResultType* result) {}

  // Selects a SSL client certificate and returns it to the |callback|. If no
  // certificate was selected NULL is returned to the |callback|.
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_frame_id,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) {}

  // Adds a new installable certificate or private key.
  // Typically used to install an X.509 user certificate.
  // Note that it's up to the embedder to verify that the data is
  // well-formed. |cert_data| will be NULL if |cert_size| is 0.
  virtual void AddCertificate(net::CertificateMimeType cert_type,
                              const void* cert_data,
                              size_t cert_size,
                              int render_process_id,
                              int render_frame_id) {}

  // Returns a class to get notifications about media event. The embedder can
  // return NULL if they're not interested.
  virtual MediaObserver* GetMediaObserver();

  // Asks permission to show desktop notifications. |callback| needs to be run
  // when the user approves the request.
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      RenderFrameHost* render_frame_host,
      const base::Closure& callback) {}

  // Checks if the given page has permission to show desktop notifications.
  // This is called on the IO thread.
  virtual blink::WebNotificationPresenter::Permission
      CheckDesktopNotificationPermission(
          const GURL& source_url,
          ResourceContext* context,
          int render_process_id);

  // Show a desktop notification. If |cancel_callback| is non-null, it's set to
  // a callback which can be used to cancel the notification.
  virtual void ShowDesktopNotification(
      const ShowDesktopNotificationHostMsgParams& params,
      RenderFrameHost* render_frame_host,
      DesktopNotificationDelegate* delegate,
      base::Closure* cancel_callback) {}

  // The renderer is requesting permission to use Geolocation. When the answer
  // to a permission request has been determined, |result_callback| should be
  // called with the result. If |cancel_callback| is non-null, it's set to a
  // callback which can be used to cancel the permission request.
  virtual void RequestGeolocationPermission(
      WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback);

  // Requests a permission to use system exclusive messages in MIDI events.
  // |result_callback| will be invoked when the request is resolved. If
  // |cancel_callback| is non-null, it's set to a callback which can be used to
  // cancel the permission request.
  virtual void RequestMidiSysExPermission(
      WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback);

  // Request permission to access protected media identifier. |result_callback
  // will tell whether it's permitted. If |cancel_callback| is non-null, it's
  // set to a callback which can be used to cancel the permission request.
  virtual void RequestProtectedMediaIdentifierPermission(
      WebContents* web_contents,
      const GURL& origin,
      base::Callback<void(bool)> result_callback,
      base::Closure* cancel_callback);

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
                               int opener_id,
                               bool* no_javascript_access);

  // Returns a title string to use in the task manager for a process host with
  // the given URL, or the empty string to fall back to the default logic.
  // This is called on the IO thread.
  virtual std::string GetWorkerProcessTitle(const GURL& url,
                                            ResourceContext* context);

  // Notifies the embedder that the ResourceDispatcherHost has been created.
  // This is when it can optionally add a delegate.
  virtual void ResourceDispatcherHostCreated() {}

  // Allows the embedder to return a delegate for the SpeechRecognitionManager.
  // The delegate will be owned by the manager. It's valid to return NULL.
  virtual SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate();

  // Getters for common objects.
  virtual net::NetLog* GetNetLog();

  // Creates a new AccessTokenStore for gelocation.
  virtual AccessTokenStore* CreateAccessTokenStore();

  // Returns true if fast shutdown is possible.
  virtual bool IsFastShutdownPossible();

  // Called by WebContents to override the WebKit preferences that are used by
  // the renderer. The content layer will add its own settings, and then it's up
  // to the embedder to update it if it wants.
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                                   const GURL& url,
                                   WebPreferences* prefs) {}

  // Inspector setting was changed and should be persisted.
  virtual void UpdateInspectorSetting(RenderViewHost* rvh,
                                      const std::string& key,
                                      const std::string& value) {}

  // Notifies that BrowserURLHandler has been created, so that the embedder can
  // optionally add their own handlers.
  virtual void BrowserURLHandlerCreated(BrowserURLHandler* handler) {}

  // Clears browser cache.
  virtual void ClearCache(RenderViewHost* rvh) {}

  // Clears browser cookies.
  virtual void ClearCookies(RenderViewHost* rvh) {}

  // Returns the default download directory.
  // This can be called on any thread.
  virtual base::FilePath GetDefaultDownloadDirectory();

  // Returns the default filename used in downloads when we have no idea what
  // else we should do with the file.
  virtual std::string GetDefaultDownloadName();

  // Notification that a pepper plugin has just been spawned. This allows the
  // embedder to add filters onto the host to implement interfaces.
  // This is called on the IO thread.
  virtual void DidCreatePpapiPlugin(BrowserPpapiHost* browser_host) {}

  // Gets the host for an external out-of-process plugin.
  virtual BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_child_id);

  // Returns true if the socket operation specified by |params| is allowed from
  // the given |browser_context| and |url|. If |params| is NULL, this method
  // checks the basic "socket" permission, which is for those operations that
  // don't require a specific socket permission rule.
  // |private_api| indicates whether this permission check is for the private
  // Pepper socket API or the public one.
  virtual bool AllowPepperSocketAPI(BrowserContext* browser_context,
                                    const GURL& url,
                                    bool private_api,
                                    const SocketPermissionRequest* params);

  // Returns an implementation of a file selecition policy. Can return NULL.
  virtual ui::SelectFilePolicy* CreateSelectFilePolicy(
      WebContents* web_contents);

  // Returns additional allowed scheme set which can access files in
  // FileSystem API.
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) {}

  // Returns auto mount handlers for URL requests for FileSystem APIs.
  virtual void GetURLRequestAutoMountHandlers(
      std::vector<fileapi::URLRequestAutoMountHandler>* handlers) {}

  // Returns additional file system backends for FileSystem API.
  // |browser_context| is needed in the additional FileSystemBackends.
  // It has mount points to create objects returned by additional
  // FileSystemBackends, and SpecialStoragePolicy for permission granting.
  virtual void GetAdditionalFileSystemBackends(
      BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      ScopedVector<fileapi::FileSystemBackend>* additional_backends) {}

  // Allows an embedder to return its own LocationProvider implementation.
  // Return NULL to use the default one for the platform to be created.
  // FYI: Used by an external project; please don't remove.
  // Contact Viatcheslav Ostapenko at sl.ostapenko@samsung.com for more
  // information.
  virtual LocationProvider* OverrideSystemLocationProvider();

  // Allows an embedder to return its own VibrationProvider implementation.
  // Return NULL to use the default one for the platform to be created.
  // FYI: Used by an external project; please don't remove.
  // Contact Viatcheslav Ostapenko at sl.ostapenko@samsung.com for more
  // information.
  virtual VibrationProvider* OverrideVibrationProvider();

  // Allow an embedder to provide a share group reimplementation to connect renderer
  // GL contexts with the root compositor.
  virtual gfx::GLShareGroup* GetInProcessGpuShareGroup() { return 0; }

  // Creates a new DevToolsManagerDelegate. The caller owns the returned value.
  // It's valid to return NULL.
  virtual DevToolsManagerDelegate* GetDevToolsManagerDelegate();

  // Returns true if plugin referred to by the url can use
  // pp::FileIO::RequestOSFileHandle.
  virtual bool IsPluginAllowedToCallRequestOSFileHandle(
      BrowserContext* browser_context,
      const GURL& url);

  // Returns true if dev channel APIs are available for plugins.
  virtual bool IsPluginAllowedToUseDevChannelAPIs(
      BrowserContext* browser_context,
      const GURL& url);

  // Returns a special cookie store to use for a given render process, or NULL
  // if the default cookie store should be used
  // This is called on the IO thread.
  virtual net::CookieStore* OverrideCookieStoreForRenderProcess(
      int render_process_id);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Populates |mappings| with all files that need to be mapped before launching
  // a child process.
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      std::vector<FileDescriptorInfo>* mappings) {}
#endif

#if defined(OS_WIN)
  // Returns the name of the dll that contains cursors and other resources.
  virtual const wchar_t* GetResourceDllName();

  // This is called on the PROCESS_LAUNCHER thread before the renderer process
  // is launched. It gives the embedder a chance to add loosen the sandbox
  // policy.
  virtual void PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                bool* success) {}
#endif

#if defined(VIDEO_HOLE)
  // Allows an embedder to provide its own ExternalVideoSurfaceContainer
  // implementation.  Return NULL to disable external surface video.
  virtual ExternalVideoSurfaceContainer*
  OverrideCreateExternalVideoSurfaceContainer(WebContents* web_contents);
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
