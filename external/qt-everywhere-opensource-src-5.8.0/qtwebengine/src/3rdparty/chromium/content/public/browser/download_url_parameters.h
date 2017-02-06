// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/common/referrer.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "url/gurl.h"

namespace content {

class DownloadItem;
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
  // An OnStartedCallback is invoked when a response is available for the
  // download request. For new downloads, this callback is invoked after the
  // OnDownloadCreated notification is issued by the DownloadManager. If the
  // download fails, then the DownloadInterruptReason parameter will indicate
  // the failure.
  //
  // DownloadItem* may be nullptr if no DownloadItem was created. DownloadItems
  // are not created when a resource throttle or a resource handler blocks the
  // download request. I.e. the download triggered a warning of some sort and
  // the user chose to not to proceed with the download as a result.
  typedef base::Callback<void(DownloadItem*, DownloadInterruptReason)>
      OnStartedCallback;

  typedef std::pair<std::string, std::string> RequestHeadersNameValuePair;
  typedef std::vector<RequestHeadersNameValuePair> RequestHeadersType;

  // Construct DownloadUrlParameters for downloading the resource at |url| and
  // associating the download with the main frame of the given WebContents.
  static std::unique_ptr<DownloadUrlParameters> CreateForWebContentsMainFrame(
      WebContents* web_contents,
      const GURL& url);

  // Constructs a download not associated with a frame.
  //
  // It is not safe to have downloads not associated with a frame and
  // this should only be done in a limited set of cases where the download URL
  // has been previously vetted. A download that's initiated without
  // associating it with a frame don't receive the same security checks
  // as a request that's associated with one. Hence, downloads that are not
  // associated with a frame should only be made for URLs that are either
  // trusted or URLs that have previously been successfully issued using a
  // non-privileged frame.
  DownloadUrlParameters(
      const GURL& url,
      net::URLRequestContextGetter* url_request_context_getter);

  // The RenderView routing ID must correspond to the RenderView of the
  // RenderFrame, both of which share the same RenderProcess. This may be a
  // different RenderView than the WebContents' main RenderView.
  DownloadUrlParameters(
      const GURL& url,
      int render_process_host_id,
      int render_view_host_routing_id,
      int render_frame_host_routing_id,
      net::URLRequestContextGetter* url_request_context_getter);

  ~DownloadUrlParameters();

  // Should be set to true if the download was initiated by a script or a web
  // page. I.e. if the download request cannot be attributed to an explicit user
  // request for a download, then set this value to true.
  void set_content_initiated(bool content_initiated) {
    content_initiated_ = content_initiated;
  }
  void add_request_header(const std::string& name, const std::string& value) {
    request_headers_.push_back(make_pair(name, value));
  }

  // HTTP Referrer and referrer encoding.
  void set_referrer(const Referrer& referrer) { referrer_ = referrer; }
  void set_referrer_encoding(const std::string& referrer_encoding) {
    referrer_encoding_ = referrer_encoding;
  }

  // If this is a request for resuming an HTTP/S download, |last_modified|
  // should be the value of the last seen Last-Modified response header.
  void set_last_modified(const std::string& last_modified) {
    last_modified_ = last_modified;
  }

  // If this is a request for resuming an HTTP/S download, |etag| should be the
  // last seen Etag response header.
  void set_etag(const std::string& etag) {
    etag_ = etag;
  }

  // HTTP method to use.
  void set_method(const std::string& method) {
    method_ = method;
  }

  // Body of the HTTP POST request.
  void set_post_body(const std::string& post_body) {
    post_body_ = post_body;
  }

  // If |prefer_cache| is true and the response to |url| is in the HTTP cache,
  // it will be used without validation. If |method| is POST, then |post_id_|
  // shoud be set via |set_post_id()| below to the identifier of the POST
  // transaction used to originally retrieve the resource.
  void set_prefer_cache(bool prefer_cache) {
    prefer_cache_ = prefer_cache;
  }

  // See set_prefer_cache() above.
  void set_post_id(int64_t post_id) { post_id_ = post_id; }

  // See OnStartedCallback above.
  void set_callback(const OnStartedCallback& callback) {
    callback_ = callback;
  }

  // If not empty, specifies the full target path for the download. This value
  // overrides the filename suggested by a Content-Disposition headers. It
  // should only be set for programmatic downloads where the caller can verify
  // the safety of the filename and the resulting download.
  void set_file_path(const base::FilePath& file_path) {
    save_info_.file_path = file_path;
  }

