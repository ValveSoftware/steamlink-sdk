// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_URL_UTILITIES_H_
#define NET_TOOLS_DUMP_CACHE_URL_UTILITIES_H_

#include <string>

namespace net {

struct UrlUtilities {
  // Gets the host from an url, strips the port number as well if the url
  // has one.
  // For example: calling GetUrlHost(www.foo.com:8080/boo) returns www.foo.com
  static std::string GetUrlHost(const std::string& url);

  // Get the host + path portion of an url
  // e.g   http://www.foo.com/path
  //       returns www.foo.com/path
  static std::string GetUrlHostPath(const std::string& url);

  // Gets the path portion of an url.
  // e.g   http://www.foo.com/path
  //       returns /path
  static std::string GetUrlPath(const std::string& url);

  // Unescape a url, converting all %XX to the the actual char 0xXX.
  // For example, this will convert "foo%21bar" to "foo!bar".
  static std::string Unescape(const std::string& escaped_url);
};

}  // namespace net

#endif  // NET_TOOLS_DUMP_CACHE_URL_UTILITIES_H_
