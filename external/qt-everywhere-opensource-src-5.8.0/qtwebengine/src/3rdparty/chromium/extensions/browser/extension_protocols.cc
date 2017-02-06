// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_protocols.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_simple_job.h"
#include "url/url_util.h"

using content::BrowserThread;
using content::ResourceRequestInfo;
using content::ResourceType;
using extensions::Extension;
using extensions::SharedModuleInfo;

namespace extensions {
namespace {

class GeneratedBackgroundPageJob : public net::URLRequestSimpleJob {
 public:
  GeneratedBackgroundPageJob(net::URLRequest* request,
                             net::NetworkDelegate* network_delegate,
                             const scoped_refptr<const Extension> extension,
                             const std::string& content_security_policy)
      : net::URLRequestSimpleJob(request, network_delegate),
        extension_(extension) {
    const bool send_cors_headers = false;
    // Leave cache headers out of generated background page jobs.
    response_info_.headers = BuildHttpHeaders(content_security_policy,
                                              send_cors_headers,
                                              base::Time());
  }

  // Overridden from URLRequestSimpleJob:
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const net::CompletionCallback& callback) const override {
    *mime_type = "text/html";
    *charset = "utf-8";

    *data = "<!DOCTYPE html>\n<body>\n";
    const std::vector<std::string>& background_scripts =
        extensions::BackgroundInfo::GetBackgroundScripts(extension_.get());
    for (size_t i = 0; i < background_scripts.size(); ++i) {
      *data += "<script src=\"";
      *data += background_scripts[i];
      *data += "\"></script>\n";
    }

    return net::OK;
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    *info = response_info_;
  }

 private:
  ~GeneratedBackgroundPageJob() override {}

  scoped_refptr<const Extension> extension_;
  net::HttpResponseInfo response_info_;
};

base::Time GetFileLastModifiedTime(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    base::File::Info info;
    if (base::GetFileInfo(filename, &info))
      return info.last_modified;
  }
  return base::Time();
}

base::Time GetFileCreationTime(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    base::File::Info info;
    if (base::GetFileInfo(filename, &info))
      return info.creation_time;
  }
  return base::Time();
}

void ReadResourceFilePathAndLastModifiedTime(
    const extensions::ExtensionResource& resource,
    const base::FilePath& directory,
    base::FilePath* file_path,
    base::Time* last_modified_time) {
  *file_path = resource.GetFilePath();
  *last_modified_time = GetFileLastModifiedTime(*file_path);
  // While we're here, log the delta between extension directory
  // creation time and the resource's last modification time.
  base::ElapsedTimer query_timer;
  base::Time dir_creation_time = GetFileCreationTime(directory);
  UMA_HISTOGRAM_TIMES("Extensions.ResourceDirectoryTimestampQueryLatency",
                      query_timer.Elapsed());
  int64_t delta_seconds = (*last_modified_time - dir_creation_time).InSeconds();
  if (delta_seconds >= 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ResourceLastModifiedDelta",
                                delta_seconds,
                                0,
                                base::TimeDelta::FromDays(30).InSeconds(),
                                50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ResourceLastModifiedNegativeDelta",
                                -delta_seconds,
                                1,
                                base::TimeDelta::FromDays(30).InSeconds(),
                                50);
  }
}

