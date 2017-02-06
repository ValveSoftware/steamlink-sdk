// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"

#include "base/files/file_path.h"
#include "base/guid.h"
#include "build/build_config.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/geolocation_delegate.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/common/sandbox_type.h"
#include "media/base/cdm_factory.h"
#include "storage/browser/quota/quota_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace content {

BrowserMainParts* ContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return nullptr;
}

void ContentBrowserClient::PostAfterStartupTask(
    const tracked_objects::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Closure& task) {
  task_runner->PostTask(from_here, task);
}

bool ContentBrowserClient::IsBrowserStartupComplete() {
  return true;
}

WebContentsViewDelegate* ContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  return nullptr;
}

GURL ContentBrowserClient::GetEffectiveURL(BrowserContext* browser_context,
                                           const GURL& url) {
  return url;
}

bool ContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

bool ContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  return false;
}

bool ContentBrowserClient::ShouldLockToOrigin(BrowserContext* browser_context,
                                              const GURL& effective_url) {
  return true;
}

bool ContentBrowserClient::LogWebUIUrl(const GURL& web_ui_url) const {
  return false;
}

bool ContentBrowserClient::IsHandledURL(const GURL& url) {
  return false;
}

bool ContentBrowserClient::CanCommitURL(RenderProcessHost* process_host,
                                        const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::IsIllegalOrigin(ResourceContext* resource_context,
                                           int child_process_id,
                                           const GURL& origin) {
  return false;
}

bool ContentBrowserClient::ShouldAllowOpenURL(SiteInstance* site_instance,
                                              const GURL& url) {
  return true;
}

bool ContentBrowserClient::IsSuitableHost(RenderProcessHost* process_host,
                                          const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::MayReuseHost(RenderProcessHost* process_host) {
  return true;
}

bool ContentBrowserClient::ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context, const GURL& url) {
  return false;
}

bool ContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_url,
    const GURL& new_url) {
  return false;
}

media::ScopedAudioManagerPtr ContentBrowserClient::CreateAudioManager(
    media::AudioLogFactory* audio_log_factory) {
  return nullptr;
}

std::unique_ptr<media::CdmFactory> ContentBrowserClient::CreateCdmFactory() {
  return nullptr;
}

bool ContentBrowserClient::ShouldSwapProcessesForRedirect(
    ResourceContext* resource_context, const GURL& current_url,
    const GURL& new_url) {
  return false;
}

bool ContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return true;
}

std::string ContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return std::string();
}

std::string ContentBrowserClient::GetApplicationLocale() {
  return "en-US";
}

std::string ContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return std::string();
}

const gfx::ImageSkia* ContentBrowserClient::GetDefaultFavicon() {
  static gfx::ImageSkia* empty = new gfx::ImageSkia();
  return empty;
}

bool ContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                                         const GURL& first_party,
                                         ResourceContext* context) {
  return true;
}

bool ContentBrowserClient::AllowServiceWorker(const GURL& scope,
                                              const GURL& document_url,
                                              content::ResourceContext* context,
                                              int render_process_id,
                                              int render_frame_id) {
  return true;
}

bool ContentBrowserClient::IsDataSaverEnabled(BrowserContext* context) {
  return false;
}

bool ContentBrowserClient::AllowGetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const net::CookieList& cookie_list,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id) {
  return true;
}

bool ContentBrowserClient::AllowSetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const std::string& cookie_line,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id,
                                          const net::CookieOptions& options) {
  return true;
}

bool ContentBrowserClient::AllowSaveLocalState(ResourceContext* context) {
  return true;
}

void ContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames,
    base::Callback<void(bool)> callback) {
  callback.Run(true);
}

bool ContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const base::string16& name,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  return true;
}

#if defined(ENABLE_WEBRTC)
bool ContentBrowserClient::AllowWebRTCIdentityCache(const GURL& url,
                                                    const GURL& first_party_url,
                                                    ResourceContext* context) {
  return true;
}
#endif  // defined(ENABLE_WEBRTC)

bool ContentBrowserClient::AllowKeygen(const GURL& url,
                                       content::ResourceContext* context) {
  return true;
}

ContentBrowserClient::AllowWebBluetoothResult
ContentBrowserClient::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return AllowWebBluetoothResult::ALLOW;
}

std::string ContentBrowserClient::GetWebBluetoothBlacklist() {
  return std::string();
}

QuotaPermissionContext* ContentBrowserClient::CreateQuotaPermissionContext() {
  return nullptr;
}

