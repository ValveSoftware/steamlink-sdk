// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/view_appcache_internals_job.h"

#include <algorithm>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/format_macros.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/view_cache_helper.h"
#include "webkit/browser/appcache/appcache.h"
#include "webkit/browser/appcache/appcache_group.h"
#include "webkit/browser/appcache/appcache_policy.h"
#include "webkit/browser/appcache/appcache_response.h"
#include "webkit/browser/appcache/appcache_service_impl.h"
#include "webkit/browser/appcache/appcache_storage.h"

using appcache::AppCacheGroup;
using appcache::AppCacheInfo;
using appcache::AppCacheInfoCollection;
using appcache::AppCacheInfoVector;
using appcache::AppCacheServiceImpl;
using appcache::AppCacheStorage;
using appcache::AppCacheStorageReference;
using appcache::AppCacheResourceInfo;
using appcache::AppCacheResourceInfoVector;
using appcache::AppCacheResponseInfo;
using appcache::AppCacheResponseReader;

namespace content {
namespace {

const char kErrorMessage[] = "Error in retrieving Application Caches.";
const char kEmptyAppCachesMessage[] = "No available Application Caches.";
const char kManifestNotFoundMessage[] = "Manifest not found.";
const char kManifest[] = "Manifest: ";
const char kSize[] = "Size: ";
const char kCreationTime[] = "Creation Time: ";
const char kLastAccessTime[] = "Last Access Time: ";
const char kLastUpdateTime[] = "Last Update Time: ";
const char kFormattedDisabledAppCacheMsg[] =
    "<b><i><font color=\"FF0000\">"
    "This Application Cache is disabled by policy.</font></i></b><br/>";
const char kRemoveCacheLabel[] = "Remove";
const char kViewCacheLabel[] = "View Entries";
const char kRemoveCacheCommand[] = "remove-cache";
const char kViewCacheCommand[] = "view-cache";
const char kViewEntryCommand[] = "view-entry";

void EmitPageStart(std::string* out) {
  out->append(
      "<!DOCTYPE HTML>\n"
      "<html><title>AppCache Internals</title>\n"
      "<meta http-equiv=\"Content-Security-Policy\""
      "  content=\"object-src 'none'; script-src 'none'\">\n"
      "<style>\n"
      "body { font-family: sans-serif; font-size: 0.8em; }\n"
      "tt, code, pre { font-family: WebKitHack, monospace; }\n"
      "form { display: inline; }\n"
      ".subsection_body { margin: 10px 0 10px 2em; }\n"
      ".subsection_title { font-weight: bold; }\n"
      "</style>\n"
      "</head><body>\n");
}

void EmitPageEnd(std::string* out) {
  out->append("</body></html>\n");
}

void EmitListItem(const std::string& label,
                  const std::string& data,
                  std::string* out) {
  out->append("<li>");
  out->append(net::EscapeForHTML(label));
  out->append(net::EscapeForHTML(data));
  out->append("</li>\n");
}

void EmitAnchor(const std::string& url, const std::string& text,
                std::string* out) {
  out->append("<a href=\"");
  out->append(net::EscapeForHTML(url));
  out->append("\">");
  out->append(net::EscapeForHTML(text));
  out->append("</a>");
}

void EmitCommandAnchor(const char* label,
                       const GURL& base_url,
                       const char* command,
                       const char* param,
                       std::string* out) {
  std::string query(command);
  query.push_back('=');
  query.append(param);
  GURL::Replacements replacements;
  replacements.SetQuery(query.data(), url::Component(0, query.length()));
  GURL command_url = base_url.ReplaceComponents(replacements);
  EmitAnchor(command_url.spec(), label, out);
}

void EmitAppCacheInfo(const GURL& base_url,
                      AppCacheServiceImpl* service,
                      const AppCacheInfo* info,
                      std::string* out) {
  std::string manifest_url_base64;
  base::Base64Encode(info->manifest_url.spec(), &manifest_url_base64);

  out->append("\n<p>");
  out->append(kManifest);
  EmitAnchor(info->manifest_url.spec(), info->manifest_url.spec(), out);
  out->append("<br/>\n");
  if (!service->appcache_policy()->CanLoadAppCache(
          info->manifest_url, info->manifest_url)) {
    out->append(kFormattedDisabledAppCacheMsg);
  }
  out->append("\n<br/>\n");
  EmitCommandAnchor(kRemoveCacheLabel, base_url,
                    kRemoveCacheCommand, manifest_url_base64.c_str(), out);
  out->append("&nbsp;&nbsp;");
  EmitCommandAnchor(kViewCacheLabel, base_url,
                    kViewCacheCommand, manifest_url_base64.c_str(), out);
  out->append("\n<br/>\n");
  out->append("<ul>");
  EmitListItem(
      kSize,
      base::UTF16ToUTF8(FormatBytesUnlocalized(info->size)),
      out);
  EmitListItem(
      kCreationTime,
      base::UTF16ToUTF8(TimeFormatFriendlyDateAndTime(info->creation_time)),
      out);
  EmitListItem(
      kLastUpdateTime,
      base::UTF16ToUTF8(TimeFormatFriendlyDateAndTime(info->last_update_time)),
      out);
  EmitListItem(
      kLastAccessTime,
      base::UTF16ToUTF8(TimeFormatFriendlyDateAndTime(info->last_access_time)),
      out);
  out->append("</ul></p></br>\n");
}

void EmitAppCacheInfoVector(
    const GURL& base_url,
    AppCacheServiceImpl* service,
    const AppCacheInfoVector& appcaches,
    std::string* out) {
  for (std::vector<AppCacheInfo>::const_iterator info =
           appcaches.begin();
       info != appcaches.end(); ++info) {
    EmitAppCacheInfo(base_url, service, &(*info), out);
  }
}

void EmitTableData(const std::string& data, bool align_right, bool bold,
                   std::string* out) {
  if (align_right)
    out->append("<td align='right'>");
  else
    out->append("<td>");
  if (bold)
    out->append("<b>");
  out->append(data);
  if (bold)
    out->append("</b>");
  out->append("</td>");
}

std::string FormFlagsString(const AppCacheResourceInfo& info) {
  std::string str;
  if (info.is_manifest)
    str.append("Manifest, ");
  if (info.is_master)
    str.append("Master, ");
  if (info.is_intercept)
    str.append("Intercept, ");
  if (info.is_fallback)
    str.append("Fallback, ");
  if (info.is_explicit)
    str.append("Explicit, ");
  if (info.is_foreign)
    str.append("Foreign, ");
  return str;
}

std::string FormViewEntryAnchor(const GURL& base_url,
                                const GURL& manifest_url, const GURL& entry_url,
                                int64 response_id,
                                int64 group_id) {
  std::string manifest_url_base64;
  std::string entry_url_base64;
  std::string response_id_string;
  std::string group_id_string;
  base::Base64Encode(manifest_url.spec(), &manifest_url_base64);
  base::Base64Encode(entry_url.spec(), &entry_url_base64);
  response_id_string = base::Int64ToString(response_id);
  group_id_string = base::Int64ToString(group_id);

  std::string query(kViewEntryCommand);
  query.push_back('=');
  query.append(manifest_url_base64);
  query.push_back('|');
  query.append(entry_url_base64);
  query.push_back('|');
  query.append(response_id_string);
  query.push_back('|');
  query.append(group_id_string);

  GURL::Replacements replacements;
  replacements.SetQuery(query.data(), url::Component(0, query.length()));
  GURL view_entry_url = base_url.ReplaceComponents(replacements);

  std::string anchor;
  EmitAnchor(view_entry_url.spec(), entry_url.spec(), &anchor);
  return anchor;
}

void EmitAppCacheResourceInfoVector(
    const GURL& base_url,
    const GURL& manifest_url,
    const AppCacheResourceInfoVector& resource_infos,
    int64 group_id,
    std::string* out) {
  out->append("<table border='0'>\n");
  out->append("<tr>");
  EmitTableData("Flags", false, true, out);
  EmitTableData("URL", false, true, out);
  EmitTableData("Size (headers and data)", true, true, out);
  out->append("</tr>\n");
  for (AppCacheResourceInfoVector::const_iterator
          iter = resource_infos.begin();
       iter != resource_infos.end(); ++iter) {
    out->append("<tr>");
    EmitTableData(FormFlagsString(*iter), false, false, out);
    EmitTableData(FormViewEntryAnchor(base_url, manifest_url,
                                      iter->url, iter->response_id,
                                      group_id),
                  false, false, out);
    EmitTableData(base::UTF16ToUTF8(FormatBytesUnlocalized(iter->size)),
                  true, false, out);
    out->append("</tr>\n");
  }
  out->append("</table>\n");
}

void EmitResponseHeaders(net::HttpResponseHeaders* headers, std::string* out) {
  out->append("<hr><pre>");
  out->append(net::EscapeForHTML(headers->GetStatusLine()));
  out->push_back('\n');

  void* iter = NULL;
  std::string name, value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    out->append(net::EscapeForHTML(name));
    out->append(": ");
    out->append(net::EscapeForHTML(value));
    out->push_back('\n');
  }
  out->append("</pre>");
}

void EmitHexDump(const char *buf, size_t buf_len, size_t total_len,
                 std::string* out) {
  out->append("<hr><pre>");
  base::StringAppendF(out, "Showing %d of %d bytes\n\n",
                      static_cast<int>(buf_len), static_cast<int>(total_len));
  net::ViewCacheHelper::HexDump(buf, buf_len, out);
  if (buf_len < total_len)
    out->append("\nNote: data is truncated...");
  out->append("</pre>");
}

GURL DecodeBase64URL(const std::string& base64) {
  std::string url;
  base::Base64Decode(base64, &url);
  return GURL(url);
}

bool ParseQuery(const std::string& query,
                std::string* command, std::string* value) {
  size_t position = query.find("=");
  if (position == std::string::npos)
    return false;
  *command = query.substr(0, position);
  *value = query.substr(position + 1);
  return !command->empty() && !value->empty();
}

bool SortByManifestUrl(const AppCacheInfo& lhs,
                       const AppCacheInfo& rhs) {
  return lhs.manifest_url.spec() < rhs.manifest_url.spec();
}

bool SortByResourceUrl(const AppCacheResourceInfo& lhs,
                       const AppCacheResourceInfo& rhs) {
  return lhs.url.spec() < rhs.url.spec();
}

GURL ClearQuery(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

// Simple base class for the job subclasses defined here.
class BaseInternalsJob : public net::URLRequestSimpleJob,
                         public AppCacheServiceImpl::Observer {
 protected:
  BaseInternalsJob(net::URLRequest* request,
                   net::NetworkDelegate* network_delegate,
                   AppCacheServiceImpl* service)
      : URLRequestSimpleJob(request, network_delegate),
        appcache_service_(service),
        appcache_storage_(service->storage()) {
    appcache_service_->AddObserver(this);
  }

  virtual ~BaseInternalsJob() {
    appcache_service_->RemoveObserver(this);
  }

  virtual void OnServiceReinitialized(
      AppCacheStorageReference* old_storage_ref) OVERRIDE {
    if (old_storage_ref->storage() == appcache_storage_)
      disabled_storage_reference_ = old_storage_ref;
  }

  AppCacheServiceImpl* appcache_service_;
  AppCacheStorage* appcache_storage_;
  scoped_refptr<AppCacheStorageReference> disabled_storage_reference_;
};

// Job that lists all appcaches in the system.
class MainPageJob : public BaseInternalsJob {
 public:
  MainPageJob(net::URLRequest* request,
              net::NetworkDelegate* network_delegate,
              AppCacheServiceImpl* service)
      : BaseInternalsJob(request, network_delegate, service),
        weak_factory_(this) {
  }

  virtual void Start() OVERRIDE {
    DCHECK(request_);
    info_collection_ = new AppCacheInfoCollection;
    appcache_service_->GetAllAppCacheInfo(
        info_collection_.get(),
        base::Bind(&MainPageJob::OnGotInfoComplete,
                   weak_factory_.GetWeakPtr()));
  }

  // Produces a page containing the listing
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* out,
                      const net::CompletionCallback& callback) const OVERRIDE {
    mime_type->assign("text/html");
    charset->assign("UTF-8");

    out->clear();
    EmitPageStart(out);
    if (!info_collection_.get()) {
      out->append(kErrorMessage);
    } else if (info_collection_->infos_by_origin.empty()) {
      out->append(kEmptyAppCachesMessage);
    } else {
      typedef std::map<GURL, AppCacheInfoVector> InfoByOrigin;
      AppCacheInfoVector appcaches;
      for (InfoByOrigin::const_iterator origin =
               info_collection_->infos_by_origin.begin();
           origin != info_collection_->infos_by_origin.end(); ++origin) {
        appcaches.insert(appcaches.end(),
                         origin->second.begin(), origin->second.end());
      }
      std::sort(appcaches.begin(), appcaches.end(), SortByManifestUrl);

      GURL base_url = ClearQuery(request_->url());
      EmitAppCacheInfoVector(base_url, appcache_service_, appcaches, out);
    }
    EmitPageEnd(out);
    return net::OK;
  }

 private:
  virtual ~MainPageJob() {}

  void OnGotInfoComplete(int rv) {
    if (rv != net::OK)
      info_collection_ = NULL;
    StartAsync();
  }

  scoped_refptr<AppCacheInfoCollection> info_collection_;
  base::WeakPtrFactory<MainPageJob> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(MainPageJob);
};

// Job that redirects back to the main appcache internals page.
class RedirectToMainPageJob : public BaseInternalsJob {
 public:
  RedirectToMainPageJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        AppCacheServiceImpl* service)
      : BaseInternalsJob(request, network_delegate, service) {}

  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE {
    return net::OK;  // IsRedirectResponse induces a redirect.
  }

  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) OVERRIDE {
    *location = ClearQuery(request_->url());
    *http_status_code = 307;
    return true;
  }

 protected:
  virtual ~RedirectToMainPageJob() {}
};