class URLRequestExtensionJob : public net::URLRequestFileJob {
 public:
  URLRequestExtensionJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& extension_id,
                         const base::FilePath& directory_path,
                         const base::FilePath& relative_path,
                         const std::string& content_security_policy,
                         bool send_cors_header,
                         bool follow_symlinks_anywhere,
                         ContentVerifyJob* verify_job)
      : net::URLRequestFileJob(
            request,
            network_delegate,
            base::FilePath(),
            BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
        verify_job_(verify_job),
        seek_position_(0),
        bytes_read_(0),
        directory_path_(directory_path),
        // TODO(tc): Move all of these files into resources.pak so we don't
        // break when updating on Linux.
        resource_(extension_id, directory_path, relative_path),
        content_security_policy_(content_security_policy),
        send_cors_header_(send_cors_header),
        weak_factory_(this) {
    if (follow_symlinks_anywhere) {
      resource_.set_follow_symlinks_anywhere();
    }
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    *info = response_info_;
  }

  // This always returns 200 because a URLRequestExtensionJob will only get
  // created in MaybeCreateJob() if the file exists.
  int GetResponseCode() const override { return 200; }

  void Start() override {
    request_timer_.reset(new base::ElapsedTimer());
    base::FilePath* read_file_path = new base::FilePath;
    base::Time* last_modified_time = new base::Time();
    bool posted = BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&ReadResourceFilePathAndLastModifiedTime,
                   resource_,
                   directory_path_,
                   base::Unretained(read_file_path),
                   base::Unretained(last_modified_time)),
        base::Bind(&URLRequestExtensionJob::OnFilePathAndLastModifiedTimeRead,
                   weak_factory_.GetWeakPtr(),
                   base::Owned(read_file_path),
                   base::Owned(last_modified_time)));
    DCHECK(posted);
  }

  bool IsRedirectResponse(GURL* location, int* http_status_code) override {
    return false;
  }

  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override {
    // TODO(asargent) - we'll need to add proper support for range headers.
    // crbug.com/369895.
    std::string range_header;
    if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
      if (verify_job_.get())
        verify_job_ = NULL;
    }
    URLRequestFileJob::SetExtraRequestHeaders(headers);
  }

  void OnSeekComplete(int64_t result) override {
    DCHECK_EQ(seek_position_, 0);
    seek_position_ = result;
    // TODO(asargent) - we'll need to add proper support for range headers.
    // crbug.com/369895.
    if (result > 0 && verify_job_.get())
      verify_job_ = NULL;
  }

  void OnReadComplete(net::IOBuffer* buffer, int result) override {
    if (result >= 0)
      UMA_HISTOGRAM_COUNTS("ExtensionUrlRequest.OnReadCompleteResult", result);
    else
      UMA_HISTOGRAM_SPARSE_SLOWLY("ExtensionUrlRequest.OnReadCompleteError",
                                  -result);
    if (result > 0) {
      bytes_read_ += result;
      if (verify_job_.get()) {
        verify_job_->BytesRead(result, buffer->data());
        if (!remaining_bytes())
          verify_job_->DoneReading();
      }
    }
  }

 private:
  ~URLRequestExtensionJob() override {
    UMA_HISTOGRAM_COUNTS("ExtensionUrlRequest.TotalKbRead", bytes_read_ / 1024);
    UMA_HISTOGRAM_COUNTS("ExtensionUrlRequest.SeekPosition", seek_position_);
    if (request_timer_.get())
      UMA_HISTOGRAM_TIMES("ExtensionUrlRequest.Latency",
                          request_timer_->Elapsed());
  }

  void OnFilePathAndLastModifiedTimeRead(base::FilePath* read_file_path,
                                         base::Time* last_modified_time) {
    file_path_ = *read_file_path;
    response_info_.headers = BuildHttpHeaders(
        content_security_policy_,
        send_cors_header_,
        *last_modified_time);
    URLRequestFileJob::Start();
  }

  scoped_refptr<ContentVerifyJob> verify_job_;

  std::unique_ptr<base::ElapsedTimer> request_timer_;

  // The position we seeked to in the file.
  int64_t seek_position_;

  // The number of bytes of content we read from the file.
  int bytes_read_;

  net::HttpResponseInfo response_info_;
  base::FilePath directory_path_;
  extensions::ExtensionResource resource_;
  std::string content_security_policy_;
  bool send_cors_header_;
  base::WeakPtrFactory<URLRequestExtensionJob> weak_factory_;
};

bool ExtensionCanLoadInIncognito(const ResourceRequestInfo* info,
                                 const std::string& extension_id,
                                 extensions::InfoMap* extension_info_map) {
  if (!extension_info_map->IsIncognitoEnabled(extension_id))
    return false;

  // Only allow incognito toplevel navigations to extension resources in
  // split mode. In spanning mode, the extension must run in a single process,
  // and an incognito tab prevents that.
  if (info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    const Extension* extension =
        extension_info_map->extensions().GetByID(extension_id);
    return extension && extensions::IncognitoInfo::IsSplitMode(extension);
  }

  return true;
}