std::unique_ptr<storage::QuotaEvictionPolicy>
ContentBrowserClient::GetTemporaryStorageEvictionPolicy(
    content::BrowserContext* context) {
  return std::unique_ptr<storage::QuotaEvictionPolicy>();
}

void ContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<ClientCertificateDelegate> delegate) {}

net::URLRequestContext* ContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, ResourceContext* context) {
  return nullptr;
}

std::string ContentBrowserClient::GetStoragePartitionIdForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  return std::string();
}

bool ContentBrowserClient::IsValidStoragePartitionId(
    BrowserContext* browser_context,
    const std::string& partition_id) {
  // Since the GetStoragePartitionIdForChildProcess() only generates empty
  // strings, we should only ever see empty strings coming back.
  return partition_id.empty();
}

void ContentBrowserClient::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site,
    bool can_be_default,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {
  partition_domain->clear();
  partition_name->clear();
  *in_memory = false;
}

MediaObserver* ContentBrowserClient::GetMediaObserver() {
  return nullptr;
}

PlatformNotificationService*
ContentBrowserClient::GetPlatformNotificationService() {
  return nullptr;
}

bool ContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
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
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return true;
}

SpeechRecognitionManagerDelegate*
    ContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return nullptr;
}

net::NetLog* ContentBrowserClient::GetNetLog() {
  return nullptr;
}

GeolocationDelegate* ContentBrowserClient::CreateGeolocationDelegate() {
  // We don't need to override anything, the default implementation is good.
  return nullptr;
}

bool ContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

base::FilePath ContentBrowserClient::GetDefaultDownloadDirectory() {
  return base::FilePath();
}

std::string ContentBrowserClient::GetDefaultDownloadName() {
  return std::string();
}

base::FilePath ContentBrowserClient::GetShaderDiskCacheDirectory() {
  return base::FilePath();
}

BrowserPpapiHost*
    ContentBrowserClient::GetExternalBrowserPpapiHost(int plugin_process_id) {
  return nullptr;
}

bool ContentBrowserClient::AllowPepperSocketAPI(
    BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const SocketPermissionRequest* params) {
  return false;
}

bool ContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

std::unique_ptr<VpnServiceProxy> ContentBrowserClient::GetVpnServiceProxy(
    BrowserContext* browser_context) {
  return nullptr;
}

ui::SelectFilePolicy* ContentBrowserClient::CreateSelectFilePolicy(
    WebContents* web_contents) {
  return nullptr;
}

DevToolsManagerDelegate* ContentBrowserClient::GetDevToolsManagerDelegate() {
  return nullptr;
}

TracingDelegate* ContentBrowserClient::GetTracingDelegate() {
  return nullptr;
}

bool ContentBrowserClient::IsPluginAllowedToCallRequestOSFileHandle(
    BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

bool ContentBrowserClient::IsPluginAllowedToUseDevChannelAPIs(
    BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

std::string ContentBrowserClient::GetShellUserIdForBrowserContext(
    BrowserContext* browser_context) {
  return base::GenerateGUID();
}

PresentationServiceDelegate*
ContentBrowserClient::GetPresentationServiceDelegate(
    WebContents* web_contents) {
  return nullptr;
}

void ContentBrowserClient::OpenURL(
    content::BrowserContext* browser_context,
    const content::OpenURLParams& params,
    const base::Callback<void(content::WebContents*)>& callback) {
  callback.Run(nullptr);
}

ScopedVector<NavigationThrottle>
ContentBrowserClient::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  return ScopedVector<NavigationThrottle>();
}

#if defined(OS_WIN)
const wchar_t* ContentBrowserClient::GetResourceDllName() {
  return nullptr;
}

bool ContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy) {
  return true;
}

base::string16 ContentBrowserClient::GetAppContainerSidForSandboxType(
    int sandbox_type) const {
  // Embedders should override this method and return different SIDs for each
  // sandbox type. Note: All content level tests will run child processes in the
  // same AppContainer.
  return base::string16(
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012148-129201922");
}

bool ContentBrowserClient::IsWin32kLockdownEnabledForMimeType(
    const std::string& mime_type) const {
  // TODO(wfh): Enable this by default once Win32k lockdown for PPAPI processes
  // is enabled by default in Chrome. See crbug.com/523278.
  return false;
}
#endif  // defined(OS_WIN)

#if defined(VIDEO_HOLE)
ExternalVideoSurfaceContainer*
ContentBrowserClient::OverrideCreateExternalVideoSurfaceContainer(
    WebContents* web_contents) {
  NOTREACHED() << "Hole-punching is not supported. See crbug.com/469348.";
  return nullptr;
}
#endif

}  // namespace content