// Job that removes an appcache and then redirects back to the main page.
class RemoveAppCacheJob : public RedirectToMainPageJob {
 public:
  RemoveAppCacheJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      AppCacheServiceImpl* service,
      const GURL& manifest_url)
      : RedirectToMainPageJob(request, network_delegate, service),
        manifest_url_(manifest_url),
        weak_factory_(this) {
  }

  virtual void Start() OVERRIDE {
    DCHECK(request_);

    appcache_service_->DeleteAppCacheGroup(
        manifest_url_,base::Bind(&RemoveAppCacheJob::OnDeleteAppCacheComplete,
                                 weak_factory_.GetWeakPtr()));
  }

 private:
  virtual ~RemoveAppCacheJob() {}

  void OnDeleteAppCacheComplete(int rv) {
    StartAsync();  // Causes the base class to redirect.
  }

  GURL manifest_url_;
  base::WeakPtrFactory<RemoveAppCacheJob> weak_factory_;
};


// Job shows the details of a particular manifest url.
class ViewAppCacheJob : public BaseInternalsJob,
                        public AppCacheStorage::Delegate {
 public:
  ViewAppCacheJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      AppCacheServiceImpl* service,
      const GURL& manifest_url)
      : BaseInternalsJob(request, network_delegate, service),
        manifest_url_(manifest_url) {}

  virtual void Start() OVERRIDE {
    DCHECK(request_);
    appcache_storage_->LoadOrCreateGroup(manifest_url_, this);
  }

  // Produces a page containing the entries listing.
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* out,
                      const net::CompletionCallback& callback) const OVERRIDE {
    mime_type->assign("text/html");
    charset->assign("UTF-8");
    out->clear();
    EmitPageStart(out);
    if (appcache_info_.manifest_url.is_empty()) {
      out->append(kManifestNotFoundMessage);
    } else {
      GURL base_url = ClearQuery(request_->url());
      EmitAppCacheInfo(base_url, appcache_service_, &appcache_info_, out);
      EmitAppCacheResourceInfoVector(base_url,
                                     manifest_url_,
                                     resource_infos_,
                                     appcache_info_.group_id,
                                     out);
    }
    EmitPageEnd(out);
    return net::OK;
  }

 private:
  virtual ~ViewAppCacheJob() {
    appcache_storage_->CancelDelegateCallbacks(this);
  }

  // AppCacheStorage::Delegate override
  virtual void OnGroupLoaded(
      AppCacheGroup* group, const GURL& manifest_url) OVERRIDE {
    DCHECK_EQ(manifest_url_, manifest_url);
    if (group && group->newest_complete_cache()) {
      appcache_info_.manifest_url = manifest_url;
      appcache_info_.group_id = group->group_id();
      appcache_info_.size = group->newest_complete_cache()->cache_size();
      appcache_info_.creation_time = group->creation_time();
      appcache_info_.last_update_time =
          group->newest_complete_cache()->update_time();
      appcache_info_.last_access_time = base::Time::Now();
      group->newest_complete_cache()->ToResourceInfoVector(&resource_infos_);
      std::sort(resource_infos_.begin(), resource_infos_.end(),
                SortByResourceUrl);
    }
    StartAsync();
  }

  GURL manifest_url_;
  AppCacheInfo appcache_info_;
  AppCacheResourceInfoVector resource_infos_;
  DISALLOW_COPY_AND_ASSIGN(ViewAppCacheJob);
};

