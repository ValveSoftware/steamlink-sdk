// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {

class DownloadItem;
class ResourceContext;
class ResourceDispatcherHost;
class WebContents;

// Pass an instance of DownloadUrlParameters to DownloadManager::DownloadUrl()
// to download the content at |url|. All parameters with setters are optional.
// |referrer| and |referrer_encoding| are the referrer for the download. If
// |prefer_cache| is true, then if the response to |url| is in the HTTP cache it
// will be used without revalidation. If |post_id| is non-negative, then it
// identifies the post transaction used to originally retrieve the |url|
// resource - it also requires |prefer_cache| to be |true| since re-post'ing is
// not done.  |save_info| specifies where the downloaded file should be saved,
// and whether the user should be prompted about the download.  If not null,
// |callback| will be called when the download starts, or if an error occurs
// that prevents a download item from being created.  We send a pointer to
// content::ResourceContext instead of the usual reference so that a copy of the
// object isn't made.

class CONTENT_EXPORT DownloadUrlParameters {
 public:
  // If there is an error, then |item| will be NULL.
  typedef base::Callback<void(DownloadItem*, DownloadInterruptReason)>
      OnStartedCallback;

  typedef std::pair<std::string, std::string> RequestHeadersNameValuePair;
  typedef std::vector<RequestHeadersNameValuePair> RequestHeadersType;

  static DownloadUrlParameters* FromWebContents(
      WebContents* web_contents,
      const GURL& url);

  DownloadUrlParameters(
      const GURL& url,
      int render_process_host_id,
      int render_view_host_routing_id,
      content::ResourceContext* resource_context);

  ~DownloadUrlParameters();

  void set_content_initiated(bool content_initiated) {
    content_initiated_ = content_initiated;
  }
  void add_request_header(const std::string& name, const std::string& value) {
    request_headers_.push_back(make_pair(name, value));
  }
  void set_referrer(const Referrer& referrer) { referrer_ = referrer; }
  void set_referrer_encoding(const std::string& referrer_encoding) {
    referrer_encoding_ = referrer_encoding;
  }
  void set_load_flags(int load_flags) { load_flags_ |= load_flags; }
  void set_last_modified(const std::string& last_modified) {
    last_modified_ = last_modified;
  }
  void set_etag(const std::string& etag) {
    etag_ = etag;
  }
  void set_method(const std::string& method) {
    method_ = method;
  }
  void set_post_body(const std::string& post_body) {
    post_body_ = post_body;
  }
  void set_prefer_cache(bool prefer_cache) {
    prefer_cache_ = prefer_cache;
  }
  void set_post_id(int64 post_id) { post_id_ = post_id; }
  void set_callback(const OnStartedCallback& callback) {
    callback_ = callback;
  }
  void set_file_path(const base::FilePath& file_path) {
    save_info_.file_path = file_path;
  }
  void set_suggested_name(const base::string16& suggested_name) {
    save_info_.suggested_name = suggested_name;
  }
  void set_offset(int64 offset) { save_info_.offset = offset; }
  void set_hash_state(std::string hash_state) {
    save_info_.hash_state = hash_state;
  }
  void set_prompt(bool prompt) { save_info_.prompt_for_save_location = prompt; }
  void set_file(base::File file) {
    save_info_.file = file.Pass();
  }

  const OnStartedCallback& callback() const { return callback_; }
  bool content_initiated() const { return content_initiated_; }
  int load_flags() const { return load_flags_; }
  const std::string& last_modified() const { return last_modified_; }
  const std::string& etag() const { return etag_; }
  const std::string& method() const { return method_; }
  const std::string& post_body() const { return post_body_; }
  int64 post_id() const { return post_id_; }
  bool prefer_cache() const { return prefer_cache_; }
  const Referrer& referrer() const { return referrer_; }
  const std::string& referrer_encoding() const { return referrer_encoding_; }
  int render_process_host_id() const { return render_process_host_id_; }
  int render_view_host_routing_id() const {
    return render_view_host_routing_id_;
  }
  RequestHeadersType::const_iterator request_headers_begin() const {
    return request_headers_.begin();
  }
  RequestHeadersType::const_iterator request_headers_end() const {
    return request_headers_.end();
  }
  content::ResourceContext* resource_context() const {
    return resource_context_;
  }
  const base::FilePath& file_path() const { return save_info_.file_path; }
  const base::string16& suggested_name() const {
    return save_info_.suggested_name;
  }
  int64 offset() const { return save_info_.offset; }
  const std::string& hash_state() const { return save_info_.hash_state; }
  bool prompt() const { return save_info_.prompt_for_save_location; }
  const GURL& url() const { return url_; }

  // Note that this is state changing--the DownloadUrlParameters object
  // will not have a file attached to it after this call.
  base::File GetFile() { return save_info_.file.Pass(); }

 private:
  OnStartedCallback callback_;
  bool content_initiated_;
  RequestHeadersType request_headers_;
  int load_flags_;
  std::string last_modified_;
  std::string etag_;
  std::string method_;
  std::string post_body_;
  int64 post_id_;
  bool prefer_cache_;
  Referrer referrer_;
  std::string referrer_encoding_;
  int render_process_host_id_;
  int render_view_host_routing_id_;
  ResourceContext* resource_context_;
  DownloadSaveInfo save_info_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUrlParameters);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_