// Returns true if an chrome-extension:// resource should be allowed to load.
// Pass true for |is_incognito| only for incognito profiles and not Chrome OS
// guest mode profiles.
// TODO(aa): This should be moved into ExtensionResourceRequestPolicy, but we
// first need to find a way to get CanLoadInIncognito state into the renderers.
bool AllowExtensionResourceLoad(net::URLRequest* request,
                                bool is_incognito,
                                const Extension* extension,
                                extensions::InfoMap* extension_info_map) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // We have seen crashes where info is NULL: crbug.com/52374.
  if (!info) {
    // SeviceWorker net requests created through ServiceWorkerWriteToCacheJob
    // do not have ResourceRequestInfo associated with them. So skip logging
    // spurious errors below.
    // TODO(falken): Either consider attaching ResourceRequestInfo to these or
    // finish refactoring ServiceWorkerWriteToCacheJob so that it doesn't spawn
    // a new URLRequest.
    if (!ResourceRequestInfo::OriginatedFromServiceWorker(request)) {
      LOG(ERROR) << "Allowing load of " << request->url().spec()
                 << "from unknown origin. Could not find user data for "
                 << "request.";
    }
    return true;
  }

  if (is_incognito && !ExtensionCanLoadInIncognito(
                          info, request->url().host(), extension_info_map)) {
    return false;
  }

  // The following checks are meant to replicate similar set of checks in the
  // renderer process, performed by ResourceRequestPolicy::CanRequestResource.
  // These are not exactly equivalent, because we don't have the same bits of
  // information. The two checks need to be kept in sync as much as possible, as
  // an exploited renderer can bypass the checks in ResourceRequestPolicy.

  // Check if the extension for which this request is made is indeed loaded in
  // the process sending the request. If not, we need to explicitly check if
  // the resource is explicitly accessible or fits in a set of exception cases.
  // Note: This allows a case where two extensions execute in the same renderer
  // process to request each other's resources. We can't do a more precise
  // check, since the renderer can lie about which extension has made the
  // request.
  if (extension_info_map->process_map().Contains(
      request->url().host(), info->GetChildID())) {
    return true;
  }

  // PlzNavigate: frame navigations to extensions have already been checked in
  // the ExtensionNavigationThrottle.
  if (info->GetChildID() == -1 &&
      content::IsResourceTypeFrame(info->GetResourceType()) &&
      content::IsBrowserSideNavigationEnabled()) {
    return true;
  }

  // Allow the extension module embedder to grant permission for loads.
  if (ExtensionsBrowserClient::Get()->AllowCrossRendererResourceLoad(
          request, is_incognito, extension, extension_info_map)) {
    return true;
  }

  // No special exceptions for cross-process loading. Block the load.
  return false;
}

// Returns true if the given URL references an icon in the given extension.
bool URLIsForExtensionIcon(const GURL& url, const Extension* extension) {
  DCHECK(url.SchemeIs(extensions::kExtensionScheme));

  if (!extension)
    return false;

  std::string path = url.path();
  DCHECK_EQ(url.host(), extension->id());
  DCHECK(path.length() > 0 && path[0] == '/');
  path = path.substr(1);
  return extensions::IconsInfo::GetIcons(extension).ContainsPath(path);
}

class ExtensionProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ExtensionProtocolHandler(bool is_incognito,
                           extensions::InfoMap* extension_info_map)
      : is_incognito_(is_incognito), extension_info_map_(extension_info_map) {}

  ~ExtensionProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  const bool is_incognito_;
  extensions::InfoMap* const extension_info_map_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionProtocolHandler);
};

