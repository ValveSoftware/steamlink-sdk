// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"

namespace base {
class MessageLoop;
class RefCountedMemory;
}

namespace net {
class URLRequest;
}

namespace content {
class BrowserContext;

// A URLDataSource is an object that can answer requests for WebUI data
// asynchronously. An implementation of URLDataSource should handle calls to
// StartDataRequest() by starting its (implementation-specific) asynchronous
// request for the data, then running the callback given in that method to
// notify.
class CONTENT_EXPORT URLDataSource {
 public:
  // Adds a URL data source to |browser_context|.
  static void Add(BrowserContext* browser_context, URLDataSource* source);

  virtual ~URLDataSource() {}

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source. For
  // sources where a scheme is used instead of the hostname as the unique
  // identifier, the suffix "://" must be added to the return value, eg. for a
  // URLDataSource which would display resources with URLs on the form
  // your-scheme://anything , GetSource() must return "your-scheme://".
  virtual std::string GetSource() const = 0;

  // Used by StartDataRequest so that the child class can return the data when
  // it's available.
  typedef base::Callback<void(base::RefCountedMemory*)> GotDataCallback;

  // Called by URLDataSource to request data at |path|. The string parameter is
  // the path of the request. The child class should run |callback| when the
  // data is available or if the request could not be satisfied. This can be
  // called either in this callback or asynchronously with the response.
  virtual void StartDataRequest(const std::string& path,
                                int render_process_id,
                                int render_frame_id,
                                const GotDataCallback& callback) = 0;

  // Return the mimetype that should be sent with this response, or empty
  // string to specify no mime type.
  virtual std::string GetMimeType(const std::string& path) const = 0;

  // The following methods are all called on the IO thread.

  // Returns the MessageLoop on which the delegate wishes to have
  // StartDataRequest called to handle the request for |path|. The default
  // implementation returns BrowserThread::UI. If the delegate does not care
  // which thread StartDataRequest is called on, this should return NULL. It may
  // be beneficial to return NULL for requests that are safe to handle directly
  // on the IO thread.  This can improve performance by satisfying such requests
  // more rapidly when there is a large amount of UI thread contention. Or the
  // delegate can return a specific thread's Messageloop if they wish.
  virtual base::MessageLoop* MessageLoopForRequestPath(
      const std::string& path) const;

  // Returns true if the URLDataSource should replace an existing URLDataSource
  // with the same name that has already been registered. The default is true.
  //
  // WARNING: this is invoked on the IO thread.
  //
  // TODO: nuke this and convert all callers to not replace.
  virtual bool ShouldReplaceExistingSource() const;

  // Returns true if responses from this URLDataSource can be cached.
  virtual bool AllowCaching() const;

  // If you are overriding this, then you have a bug.
  // It is not acceptable to disable content-security-policy on chrome:// pages
  // to permit functionality excluded by CSP, such as inline script.
  // Instead, you must go back and change your WebUI page so that it is
  // compliant with the policy. This typically involves ensuring that all script
  // is delivered through the data manager backend. Talk to tsepez for more
  // info.
  virtual bool ShouldAddContentSecurityPolicy() const;

  // It is OK to override the following two methods to a custom CSP directive
  // thereby slightly reducing the protection applied to the page.

  // By default, "object-src 'none';" is added to CSP. Override to change this.
  virtual std::string GetContentSecurityPolicyObjectSrc() const;
  // By default, "frame-src 'none';" is added to CSP. Override to change this.
  virtual std::string GetContentSecurityPolicyFrameSrc() const;

  // By default, the "X-Frame-Options: DENY" header is sent. To stop this from
  // happening, return false. It is OK to return false as needed.
  virtual bool ShouldDenyXFrameOptions() const;

  // By default, only chrome: and chrome-devtools: requests are allowed.
  // Override in specific WebUI data sources to enable for additional schemes or
  // to implement fancier access control.  Typically used in concert with
  // ContentBrowserClient::GetAdditionalWebUISchemes() to permit additional
  // WebUI scheme support for an embedder.
  virtual bool ShouldServiceRequest(const net::URLRequest* request) const;

  // By default, Content-Type: header is not sent along with the response.
  // To start sending mime type returned by GetMimeType in HTTP headers,
  // return true. It is useful when tunneling response served from this data
  // source programmatically. Or when AppCache is enabled for this source as it
  // is for chrome-devtools.
  virtual bool ShouldServeMimeTypeAsContentTypeHeader() const;

  // Called to inform the source that StartDataRequest() will be called soon.
  // Gives the source an opportunity to rewrite |path| to incorporate extra
  // information from the URLRequest prior to serving.
  virtual void WillServiceRequest(
      const net::URLRequest* request,
      std::string* path) const {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_H_
