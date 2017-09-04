// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the child process (i.e. [Renderer, Plugin, Worker]ProcessHost), and
// dispatches them to URLRequests. It then forwards the messages from the
// URLRequests back to the correct process for handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_DISPATCHER_HOST_IMPL_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_DISPATCHER_HOST_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/browser/loader/global_routing_id.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/common/content_export.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "ipc/ipc_message.h"
#include "net/base/load_states.h"
#include "net/base/request_priority.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class RepeatingTimer;
}

namespace net {
class URLRequest;
class HttpRequestHeaders;
}

namespace storage {
class ShareableFileReference;
}

namespace content {
class AppCacheService;
class AsyncRevalidationManager;
class LoaderDelegate;
class NavigationURLLoaderImplCore;
class NavigationUIData;
class RenderFrameHostImpl;
class ResourceContext;
class ResourceDispatcherHostDelegate;
class ResourceLoader;
class ResourceHandler;
class ResourceMessageDelegate;
class ResourceMessageFilter;
class ResourceRequestInfoImpl;
class ResourceScheduler;
class ServiceWorkerNavigationHandleCore;
struct NavigationRequestInfo;
struct Referrer;
struct ResourceRequest;

using CreateDownloadHandlerIntercept =
    base::Callback<std::unique_ptr<ResourceHandler>(net::URLRequest*)>;

class CONTENT_EXPORT ResourceDispatcherHostImpl
    : public ResourceDispatcherHost,
      public ResourceLoaderDelegate {
 public:
  // Used to handle the result of SyncLoad IPC. |result| is null if it's
  // unavailable due to an error.
  using SyncLoadResultCallback =
      base::Callback<void(const SyncLoadResult* result)>;

  // This constructor should be used if we want downloads to work correctly.
  // TODO(ananta)
  // Work on moving creation of download handlers out of
  // ResourceDispatcherHostImpl.
  ResourceDispatcherHostImpl(
      CreateDownloadHandlerIntercept download_handler_intercept);
  ResourceDispatcherHostImpl();
  ~ResourceDispatcherHostImpl() override;

  // Returns the current ResourceDispatcherHostImpl. May return NULL if it
  // hasn't been created yet.
  static ResourceDispatcherHostImpl* Get();

  // The following static methods should all be called from the UI thread.

  // Resumes requests for a given render frame routing id. This will only resume
  // requests for a single frame.
  static void ResumeBlockedRequestsForRouteFromUI(
      const GlobalFrameRoutingId& global_routing_id);

  // Blocks (and does not start) all requests for the frame and its subframes.
  static void BlockRequestsForFrameFromUI(RenderFrameHost* root_frame_host);

  // Resumes any blocked requests for the specified frame and its subframes.
  static void ResumeBlockedRequestsForFrameFromUI(
      RenderFrameHost* root_frame_host);

  // Cancels any blocked request for the frame and its subframes.
  static void CancelBlockedRequestsForFrameFromUI(
      RenderFrameHostImpl* root_frame_host);

  // ResourceDispatcherHost implementation:
  void SetDelegate(ResourceDispatcherHostDelegate* delegate) override;
  void SetAllowCrossOriginAuthPrompt(bool value) override;
  void ClearLoginDelegateForRequest(net::URLRequest* request) override;
  void RegisterInterceptor(const std::string& http_header,
                           const std::string& starts_with,
                           const InterceptorCallback& interceptor) override;

  // Puts the resource dispatcher host in an inactive state (unable to begin
  // new requests).  Cancels all pending requests.
  void Shutdown();

  // Force cancels any pending requests for the given |context|. This is
  // necessary to ensure that before |context| goes away, all requests
  // for it are dead.
  void CancelRequestsForContext(ResourceContext* context);

  // Returns true if the message was a resource message that was processed.
  bool OnMessageReceived(const IPC::Message& message,
                         ResourceMessageFilter* filter);

  // Cancels the given request if it still exists.
  void CancelRequest(int child_id, int request_id);

  // Marks the request, with its current |response|, as "parked". This
  // happens if a request is redirected cross-site and needs to be
  // resumed by a new process.
  void MarkAsTransferredNavigation(
      const GlobalRequestID& id,
      const base::Closure& on_transfer_complete_callback);

  // Cancels a request previously marked as being transferred, for use when a
  // navigation was cancelled.
  void CancelTransferringNavigation(const GlobalRequestID& id);

  // Resumes the request without transferring it to a new process.
  void ResumeDeferredNavigation(const GlobalRequestID& id);

  // Returns the number of pending requests. This is designed for the unittests
  int pending_requests() const {
    return static_cast<int>(pending_loaders_.size());
  }

  // Intended for unit-tests only. Overrides the outstanding requests bound.
  void set_max_outstanding_requests_cost_per_process(int limit) {
    max_outstanding_requests_cost_per_process_ = limit;
  }
  void set_max_num_in_flight_requests_per_process(int limit) {
    max_num_in_flight_requests_per_process_ = limit;
  }
  void set_max_num_in_flight_requests(int limit) {
    max_num_in_flight_requests_ = limit;
  }

  // The average private bytes increase of the browser for each new pending
  // request. Experimentally obtained.
  static const int kAvgBytesPerOutstandingRequest = 4400;

  // Called when a RenderViewHost is created.
  void OnRenderViewHostCreated(int child_id,
                               int route_id);

  // Called when a RenderViewHost is deleted.
  void OnRenderViewHostDeleted(int child_id, int route_id);

  // Called when a RenderViewHost starts or stops loading.
  void OnRenderViewHostSetIsLoading(int child_id,
                                    int route_id,
                                    bool is_loading);

  // Force cancels any pending requests for the given process.
  void CancelRequestsForProcess(int child_id);

  void OnUserGesture();

  // Retrieves a net::URLRequest.  Must be called from the IO thread.
  net::URLRequest* GetURLRequest(const GlobalRequestID& request_id);

  void RemovePendingRequest(int child_id, int request_id);

  // Causes all new requests for the route identified by |routing_id| to be
  // blocked (not being started) until ResumeBlockedRequestsForRoute is called.
  void BlockRequestsForRoute(const GlobalFrameRoutingId& global_routing_id);

  // Resumes any blocked request for the specified route id.
  void ResumeBlockedRequestsForRoute(
      const GlobalFrameRoutingId& global_routing_id);

  // Cancels any blocked request for the specified route id.
  void CancelBlockedRequestsForRoute(
      const GlobalFrameRoutingId& global_routing_id);

  // Maintains a collection of temp files created in support of
  // the download_to_file capability. Used to grant access to the
  // child process and to defer deletion of the file until it's
  // no longer needed.
  void RegisterDownloadedTempFile(
      int child_id, int request_id,
      const base::FilePath& file_path);
  void UnregisterDownloadedTempFile(int child_id, int request_id);

  // Needed for the sync IPC message dispatcher macros.
  bool Send(IPC::Message* message);

  // Indicates whether third-party sub-content can pop-up HTTP basic auth
  // dialog boxes.
  bool allow_cross_origin_auth_prompt();

  ResourceDispatcherHostDelegate* delegate() {
    return delegate_;
  }

  // Must be called after the ResourceRequestInfo has been created
  // and associated with the request.
  // This is marked virtual so it can be overriden in testing.
  // TODO(ananta)
  // This method should be removed or moved outside this class.
  virtual std::unique_ptr<ResourceHandler> CreateResourceHandlerForDownload(
      net::URLRequest* request,
      bool is_content_initiated,
      bool must_download,
      bool is_new_request);

  // Called to determine whether the response to |request| should be intercepted
  // and handled as a stream. Streams are used to pass direct access to a
  // resource response to another application (e.g. a web page) without being
  // handled by the browser itself. If the request should be intercepted as a
  // stream, a StreamResourceHandler is returned which provides access to the
  // response. |plugin_path| is the path to the plugin which is handling the
  // URL request. This may be empty if there is no plugin handling the request.
  //
  // This function must be called after the ResourceRequestInfo has been created
  // and associated with the request. If |payload| is set to a non-empty value,
  // the caller must send it to the old resource handler instead of cancelling
  // it.
  virtual std::unique_ptr<ResourceHandler> MaybeInterceptAsStream(
      const base::FilePath& plugin_path,
      net::URLRequest* request,
      ResourceResponse* response,
      std::string* payload);

  ResourceScheduler* scheduler() { return scheduler_.get(); }

  // Called by a ResourceHandler when it's ready to start reading data and
  // sending it to the renderer. Returns true if there are enough file
  // descriptors available for the shared memory buffer. If false is returned,
  // the request should cancel.
  bool HasSufficientResourcesForRequest(net::URLRequest* request);

  // Called by a ResourceHandler after it has finished its request and is done
  // using its shared memory buffer. Frees up that file descriptor to be used
  // elsewhere.
  void FinishedWithResourcesForRequest(net::URLRequest* request);

  // PlzNavigate: Begins a request for NavigationURLLoader. |loader| is the
  // loader to attach to the leaf resource handler.
  void BeginNavigationRequest(
      ResourceContext* resource_context,
      const NavigationRequestInfo& info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      NavigationURLLoaderImplCore* loader,
      ServiceWorkerNavigationHandleCore* service_worker_handle_core);

  int num_in_flight_requests_for_testing() const {
    return num_in_flight_requests_;
  }

  // Turns on stale-while-revalidate support, regardless of command-line flags
  // or experiment status. For unit tests only.
  void EnableStaleWhileRevalidateForTesting();

  // Sets the LoaderDelegate, which must outlive this object. Ownership is not
  // transferred. The LoaderDelegate should be interacted with on the IO thread.
  void SetLoaderDelegate(LoaderDelegate* loader_delegate);

  void OnRenderFrameDeleted(const GlobalFrameRoutingId& global_routing_id);

  // Called when loading a request with mojo.
  void OnRequestResourceWithMojo(
      int routing_id,
      int request_id,
      const ResourceRequest& request,
      mojom::URLLoaderAssociatedRequest mojo_request,
      mojom::URLLoaderClientAssociatedPtr url_loader_client,
      ResourceMessageFilter* filter);

  void OnSyncLoadWithMojo(int routing_id,
                          int request_id,
                          const ResourceRequest& request_data,
                          ResourceMessageFilter* filter,
                          const SyncLoadResultCallback& result_handler);

  // Helper function for initializing the |request| passed in. By initializing
  // we mean setting the |referrer| on the |request|, associating the
  // ResourceRequestInfoImpl structure with the |request|, etc.
  // This function should be called for invoking the BeginURLRequest() function
  // to initiate a URL request.
  void InitializeURLRequest(net::URLRequest* request,
                            const Referrer& referrer,
                            bool is_download,
                            int render_process_host_id,
                            int render_view_routing_id,
                            int render_frame_routing_id,
                            ResourceContext* context);

  // Helper function for initiating a URL request. The |is_download| and
  // |is_content_initiated and |do_not_prompt_for_login| parameters are
  // specific to download requests.
  // TODO(ananta)
  // Look into a better way of passing these parameters in.
  // Please note that the InitializeURLRequest() function needs to be called
  // called to initialize the request before calling this function.
  void BeginURLRequest(std::unique_ptr<net::URLRequest> request,
                       std::unique_ptr<ResourceHandler> handler,
                       bool is_download,
                       bool is_content_initiated,
                       bool do_not_prompt_for_login,
                       ResourceContext* context);

  bool is_shutdown() const { return is_shutdown_; }

  // Creates a new request ID for browser initiated requests. See the comments
  // of |request_id_| for the details. Must be called on the IO thread.
  int MakeRequestID();

  // Cancels a request as requested by a renderer. This function is called when
  // a mojo connection is lost.
  // Note that this cancel is subtly different from the other CancelRequest
  // methods in this file, which also tear down the loader.
  void CancelRequestFromRenderer(GlobalRequestID request_id);

 private:
  friend class ResourceDispatcherHostTest;

  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           TestBlockedRequestsProcessDies);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           CalculateApproximateMemoryCost);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           DetachableResourceTimesOut);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           TestProcessCancelDetachableTimesOut);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessIgnoreCertErrorsBrowserTest,
                           CrossSiteRedirectCertificateStore);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest, LoadInfo);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest, LoadInfoSamePriority);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest, LoadInfoUploadProgress);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest, LoadInfoTwoRenderViews);

  struct OustandingRequestsStats {
    int memory_cost;
    int num_requests;
  };

  friend class ShutdownTask;
  friend class ResourceMessageDelegate;

  // Information about status of a ResourceLoader.
  struct CONTENT_EXPORT LoadInfo {
    LoadInfo();
    LoadInfo(const LoadInfo& other);
    ~LoadInfo();

    ResourceRequestInfo::WebContentsGetter web_contents_getter;
    GURL url;
    net::LoadStateWithParam load_state;
    uint64_t upload_position;
    uint64_t upload_size;
  };

  // Map from WebContents* to the "most interesting" LoadState.
  typedef std::map<WebContents*, LoadInfo> LoadInfoMap;
  typedef std::vector<LoadInfo> LoadInfoList;

  // Information about a HTTP header interceptor.
  struct HeaderInterceptorInfo {
    // Structure is sufficiently complicated to require the constructor
    // destructor definitions in the cc file.
    HeaderInterceptorInfo();
    ~HeaderInterceptorInfo();
    HeaderInterceptorInfo(const HeaderInterceptorInfo& other);

    // Used to prefix match the value of the http header.
    std::string starts_with;
    // The interceptor.
    InterceptorCallback interceptor;
  };

  // Map from HTTP header to its information HeaderInterceptorInfo.
  typedef std::map<std::string, HeaderInterceptorInfo> HeaderInterceptorMap;

  // ResourceLoaderDelegate implementation:
  ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) override;
  bool HandleExternalProtocol(ResourceLoader* loader, const GURL& url) override;
  void DidStartRequest(ResourceLoader* loader) override;
  void DidReceiveRedirect(ResourceLoader* loader, const GURL& new_url,
                          ResourceResponse* response) override;
  void DidReceiveResponse(ResourceLoader* loader) override;
  void DidFinishLoading(ResourceLoader* loader) override;
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      ResourceLoader* loader) override;

  // An init helper that runs on the IO thread.
  void OnInit();

  // A shutdown helper that runs on the IO thread.
  void OnShutdown();

  // Helper function for URL requests.
  void BeginRequestInternal(std::unique_ptr<net::URLRequest> request,
                            std::unique_ptr<ResourceHandler> handler);

  void StartLoading(ResourceRequestInfoImpl* info,
                    std::unique_ptr<ResourceLoader> loader);

  // We keep track of how much memory each request needs and how many requests
  // are issued by each renderer. These are known as OustandingRequestStats.
  // Memory limits apply to all requests sent to us by the renderers. There is a
  // limit for each renderer. File descriptor limits apply to requests that are
  // receiving their body. These are known as in-flight requests. There is a
  // global limit that applies for the browser process. Each render is allowed
  // to use up to a fraction of that.

  // Returns the OustandingRequestsStats for |info|'s renderer, or an empty
  // struct if that renderer has no outstanding requests.
  OustandingRequestsStats GetOutstandingRequestsStats(
      const ResourceRequestInfoImpl& info);

  // Updates |outstanding_requests_stats_map_| with the specified |stats| for
  // the renderer that made the request in |info|.
  void UpdateOutstandingRequestsStats(const ResourceRequestInfoImpl& info,
                                      const OustandingRequestsStats& stats);

  // Called every time an outstanding request is created or deleted. |count|
  // indicates whether the request is new or deleted. |count| must be 1 or -1.
  OustandingRequestsStats IncrementOutstandingRequestsMemory(
      int count,
      const ResourceRequestInfoImpl& info);

  // Called when an in flight request allocates or releases a shared memory
  // buffer. |count| indicates whether the request is issuing or finishing.
  // |count| must be 1 or -1.
  OustandingRequestsStats IncrementOutstandingRequestsCount(
      int count,
      ResourceRequestInfoImpl* info);

  // Estimate how much heap space |request| will consume to run.
  static int CalculateApproximateMemoryCost(net::URLRequest* request);

  // Force cancels any pending requests for the given route id.  This method
  // acts like CancelRequestsForProcess when the |route_id| member of
  // |routing_id| is MSG_ROUTING_NONE.
  void CancelRequestsForRoute(const GlobalFrameRoutingId& global_routing_id);

  // The list of all requests that we have pending. This list is not really
  // optimized, and assumes that we have relatively few requests pending at once
  // since some operations require brute-force searching of the list.
  //
  // It may be enhanced in the future to provide some kind of prioritization
  // mechanism. We should also consider a hashtable or binary tree if it turns
  // out we have a lot of things here.
  using LoaderMap = std::map<GlobalRequestID, std::unique_ptr<ResourceLoader>>;

  // Deletes the pending request identified by the iterator passed in.
  // This function will invalidate the iterator passed in. Callers should
  // not rely on this iterator being valid on return.
  void RemovePendingLoader(const LoaderMap::iterator& iter);

  // This function returns true if the LoadInfo of |a| is "more interesting"
  // than the LoadInfo of |b|.  The load that is currently sending the larger
  // request body is considered more interesting.  If neither request is
  // sending a body (Neither request has a body, or any request that has a body
  // is not currently sending the body), the request that is further along is
  // considered more interesting.
  //
  // This takes advantage of the fact that the load states are an enumeration
  // listed in the order in which they usually occur during the lifetime of a
  // request, so states with larger numeric values are generally further along
  // toward completion.
  //
  // For example, by this measure "tranferring data" is a more interesting state
  // than "resolving host" because when transferring data something is being
  // done that corresponds to changes that the user might observe, whereas
  // waiting for a host name to resolve implies being stuck.
  static bool LoadInfoIsMoreInteresting(const LoadInfo& a, const LoadInfo& b);

  // Picks the most interesting LoadInfos from the given array per WebContents
  // and calls the delegate with them.
  static void UpdateLoadStateOnUI(LoaderDelegate* loader_delegate,
                                  std::unique_ptr<LoadInfoList> infos);

  // Returns the most interesting LoadInfos from the given array per
  // WebContents.
  static std::unique_ptr<LoadInfoMap> PickMoreInterestingLoadInfos(
      std::unique_ptr<LoadInfoList> infos);

  // Gets all the LoadInfos for each pending request.
  std::unique_ptr<LoadInfoList> GetLoadInfoForAllRoutes();

  // Checks all pending requests and updates the load info if necessary.
  void UpdateLoadInfo();

  // Resumes or cancels (if |cancel_requests| is true) any blocked requests.
  void ProcessBlockedRequestsForRoute(
      const GlobalFrameRoutingId& global_routing_id,
      bool cancel_requests);

  void OnRequestResource(int routing_id,
                         int request_id,
                         const ResourceRequest& request_data);

  void OnRequestResourceInternal(
      int routing_id,
      int request_id,
      const ResourceRequest& request_data,
      mojom::URLLoaderAssociatedRequest mojo_request,
      mojom::URLLoaderClientAssociatedPtr url_loader_client);

  void OnSyncLoad(int request_id,
                  const ResourceRequest& request_data,
                  IPC::Message* sync_result);

  bool IsRequestIDInUse(const GlobalRequestID& id) const;

  // Update the ResourceRequestInfo and internal maps when a request is
  // transferred from one process to another.
  void UpdateRequestForTransfer(int child_id,
                                int route_id,
                                int request_id,
                                const ResourceRequest& request_data,
                                LoaderMap::iterator iter);

  // If |request_data| is for a request being transferred from another process,
  // then CompleteTransfer method can be used to complete the transfer.
  void CompleteTransfer(int request_id,
                        const ResourceRequest& request_data,
                        int route_id);

  void BeginRequest(
      int request_id,
      const ResourceRequest& request_data,
      const SyncLoadResultCallback& sync_result_handler,  // only valid for sync
      int route_id,
      mojom::URLLoaderAssociatedRequest mojo_request,
      mojom::URLLoaderClientAssociatedPtr url_loader_client);

  // There are requests which need decisions to be made like the following:
  // Whether the presence of certain HTTP headers like the Origin header are
  // valid, etc. These requests may need to be aborted based on these
  // decisions which could be time consuming. We allow for these decisions
  // to be made asynchronously. The request proceeds when we hear back from
  // the interceptors about whether to continue or not.
  // The |continue_request| parameter in the function indicates whether the
  // request should be continued or aborted. The |error_code| parameter is set
  // if |continue_request| is false.
  void ContinuePendingBeginRequest(
      int request_id,
      const ResourceRequest& request_data,
      const SyncLoadResultCallback& sync_result_handler,  // only valid for sync
      int route_id,
      const net::HttpRequestHeaders& headers,
      mojom::URLLoaderAssociatedRequest mojo_request,
      mojom::URLLoaderClientAssociatedPtr url_loader_client,
      bool continue_request,
      int error_code);

  // Creates a ResourceHandler to be used by BeginRequest() for normal resource
  // loading.
  std::unique_ptr<ResourceHandler> CreateResourceHandler(
      net::URLRequest* request,
      const ResourceRequest& request_data,
      const SyncLoadResultCallback& sync_result_handler,
      int route_id,
      int process_type,
      int child_id,
      ResourceContext* resource_context,
      mojom::URLLoaderAssociatedRequest mojo_request,
      mojom::URLLoaderClientAssociatedPtr url_loader_client);

  // Wraps |handler| in the standard resource handlers for normal resource
  // loading and navigation requests. This adds MimeTypeResourceHandler and
  // ResourceThrottles.
  std::unique_ptr<ResourceHandler> AddStandardHandlers(
      net::URLRequest* request,
      ResourceType resource_type,
      ResourceContext* resource_context,
      RequestContextType fetch_request_context_type,
      AppCacheService* appcache_service,
      int child_id,
      int route_id,
      std::unique_ptr<ResourceHandler> handler);

  void OnCancelRequest(int request_id);
  void OnReleaseDownloadedFile(int request_id);
  void OnDidChangePriority(int request_id,
                           net::RequestPriority new_priority,
                           int intra_priority_value);

  // Creates ResourceRequestInfoImpl for a download or page save.
  // |download| should be true if the request is a file download.
  ResourceRequestInfoImpl* CreateRequestInfo(
      int child_id,
      int render_view_route_id,
      int render_frame_route_id,
      bool download,
      ResourceContext* context);

  // Relationship of resource being authenticated with the top level page.
  enum HttpAuthRelationType {
    HTTP_AUTH_RELATION_TOP,            // Top-level page itself
    HTTP_AUTH_RELATION_SAME_DOMAIN,    // Sub-content from same domain
    HTTP_AUTH_RELATION_BLOCKED_CROSS,  // Blocked Sub-content from cross domain
    HTTP_AUTH_RELATION_ALLOWED_CROSS,  // Allowed Sub-content per command line
    HTTP_AUTH_RELATION_LAST
  };

  HttpAuthRelationType HttpAuthRelationTypeOf(const GURL& request_url,
                                              const GURL& first_party);

  // Returns whether the URLRequest identified by |transferred_request_id| is
  // currently in the process of being transferred to a different renderer.
  // This happens if a request is redirected cross-site and needs to be resumed
  // by a new process.
  bool IsTransferredNavigation(
      const GlobalRequestID& transferred_request_id) const;

  ResourceLoader* GetLoader(const GlobalRequestID& id) const;
  ResourceLoader* GetLoader(int child_id, int request_id) const;

  // Registers |delegate| to receive resource IPC messages targeted to the
  // specified |id|.
  void RegisterResourceMessageDelegate(const GlobalRequestID& id,
                                       ResourceMessageDelegate* delegate);
  void UnregisterResourceMessageDelegate(const GlobalRequestID& id,
                                         ResourceMessageDelegate* delegate);

  int BuildLoadFlagsForRequest(const ResourceRequest& request_data,
                               int child_id,
                               bool is_sync_load);

  // Consults the RendererSecurity policy to determine whether the
  // ResourceDispatcherHostImpl should service this request.  A request might
  // be disallowed if the renderer is not authorized to retrieve the request
  // URL or if the renderer is attempting to upload an unauthorized file.
  bool ShouldServiceRequest(int process_type,
                            int child_id,
                            const ResourceRequest& request_data,
                            const net::HttpRequestHeaders& headers,
                            ResourceMessageFilter* filter,
                            ResourceContext* resource_context);

  // Notifies the ResourceDispatcherHostDelegate about a download having
  // started. The function returns the |handler| passed in, if the download
  // is not throttled. If the download is to be throttled (Decided by the
  // delegate) the function returns a ThrottlingResourceHandler to handle the
  // download.
  std::unique_ptr<ResourceHandler> HandleDownloadStarted(
      net::URLRequest* request,
      std::unique_ptr<ResourceHandler> handler,
      bool is_content_initiated,
      bool must_download,
      bool is_new_request);

  LoaderMap pending_loaders_;

  // Collection of temp files downloaded for child processes via
  // the download_to_file mechanism. We avoid deleting them until
  // the client no longer needs them.
  typedef std::map<int, scoped_refptr<storage::ShareableFileReference> >
      DeletableFilesMap;  // key is request id
  typedef std::map<int, DeletableFilesMap>
      RegisteredTempFiles;  // key is child process id
  RegisteredTempFiles registered_temp_files_;

  // A timer that periodically calls UpdateLoadInfo while pending_loaders_ is
  // not empty and at least one RenderViewHost is loading.
  std::unique_ptr<base::RepeatingTimer> update_load_states_timer_;

  // Request ID for browser initiated requests. request_ids generated by
  // child processes are counted up from 0, while browser created requests
  // start at -2 and go down from there. (We need to start at -2 because -1 is
  // used as a special value all over the resource_dispatcher_host for
  // uninitialized variables.) This way, we no longer have the unlikely (but
  // observed in the real world!) event where we have two requests with the same
  // request_id_.
  int request_id_;

  // True if the resource dispatcher host has been shut down.
  bool is_shutdown_;

  using BlockedLoadersList = std::vector<std::unique_ptr<ResourceLoader>>;
  using BlockedLoadersMap =
      std::map<GlobalFrameRoutingId, std::unique_ptr<BlockedLoadersList>>;
  BlockedLoadersMap blocked_loaders_map_;

  // Maps the child_ids to the approximate number of bytes
  // being used to service its resource requests. No entry implies 0 cost.
  typedef std::map<int, OustandingRequestsStats> OutstandingRequestsStatsMap;
  OutstandingRequestsStatsMap outstanding_requests_stats_map_;

  // |num_in_flight_requests_| is the total number of requests currently issued
  // summed across all renderers.
  int num_in_flight_requests_;

  // |max_num_in_flight_requests_| is the upper bound on how many requests
  // can be in flight at once. It's based on the maximum number of file
  // descriptors open per process. We need a global limit for the browser
  // process.
  int max_num_in_flight_requests_;

  // |max_num_in_flight_requests_| is the upper bound on how many requests
  // can be issued at once. It's based on the maximum number of file
  // descriptors open per process. We need a per-renderer limit so that no
  // single renderer can hog the browser's limit.
  int max_num_in_flight_requests_per_process_;

  // |max_outstanding_requests_cost_per_process_| is the upper bound on how
  // many outstanding requests can be issued per child process host.
  // The constraint is expressed in terms of bytes (where the cost of
  // individual requests is given by CalculateApproximateMemoryCost).
  // The total number of outstanding requests is roughly:
  //   (max_outstanding_requests_cost_per_process_ /
  //       kAvgBytesPerOutstandingRequest)
  int max_outstanding_requests_cost_per_process_;

  // Time of the last user gesture. Stored so that we can add a load
  // flag to requests occurring soon after a gesture to indicate they
  // may be because of explicit user action.
  base::TimeTicks last_user_gesture_time_;

  // Used during IPC message dispatching so that the handlers can get a pointer
  // to the source of the message.
  ResourceMessageFilter* filter_;

  ResourceDispatcherHostDelegate* delegate_;

  LoaderDelegate* loader_delegate_;

  bool allow_cross_origin_auth_prompt_;

  // AsyncRevalidationManager is non-NULL if and only if
  // stale-while-revalidate is enabled.
  std::unique_ptr<AsyncRevalidationManager> async_revalidation_manager_;

  typedef std::map<GlobalRequestID,
                   base::ObserverList<ResourceMessageDelegate>*> DelegateMap;
  DelegateMap delegate_map_;

  std::unique_ptr<ResourceScheduler> scheduler_;

  // Used to invoke an interceptor for the HTTP header.
  HeaderInterceptorMap http_header_interceptor_map_;

  // Points to the registered download handler intercept.
  CreateDownloadHandlerIntercept create_download_handler_intercept_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatcherHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_DISPATCHER_HOST_IMPL_H_
