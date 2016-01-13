// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_URL_UTIL_H_
#define NET_BASE_URL_UTIL_H_

#include <string>

#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "url/url_parse.h"

class GURL;

namespace net {

// Returns a new GURL by appending the given query parameter name and the
// value. Unsafe characters in the name and the value are escaped like
// %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendQueryParameter(GURL("http://example.com"), "name", "value").spec()
// => "http://example.com?name=value"
// AppendQueryParameter(GURL("http://example.com?x=y"), "name", "value").spec()
// => "http://example.com?x=y&name=value"
NET_EXPORT GURL AppendQueryParameter(const GURL& url,
                                     const std::string& name,
                                     const std::string& value);

// Returns a new GURL by appending or replacing the given query parameter name
// and the value. If |name| appears more than once, only the first name-value
// pair is replaced. Unsafe characters in the name and the value are escaped
// like %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com"), "name", "new").spec()
// => "http://example.com?name=value"
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com?x=y&name=old"), "name", "new").spec()
// => "http://example.com?x=y&name=new"
NET_EXPORT GURL AppendOrReplaceQueryParameter(const GURL& url,
                                              const std::string& name,
                                              const std::string& value);

// Iterates over the key-value pairs in the query portion of |url|.
class NET_EXPORT QueryIterator {
 public:
  explicit QueryIterator(const GURL& url);
  ~QueryIterator();

  std::string GetKey() const;
  std::string GetValue() const;
  const std::string& GetUnescapedValue();

  bool IsAtEnd() const;
  void Advance();

 private:
  const GURL& url_;
  url::Component query_;
  bool at_end_;
  url::Component key_;
  url::Component value_;
  std::string unescaped_value_;

  DISALLOW_COPY_AND_ASSIGN(QueryIterator);
};

// Looks for |search_key| in the query portion of |url|. Returns true if the
// key is found and sets |out_value| to the unescaped value for the key.
// Returns false if the key is not found.
NET_EXPORT bool GetValueForKeyInQuery(const GURL& url,
                                      const std::string& search_key,
                                      std::string* out_value);

}  // namespace net

#endif  // NET_BASE_URL_UTIL_H_