// Creates URLRequestJobs for extension:// URLs.
net::URLRequestJob*
ExtensionProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  // chrome-extension://extension-id/resource/path.js
  std::string extension_id = request->url().host();
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);

  if (!AllowExtensionResourceLoad(
          request, is_incognito_, extension, extension_info_map_)) {
    return new net::URLRequestErrorJob(request, network_delegate,
                                       net::ERR_BLOCKED_BY_CLIENT);
  }

  // If this is a disabled extension only allow the icon to load.
  base::FilePath directory_path;
  if (extension)
    directory_path = extension->path();
  if (directory_path.value().empty()) {
    const Extension* disabled_extension =
        extension_info_map_->disabled_extensions().GetByID(extension_id);
    if (URLIsForExtensionIcon(request->url(), disabled_extension))
      directory_path = disabled_extension->path();
    if (directory_path.value().empty()) {
      LOG(WARNING) << "Failed to GetPathForExtension: " << extension_id;
      return NULL;
    }
  }

  // Set up content security policy.
  std::string content_security_policy;
  bool send_cors_header = false;
  bool follow_symlinks_anywhere = false;

  if (extension) {
    std::string resource_path = request->url().path();

    // Use default CSP for <webview>.
    if (!url_request_util::IsWebViewRequest(request)) {
      content_security_policy =
          extensions::CSPInfo::GetResourceContentSecurityPolicy(extension,
                                                                resource_path);
    }

    if ((extension->manifest_version() >= 2 ||
         extensions::WebAccessibleResourcesInfo::HasWebAccessibleResources(
             extension)) &&
        extensions::WebAccessibleResourcesInfo::IsResourceWebAccessible(
            extension, resource_path)) {
      send_cors_header = true;
    }

    follow_symlinks_anywhere =
        (extension->creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE)
        != 0;
  }

  // Create a job for a generated background page.
  std::string path = request->url().path();
  if (path.size() > 1 &&
      path.substr(1) == extensions::kGeneratedBackgroundPageFilename) {
    return new GeneratedBackgroundPageJob(
        request, network_delegate, extension, content_security_policy);
  }

  // Component extension resources may be part of the embedder's resource files,
  // for example component_extension_resources.pak in Chrome.
  net::URLRequestJob* resource_bundle_job =
      extensions::ExtensionsBrowserClient::Get()
          ->MaybeCreateResourceBundleRequestJob(request,
                                                network_delegate,
                                                directory_path,
                                                content_security_policy,
                                                send_cors_header);
  if (resource_bundle_job)
    return resource_bundle_job;

  base::FilePath relative_path =
      extensions::file_util::ExtensionURLToRelativeFilePath(request->url());

  // Do not allow requests for resources in the _metadata folder, since any
  // files there are internal implementation details that should not be
  // considered part of the extension.
  if (base::FilePath(kMetadataFolder).IsParent(relative_path))
    return nullptr;

  // Handle shared resources (extension A loading resources out of extension B).
  if (SharedModuleInfo::IsImportedPath(path)) {
    std::string new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                        &new_relative_path);
    const Extension* new_extension =
        extension_info_map_->extensions().GetByID(new_extension_id);

    if (SharedModuleInfo::ImportsExtensionById(extension, new_extension_id) &&
        new_extension) {
      directory_path = new_extension->path();
      extension_id = new_extension_id;
      relative_path = base::FilePath::FromUTF8Unsafe(new_relative_path);
    } else {
      return NULL;
    }
  }
  ContentVerifyJob* verify_job = NULL;
  ContentVerifier* verifier = extension_info_map_->content_verifier();
  if (verifier) {
    verify_job =
        verifier->CreateJobFor(extension_id, directory_path, relative_path);
    if (verify_job)
      verify_job->Start();
  }

  return new URLRequestExtensionJob(request,
                                    network_delegate,
                                    extension_id,
                                    directory_path,
                                    relative_path,
                                    content_security_policy,
                                    send_cors_header,
                                    follow_symlinks_anywhere,
                                    verify_job);
}

}  // namespace

net::HttpResponseHeaders* BuildHttpHeaders(
    const std::string& content_security_policy,
    bool send_cors_header,
    const base::Time& last_modified_time) {
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  if (!content_security_policy.empty()) {
    raw_headers.append(1, '\0');
    raw_headers.append("Content-Security-Policy: ");
    raw_headers.append(content_security_policy);
  }

  if (send_cors_header) {
    raw_headers.append(1, '\0');
    raw_headers.append("Access-Control-Allow-Origin: *");
  }

  if (!last_modified_time.is_null()) {
    // Hash the time and make an etag to avoid exposing the exact
    // user installation time of the extension.
    std::string hash =
        base::StringPrintf("%" PRId64, last_modified_time.ToInternalValue());
    hash = base::SHA1HashString(hash);
    std::string etag;
    base::Base64Encode(hash, &etag);
    raw_headers.append(1, '\0');
    raw_headers.append("ETag: \"");
    raw_headers.append(etag);
    raw_headers.append("\"");
    // Also force revalidation.
    raw_headers.append(1, '\0');
    raw_headers.append("cache-control: no-cache");
  }

  raw_headers.append(2, '\0');
  return new net::HttpResponseHeaders(raw_headers);
}

std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
CreateExtensionProtocolHandler(bool is_incognito,
                               extensions::InfoMap* extension_info_map) {
  return base::WrapUnique(
      new ExtensionProtocolHandler(is_incognito, extension_info_map));
}

}  // namespace extensions