  // Suggested filename for the download. The suggestion can be overridden by
  // either a Content-Disposition response header or a |file_path|.
  void set_suggested_name(const base::string16& suggested_name) {
    save_info_.suggested_name = suggested_name;
  }

  // If |offset| is non-zero, then a byte range request will be issued to fetch
  // the range of bytes starting at |offset| through to the end of thedownload.
  void set_offset(int64_t offset) { save_info_.offset = offset; }

  // If |offset| is non-zero, then |hash_of_partial_file| contains the raw
  // SHA-256 hash of the first |offset| bytes of the target file. Only
  // meaningful if a partial file exists and is identified by either the
  // |file_path()| or |file()|.
  void set_hash_of_partial_file(const std::string& hash_of_partial_file) {
    save_info_.hash_of_partial_file = hash_of_partial_file;
  }

  // If |offset| is non-zero, then |hash_state| indicates the SHA-256 hash state
  // of the first |offset| bytes of the target file. In this case, the prefix
  // hash will be ignored since the |hash_state| is assumed to be correct if
  // provided.
  void set_hash_state(std::unique_ptr<crypto::SecureHash> hash_state) {
    save_info_.hash_state = std::move(hash_state);
  }

  // If |prompt| is true, then the user will be prompted for a filename. Ignored
  // if |file_path| is non-empty.
  void set_prompt(bool prompt) { save_info_.prompt_for_save_location = prompt; }
  void set_file(base::File file) { save_info_.file = std::move(file); }
  void set_do_not_prompt_for_login(bool do_not_prompt) {
    do_not_prompt_for_login_ = do_not_prompt;
  }

  // For downloads of blob URLs, the caller can store a BlobDataHandle in the
  // DownloadUrlParameters object so that the blob will remain valid until
  // the download starts. The BlobDataHandle will be attached to the associated
  // URLRequest.
  //
  // This is optional. If left unspecified, and the blob URL cannot be mapped to
  // a blob by the time the download request starts, then the download will
  // fail.
  void set_blob_data_handle(
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
    blob_data_handle_ = std::move(blob_data_handle);
  }

  const OnStartedCallback& callback() const { return callback_; }
  bool content_initiated() const { return content_initiated_; }
  const std::string& last_modified() const { return last_modified_; }
  const std::string& etag() const { return etag_; }
  const std::string& method() const { return method_; }
  const std::string& post_body() const { return post_body_; }
  int64_t post_id() const { return post_id_; }
  bool prefer_cache() const { return prefer_cache_; }
  const Referrer& referrer() const { return referrer_; }
  const std::string& referrer_encoding() const { return referrer_encoding_; }

  // These will be -1 if the request is not associated with a frame. See
  // the constructors for more.
  int render_process_host_id() const { return render_process_host_id_; }
  int render_view_host_routing_id() const {
    return render_view_host_routing_id_;
  }
  int render_frame_host_routing_id() const {
    return render_frame_host_routing_id_;
  }

  const RequestHeadersType& request_headers() const { return request_headers_; }
  net::URLRequestContextGetter* url_request_context_getter() {
    return url_request_context_getter_.get();
  }
  const base::FilePath& file_path() const { return save_info_.file_path; }
  const base::string16& suggested_name() const {
    return save_info_.suggested_name;
  }
  int64_t offset() const { return save_info_.offset; }
  const std::string& hash_of_partial_file() const {
    return save_info_.hash_of_partial_file;
  }
  bool prompt() const { return save_info_.prompt_for_save_location; }
  const GURL& url() const { return url_; }
  bool do_not_prompt_for_login() const { return do_not_prompt_for_login_; }

  // STATE_CHANGING: Return the BlobDataHandle.
  std::unique_ptr<storage::BlobDataHandle> GetBlobDataHandle() {
    return std::move(blob_data_handle_);
  }

  // STATE CHANGING: All save_info_ sub-objects will be in an indeterminate
  // state following this call.
  DownloadSaveInfo GetSaveInfo() { return std::move(save_info_); }

 private:
  OnStartedCallback callback_;
  bool content_initiated_;
  RequestHeadersType request_headers_;
  std::string last_modified_;
  std::string etag_;
  std::string method_;
  std::string post_body_;
  int64_t post_id_;
  bool prefer_cache_;
  Referrer referrer_;
  std::string referrer_encoding_;
  int render_process_host_id_;
  int render_view_host_routing_id_;
  int render_frame_host_routing_id_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  DownloadSaveInfo save_info_;
  GURL url_;
  bool do_not_prompt_for_login_;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUrlParameters);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_
