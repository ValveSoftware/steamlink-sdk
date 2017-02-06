// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <stddef.h>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace variations {

TEST(VariationsHttpHeadersTest, ShouldAppendHeaders) {
  struct {
    const char* url;
    bool should_append_headers;
  } cases[] = {
      {"http://google.com", true},
      {"http://www.google.com", true},
      {"http://m.google.com", true},
      {"http://google.ca", true},
      {"https://google.ca", true},
      {"http://google.co.uk", true},
      {"http://google.co.uk:8080/", true},
      {"http://www.google.co.uk:8080/", true},
      {"http://google", false},

      {"http://youtube.com", true},
      {"http://www.youtube.com", true},
      {"http://www.youtube.ca", true},
      {"http://www.youtube.co.uk:8080/", true},
      {"https://www.youtube.com", true},
      {"http://youtube", false},

      {"http://www.yahoo.com", false},

      {"http://ad.doubleclick.net", true},
      {"https://a.b.c.doubleclick.net", true},
      {"https://a.b.c.doubleclick.net:8081", true},
      {"http://www.doubleclick.com", true},
      {"http://www.doubleclick.org", false},
      {"http://www.doubleclick.net.com", false},
      {"https://www.doubleclick.net.com", false},

      {"http://ad.googlesyndication.com", true},
      {"https://a.b.c.googlesyndication.com", true},
      {"https://a.b.c.googlesyndication.com:8080", true},
      {"http://www.doubleclick.edu", false},
      {"http://www.googlesyndication.com.edu", false},
      {"https://www.googlesyndication.com.com", false},

      {"http://www.googleadservices.com", true},
      {"http://www.googleadservices.com:8080", true},
      {"https://www.googleadservices.com", true},
      {"https://www.internal.googleadservices.com", true},
      {"https://www2.googleadservices.com", true},
      {"https://www.googleadservices.org", false},
      {"https://www.googleadservices.com.co.uk", false},

      {"http://WWW.ANDROID.COM", true},
      {"http://www.android.com", true},
      {"http://www.doubleclick.com", true},
      {"http://www.doubleclick.net", true},
      {"http://www.ggpht.com", true},
      {"http://www.googleadservices.com", true},
      {"http://www.googleapis.com", true},
      {"http://www.googlesyndication.com", true},
      {"http://www.googleusercontent.com", true},
      {"http://www.googlevideo.com", true},
      {"http://ssl.gstatic.com", true},
      {"http://www.gstatic.com", true},
      {"http://www.ytimg.com", true},
      {"http://wwwytimg.com", false},
      {"http://ytimg.com", false},

      {"http://www.android.org", false},
      {"http://www.doubleclick.org", false},
      {"http://www.doubleclick.net", true},
      {"http://www.ggpht.org", false},
      {"http://www.googleadservices.org", false},
      {"http://www.googleapis.org", false},
      {"http://www.googlesyndication.org", false},
      {"http://www.googleusercontent.org", false},
      {"http://www.googlevideo.org", false},
      {"http://ssl.gstatic.org", false},
      {"http://www.gstatic.org", false},
      {"http://www.ytimg.org", false},

      {"http://a.b.android.com", true},
      {"http://a.b.doubleclick.com", true},
      {"http://a.b.doubleclick.net", true},
      {"http://a.b.ggpht.com", true},
      {"http://a.b.googleadservices.com", true},
      {"http://a.b.googleapis.com", true},
      {"http://a.b.googlesyndication.com", true},
      {"http://a.b.googleusercontent.com", true},
      {"http://a.b.googlevideo.com", true},
      {"http://ssl.gstatic.com", true},
      {"http://a.b.gstatic.com", true},
      {"http://a.b.ytimg.com", true},
      {"http://googleweblight.com", true},
      {"https://googleweblight.com", true},
      {"http://wwwgoogleweblight.com", false},
      {"http://www.googleweblight.com", false},
      {"http://a.b.googleweblight.com", false},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    const GURL url(cases[i].url);
    EXPECT_EQ(cases[i].should_append_headers,
              internal::ShouldAppendVariationHeaders(url))
        << url;
  }
}

}  // namespace variations