// Job that shows the details of a particular cached resource.
class ViewEntryJob : public BaseInternalsJob,
                     public AppCacheStorage::Delegate {
 public:
  ViewEntryJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      AppCacheServiceImpl* service,
      const GURL& manifest_url,
      const GURL& entry_url,
      int64 response_id, int64 group_id)
      : BaseInternalsJob(request, network_delegate, service),
        manifest_url_(manifest_url), entry_url_(entry_url),
        response_id_(response_id), group_id_(group_id), amount_read_(0) {
  }

  virtual void Start() OVERRIDE {
    DCHECK(request_);
    appcache_storage_->LoadResponseInfo(
        manifest_url_, group_id_, response_id_, this);
  }

  // Produces a page containing the response headers and data.
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* out,
                      const net::CompletionCallback& callback) const OVERRIDE {
    mime_type->assign("text/html");
    charset->assign("UTF-8");
    out->clear();
    EmitPageStart(out);
    EmitAnchor(entry_url_.spec(), entry_url_.spec(), out);
    out->append("<br/>\n");
    if (response_info_.get()) {
      if (response_info_->http_response_info())
        EmitResponseHeaders(response_info_->http_response_info()->headers.get(),
                            out);
      else
        out->append("Failed to read response headers.<br>");

      if (response_data_.get()) {
        EmitHexDump(response_data_->data(),
                    amount_read_,
                    response_info_->response_data_size(),
                    out);
      } else {
        out->append("Failed to read response data.<br>");
      }
    } else {
      out->append("Failed to read response headers and data.<br>");
    }
    EmitPageEnd(out);
    return net::OK;
  }

 private:
  virtual ~ViewEntryJob() {
    appcache_storage_->CancelDelegateCallbacks(this);
  }

  virtual void OnResponseInfoLoaded(
      AppCacheResponseInfo* response_info, int64 response_id) OVERRIDE {
    if (!response_info) {
      StartAsync();
      return;
    }
    response_info_ = response_info;

    // Read the response data, truncating if its too large.
    const int64 kLimit = 100 * 1000;
    int64 amount_to_read =
        std::min(kLimit, response_info->response_data_size());
    response_data_ = new net::IOBuffer(amount_to_read);

    reader_.reset(appcache_storage_->CreateResponseReader(
        manifest_url_, group_id_, response_id_));
    reader_->ReadData(
        response_data_.get(),
        amount_to_read,
        base::Bind(&ViewEntryJob::OnReadComplete, base::Unretained(this)));
  }

  void OnReadComplete(int result) {
    reader_.reset();
    amount_read_ = result;
    if (result < 0)
      response_data_ = NULL;
    StartAsync();
  }

  GURL manifest_url_;
  GURL entry_url_;
  int64 response_id_;
  int64 group_id_;
  scoped_refptr<AppCacheResponseInfo> response_info_;
  scoped_refptr<net::IOBuffer> response_data_;
  int amount_read_;
  scoped_ptr<AppCacheResponseReader> reader_;
};

}  // namespace

net::URLRequestJob* ViewAppCacheInternalsJobFactory::CreateJobForRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    AppCacheServiceImpl* service) {
  if (!request->url().has_query())
    return new MainPageJob(request, network_delegate, service);

  std::string command;
  std::string param;
  ParseQuery(request->url().query(), &command, &param);

  if (command == kRemoveCacheCommand)
    return new RemoveAppCacheJob(request, network_delegate, service,
                                 DecodeBase64URL(param));

  if (command == kViewCacheCommand)
    return new ViewAppCacheJob(request, network_delegate, service,
                               DecodeBase64URL(param));

  std::vector<std::string> tokens;
  int64 response_id;
  int64 group_id;
  if (command == kViewEntryCommand && Tokenize(param, "|", &tokens) == 4u &&
      base::StringToInt64(tokens[2], &response_id) &&
      base::StringToInt64(tokens[3], &group_id)) {
    return new ViewEntryJob(request, network_delegate, service,
                            DecodeBase64URL(tokens[0]),  // manifest url
                            DecodeBase64URL(tokens[1]),  // entry url
                            response_id, group_id);
  }

  return new RedirectToMainPageJob(request, network_delegate, service);
}

}  // namespace content
