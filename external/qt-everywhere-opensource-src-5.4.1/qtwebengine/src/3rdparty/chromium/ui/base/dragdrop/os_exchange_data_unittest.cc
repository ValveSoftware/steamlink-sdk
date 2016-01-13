// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/platform/platform_event_source.h"
#include "url/gurl.h"

namespace ui {

class OSExchangeDataTest : public PlatformTest {
 public:
  OSExchangeDataTest()
      : event_source_(ui::PlatformEventSource::CreateDefault()) {}

 private:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<PlatformEventSource> event_source_;
};

TEST_F(OSExchangeDataTest, StringDataGetAndSet) {
  OSExchangeData data;
  base::string16 input = base::ASCIIToUTF16("I can has cheezburger?");
  data.SetString(input);

  OSExchangeData data2(data.provider().Clone());
  base::string16 output;
  EXPECT_TRUE(data2.GetString(&output));
  EXPECT_EQ(input, output);
  std::string url_spec = "http://www.goats.com/";
  GURL url(url_spec);
  base::string16 title;
  EXPECT_FALSE(
      data2.GetURLAndTitle(OSExchangeData::CONVERT_FILENAMES, &url, &title));
  // No URLs in |data|, so url should be untouched.
  EXPECT_EQ(url_spec, url.spec());
}

TEST_F(OSExchangeDataTest, TestURLExchangeFormats) {
  OSExchangeData data;
  std::string url_spec = "http://www.google.com/";
  GURL url(url_spec);
  base::string16 url_title = base::ASCIIToUTF16("www.google.com");
  data.SetURL(url, url_title);
  base::string16 output;

  OSExchangeData data2(data.provider().Clone());

  // URL spec and title should match
  GURL output_url;
  base::string16 output_title;
  EXPECT_TRUE(data2.GetURLAndTitle(
      OSExchangeData::CONVERT_FILENAMES, &output_url, &output_title));
  EXPECT_EQ(url_spec, output_url.spec());
  EXPECT_EQ(url_title, output_title);
  base::string16 output_string;

  // URL should be the raw text response
  EXPECT_TRUE(data2.GetString(&output_string));
  EXPECT_EQ(url_spec, base::UTF16ToUTF8(output_string));
}

TEST_F(OSExchangeDataTest, TestPickledData) {
  const OSExchangeData::CustomFormat kTestFormat =
      ui::Clipboard::GetFormatType("application/vnd.chromium.test");

  Pickle saved_pickle;
  saved_pickle.WriteInt(1);
  saved_pickle.WriteInt(2);
  OSExchangeData data;
  data.SetPickledData(kTestFormat, saved_pickle);

  OSExchangeData copy(data.provider().Clone());
  EXPECT_TRUE(copy.HasCustomFormat(kTestFormat));

  Pickle restored_pickle;
  EXPECT_TRUE(copy.GetPickledData(kTestFormat, &restored_pickle));
  PickleIterator iterator(restored_pickle);
  int value;
  EXPECT_TRUE(restored_pickle.ReadInt(&iterator, &value));
  EXPECT_EQ(1, value);
  EXPECT_TRUE(restored_pickle.ReadInt(&iterator, &value));
  EXPECT_EQ(2, value);
}

#if defined(USE_AURA)
TEST_F(OSExchangeDataTest, TestHTML) {
  OSExchangeData data;
  GURL url("http://www.google.com/");
  base::string16 html = base::ASCIIToUTF16(
      "<HTML>\n<BODY>\n"
      "<b>bold.</b> <i><b>This is bold italic.</b></i>\n"
      "</BODY>\n</HTML>");
  data.SetHtml(html, url);

  OSExchangeData copy(data.provider().Clone());
  base::string16 read_html;
  EXPECT_TRUE(copy.GetHtml(&read_html, &url));
  EXPECT_EQ(html, read_html);
}
#endif

}  // namespace ui
