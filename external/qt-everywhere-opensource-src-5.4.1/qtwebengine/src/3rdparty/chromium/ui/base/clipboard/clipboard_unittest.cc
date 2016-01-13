// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "ui/base/clipboard/clipboard_util_win.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#endif

#if defined(USE_AURA)
#include "ui/events/platform/platform_event_source.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;

namespace ui {

class ClipboardTest : public PlatformTest {
 public:
#if defined(USE_AURA)
  ClipboardTest() : event_source_(ui::PlatformEventSource::CreateDefault()) {}
#else
  ClipboardTest() {}
#endif

  static void WriteObjectsToClipboard(ui::Clipboard* clipboard,
                                      const Clipboard::ObjectMap& objects) {
    clipboard->WriteObjects(ui::CLIPBOARD_TYPE_COPY_PASTE, objects);
  }

 protected:
  Clipboard& clipboard() { return clipboard_; }

  void WriteObjectsToClipboard(const Clipboard::ObjectMap& objects) {
    WriteObjectsToClipboard(&clipboard(), objects);
  }

 private:
  base::MessageLoopForUI message_loop_;
#if defined(USE_AURA)
  scoped_ptr<PlatformEventSource> event_source_;
#endif
  Clipboard clipboard_;
};

namespace {

bool MarkupMatches(const base::string16& expected_markup,
                   const base::string16& actual_markup) {
  return actual_markup.find(expected_markup) != base::string16::npos;
}

}  // namespace

TEST_F(ClipboardTest, ClearTest) {
  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(ASCIIToUTF16("clear me"));
  }

  clipboard().Clear(CLIPBOARD_TYPE_COPY_PASTE);

  EXPECT_FALSE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_FALSE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextFormatType(), CLIPBOARD_TYPE_COPY_PASTE));
}

TEST_F(ClipboardTest, TextTest) {
  base::string16 text(ASCIIToUTF16("This is a base::string16!#$")), text_result;
  std::string ascii_text;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(text);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetPlainTextFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  clipboard().ReadText(CLIPBOARD_TYPE_COPY_PASTE, &text_result);

  EXPECT_EQ(text, text_result);
  clipboard().ReadAsciiText(CLIPBOARD_TYPE_COPY_PASTE, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TEST_F(ClipboardTest, HTMLTest) {
  base::string16 markup(ASCIIToUTF16("<string>Hi!</string>")), markup_result;
  base::string16 plain(ASCIIToUTF16("Hi!")), plain_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(plain);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  uint32 ignored;
  clipboard().ReadHTML(CLIPBOARD_TYPE_COPY_PASTE, &markup_result, &url_result,
                     &ignored, &ignored);
  EXPECT_PRED2(MarkupMatches, markup, markup_result);
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
}

TEST_F(ClipboardTest, RTFTest) {
  std::string rtf =
      "{\\rtf1\\ansi{\\fonttbl\\f0\\fswiss Helvetica;}\\f0\\pard\n"
      "This is some {\\b bold} text.\\par\n"
      "}";

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteRTF(rtf);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetRtfFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  std::string result;
  clipboard().ReadRTF(CLIPBOARD_TYPE_COPY_PASTE, &result);
  EXPECT_EQ(rtf, result);
}

// TODO(dnicoara) Enable test once Ozone implements clipboard support:
// crbug.com/361707
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(USE_OZONE)
TEST_F(ClipboardTest, MultipleBufferTest) {
  base::string16 text(ASCIIToUTF16("Standard")), text_result;
  base::string16 markup(ASCIIToUTF16("<string>Selection</string>"));
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(text);
  }

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_SELECTION);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetPlainTextFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_FALSE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextFormatType(),
      CLIPBOARD_TYPE_SELECTION));

  EXPECT_FALSE(clipboard().IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                             CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                            CLIPBOARD_TYPE_SELECTION));

  clipboard().ReadText(CLIPBOARD_TYPE_COPY_PASTE, &text_result);
  EXPECT_EQ(text, text_result);

  uint32 ignored;
  base::string16 markup_result;
  clipboard().ReadHTML(CLIPBOARD_TYPE_SELECTION,
                       &markup_result,
                       &url_result,
                       &ignored,
                       &ignored);
  EXPECT_PRED2(MarkupMatches, markup, markup_result);
}
#endif

TEST_F(ClipboardTest, TrickyHTMLTest) {
  base::string16 markup(ASCIIToUTF16("<em>Bye!<!--EndFragment --></em>")),
      markup_result;
  std::string url, url_result;
  base::string16 plain(ASCIIToUTF16("Bye!")), plain_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(plain);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  uint32 ignored;
  clipboard().ReadHTML(CLIPBOARD_TYPE_COPY_PASTE, &markup_result, &url_result,
                       &ignored, &ignored);
  EXPECT_PRED2(MarkupMatches, markup, markup_result);
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
}

#if defined(OS_WIN)
TEST_F(ClipboardTest, UniodeHTMLTest) {
  base::string16 markup(UTF8ToUTF16("<div>A \xc3\xb8 \xe6\xb0\xb4</div>")),
      markup_result;
  std::string url, url_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  uint32 fragment_start;
  uint32 fragment_end;
  clipboard().ReadHTML(CLIPBOARD_TYPE_COPY_PASTE, &markup_result, &url_result,
                       &fragment_start, &fragment_end);
  EXPECT_PRED2(MarkupMatches, markup, markup_result);
  EXPECT_EQ(url, url_result);
  // Make sure that fragment indices were adjusted when converting.
  EXPECT_EQ(36, fragment_start);
  EXPECT_EQ(52, fragment_end);
}
#endif  // defined(OS_WIN)

// TODO(estade): Port the following test (decide what target we use for urls)
#if !defined(OS_POSIX) || defined(OS_MACOSX)
TEST_F(ClipboardTest, BookmarkTest) {
  base::string16 title(ASCIIToUTF16("The Example Company")), title_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteBookmark(title, url);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetUrlWFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  clipboard().ReadBookmark(&title_result, &url_result);
  EXPECT_EQ(title, title_result);
  EXPECT_EQ(url, url_result);
}
#endif  // defined(OS_WIN)

TEST_F(ClipboardTest, MultiFormatTest) {
  base::string16 text(ASCIIToUTF16("Hi!")), text_result;
  base::string16 markup(ASCIIToUTF16("<strong>Hi!</string>")), markup_result;
  std::string url("http://www.example.com/"), url_result;
  std::string ascii_text;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteHTML(markup, url);
    clipboard_writer.WriteText(text);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextFormatType(), CLIPBOARD_TYPE_COPY_PASTE));
  uint32 ignored;
  clipboard().ReadHTML(CLIPBOARD_TYPE_COPY_PASTE, &markup_result, &url_result,
                       &ignored, &ignored);
  EXPECT_PRED2(MarkupMatches, markup, markup_result);
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
  clipboard().ReadText(CLIPBOARD_TYPE_COPY_PASTE, &text_result);
  EXPECT_EQ(text, text_result);
  clipboard().ReadAsciiText(CLIPBOARD_TYPE_COPY_PASTE, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TEST_F(ClipboardTest, URLTest) {
  base::string16 url(ASCIIToUTF16("http://www.google.com/"));

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteURL(url);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetPlainTextFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  base::string16 text_result;
  clipboard().ReadText(CLIPBOARD_TYPE_COPY_PASTE, &text_result);

  EXPECT_EQ(text_result, url);

  std::string ascii_text;
  clipboard().ReadAsciiText(CLIPBOARD_TYPE_COPY_PASTE, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  ascii_text.clear();
  clipboard().ReadAsciiText(CLIPBOARD_TYPE_SELECTION, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);
#endif
}

// TODO(dcheng): The tests for copying to the clipboard also test IPC
// interaction... consider moving them to a different layer so we can
// consolidate the validation logic.
// Note that |bitmap_data| is not premultiplied!
static void TestBitmapWrite(Clipboard* clipboard,
                            const uint32* bitmap_data,
                            size_t bitmap_data_size,
                            const gfx::Size& size) {
  // Create shared memory region.
  base::SharedMemory shared_buf;
  ASSERT_TRUE(shared_buf.CreateAndMapAnonymous(bitmap_data_size));
  memcpy(shared_buf.memory(), bitmap_data, bitmap_data_size);
  // CBF_SMBITMAP expects premultiplied bitmap data so do that now.
  uint32* pixel_buffer = static_cast<uint32*>(shared_buf.memory());
  for (int j = 0; j < size.height(); ++j) {
    for (int i = 0; i < size.width(); ++i) {
      uint32& pixel = pixel_buffer[i + j * size.width()];
      pixel = SkPreMultiplyColor(pixel);
    }
  }
  base::SharedMemoryHandle handle_to_share;
  base::ProcessHandle current_process = base::kNullProcessHandle;
#if defined(OS_WIN)
  current_process = GetCurrentProcess();
#endif
  shared_buf.ShareToProcess(current_process, &handle_to_share);
  ASSERT_TRUE(shared_buf.Unmap());

  // Setup data for clipboard().
  Clipboard::ObjectMapParam placeholder_param;
  Clipboard::ObjectMapParam size_param;
  const char* size_data = reinterpret_cast<const char*>(&size);
  for (size_t i = 0; i < sizeof(size); ++i)
    size_param.push_back(size_data[i]);

  Clipboard::ObjectMapParams params;
  params.push_back(placeholder_param);
  params.push_back(size_param);

  Clipboard::ObjectMap objects;
  objects[Clipboard::CBF_SMBITMAP] = params;
  ASSERT_TRUE(Clipboard::ReplaceSharedMemHandle(
      &objects, handle_to_share, current_process));

  ClipboardTest::WriteObjectsToClipboard(clipboard, objects);

  EXPECT_TRUE(clipboard->IsFormatAvailable(Clipboard::GetBitmapFormatType(),
                                           CLIPBOARD_TYPE_COPY_PASTE));
  const SkBitmap& image = clipboard->ReadImage(CLIPBOARD_TYPE_COPY_PASTE);
  EXPECT_EQ(size, gfx::Size(image.width(), image.height()));
  SkAutoLockPixels image_lock(image);
  for (int j = 0; j < image.height(); ++j) {
    const uint32* row_address = image.getAddr32(0, j);
    for (int i = 0; i < image.width(); ++i) {
      int offset = i + j * image.width();
      uint32 pixel = SkPreMultiplyColor(bitmap_data[offset]);
      EXPECT_EQ(pixel, row_address[i])
          << "i = " << i << ", j = " << j;
    }
  }
}

TEST_F(ClipboardTest, SharedBitmapTest) {
  const uint32 fake_bitmap_1[] = {
    0x46155189, 0xF6A55C8D, 0x79845674, 0xFA57BD89,
    0x78FD46AE, 0x87C64F5A, 0x36EDC5AF, 0x4378F568,
    0x91E9F63A, 0xC31EA14F, 0x69AB32DF, 0x643A3FD1,
  };
  {
    SCOPED_TRACE("first bitmap");
    TestBitmapWrite(
        &clipboard(), fake_bitmap_1, sizeof(fake_bitmap_1), gfx::Size(4, 3));
  }

  const uint32 fake_bitmap_2[] = {
    0x46155189, 0xF6A55C8D,
    0x79845674, 0xFA57BD89,
    0x78FD46AE, 0x87C64F5A,
    0x36EDC5AF, 0x4378F568,
    0x91E9F63A, 0xC31EA14F,
    0x69AB32DF, 0x643A3FD1,
    0xA6DF041D, 0x83046278,
  };
  {
    SCOPED_TRACE("second bitmap");
    TestBitmapWrite(
        &clipboard(), fake_bitmap_2, sizeof(fake_bitmap_2), gfx::Size(2, 7));
  }
}

namespace {
// A size class that just happens to have the same layout as gfx::Size!
struct UnsafeSize {
  int width;
  int height;
};
COMPILE_ASSERT(sizeof(UnsafeSize) == sizeof(gfx::Size),
               UnsafeSize_must_be_same_size_as_gfx_Size);
}  // namespace

TEST_F(ClipboardTest, SharedBitmapWithTwoNegativeSizes) {
  Clipboard::ObjectMapParam placeholder_param;
  void* crash_me = reinterpret_cast<void*>(57);
  placeholder_param.resize(sizeof(crash_me));
  memcpy(&placeholder_param.front(), &crash_me, sizeof(crash_me));

  Clipboard::ObjectMapParam size_param;
  UnsafeSize size = {-100, -100};
  size_param.resize(sizeof(size));
  memcpy(&size_param.front(), &size, sizeof(size));

  Clipboard::ObjectMapParams params;
  params.push_back(placeholder_param);
  params.push_back(size_param);

  Clipboard::ObjectMap objects;
  objects[Clipboard::CBF_SMBITMAP] = params;

  WriteObjectsToClipboard(objects);
  EXPECT_FALSE(clipboard().IsFormatAvailable(Clipboard::GetBitmapFormatType(),
                                             CLIPBOARD_TYPE_COPY_PASTE));
}

TEST_F(ClipboardTest, SharedBitmapWithOneNegativeSize) {
  Clipboard::ObjectMapParam placeholder_param;
  void* crash_me = reinterpret_cast<void*>(57);
  placeholder_param.resize(sizeof(crash_me));
  memcpy(&placeholder_param.front(), &crash_me, sizeof(crash_me));

  Clipboard::ObjectMapParam size_param;
  UnsafeSize size = {-100, 100};
  size_param.resize(sizeof(size));
  memcpy(&size_param.front(), &size, sizeof(size));

  Clipboard::ObjectMapParams params;
  params.push_back(placeholder_param);
  params.push_back(size_param);

  Clipboard::ObjectMap objects;
  objects[Clipboard::CBF_SMBITMAP] = params;

  WriteObjectsToClipboard(objects);
  EXPECT_FALSE(clipboard().IsFormatAvailable(Clipboard::GetBitmapFormatType(),
                                             CLIPBOARD_TYPE_COPY_PASTE));
}

TEST_F(ClipboardTest, BitmapWithSuperSize) {
  Clipboard::ObjectMapParam placeholder_param;
  void* crash_me = reinterpret_cast<void*>(57);
  placeholder_param.resize(sizeof(crash_me));
  memcpy(&placeholder_param.front(), &crash_me, sizeof(crash_me));

  Clipboard::ObjectMapParam size_param;
  // Width just big enough that bytes per row won't fit in a 32-bit
  // representation.
  gfx::Size size(0x20000000, 1);
  size_param.resize(sizeof(size));
  memcpy(&size_param.front(), &size, sizeof(size));

  Clipboard::ObjectMapParams params;
  params.push_back(placeholder_param);
  params.push_back(size_param);

  Clipboard::ObjectMap objects;
  objects[Clipboard::CBF_SMBITMAP] = params;

  WriteObjectsToClipboard(objects);
  EXPECT_FALSE(clipboard().IsFormatAvailable(Clipboard::GetBitmapFormatType(),
                                             CLIPBOARD_TYPE_COPY_PASTE));
}

TEST_F(ClipboardTest, BitmapWithSuperSize2) {
  Clipboard::ObjectMapParam placeholder_param;
  void* crash_me = reinterpret_cast<void*>(57);
  placeholder_param.resize(sizeof(crash_me));
  memcpy(&placeholder_param.front(), &crash_me, sizeof(crash_me));

  Clipboard::ObjectMapParam size_param;
  // Width and height large enough that SkBitmap::getSize() will be truncated.
  gfx::Size size(0x0fffffff, 0x0fffffff);
  size_param.resize(sizeof(size));
  memcpy(&size_param.front(), &size, sizeof(size));

  Clipboard::ObjectMapParams params;
  params.push_back(placeholder_param);
  params.push_back(size_param);

  Clipboard::ObjectMap objects;
  objects[Clipboard::CBF_SMBITMAP] = params;

  WriteObjectsToClipboard(objects);
  EXPECT_FALSE(clipboard().IsFormatAvailable(Clipboard::GetBitmapFormatType(),
                                             CLIPBOARD_TYPE_COPY_PASTE));
}

TEST_F(ClipboardTest, DataTest) {
  const ui::Clipboard::FormatType kFormat =
      ui::Clipboard::GetFormatType("chromium/x-test-format");
  std::string payload("test string");
  Pickle write_pickle;
  write_pickle.WriteString(payload);

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WritePickledData(write_pickle, kFormat);
  }

  ASSERT_TRUE(clipboard().IsFormatAvailable(
      kFormat, CLIPBOARD_TYPE_COPY_PASTE));
  std::string output;
  clipboard().ReadData(kFormat, &output);
  ASSERT_FALSE(output.empty());

  Pickle read_pickle(output.data(), output.size());
  PickleIterator iter(read_pickle);
  std::string unpickled_string;
  ASSERT_TRUE(read_pickle.ReadString(&iter, &unpickled_string));
  EXPECT_EQ(payload, unpickled_string);
}

TEST_F(ClipboardTest, MultipleDataTest) {
  const ui::Clipboard::FormatType kFormat1 =
      ui::Clipboard::GetFormatType("chromium/x-test-format1");
  std::string payload1("test string1");
  Pickle write_pickle1;
  write_pickle1.WriteString(payload1);

  const ui::Clipboard::FormatType kFormat2 =
      ui::Clipboard::GetFormatType("chromium/x-test-format2");
  std::string payload2("test string2");
  Pickle write_pickle2;
  write_pickle2.WriteString(payload2);

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WritePickledData(write_pickle1, kFormat1);
    // overwrite the previous pickle for fun
    clipboard_writer.WritePickledData(write_pickle2, kFormat2);
  }

  ASSERT_TRUE(clipboard().IsFormatAvailable(
      kFormat2, CLIPBOARD_TYPE_COPY_PASTE));

  // Check string 2.
  std::string output2;
  clipboard().ReadData(kFormat2, &output2);
  ASSERT_FALSE(output2.empty());

  Pickle read_pickle2(output2.data(), output2.size());
  PickleIterator iter2(read_pickle2);
  std::string unpickled_string2;
  ASSERT_TRUE(read_pickle2.ReadString(&iter2, &unpickled_string2));
  EXPECT_EQ(payload2, unpickled_string2);

  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WritePickledData(write_pickle2, kFormat2);
    // overwrite the previous pickle for fun
    clipboard_writer.WritePickledData(write_pickle1, kFormat1);
  }

  ASSERT_TRUE(clipboard().IsFormatAvailable(
      kFormat1, CLIPBOARD_TYPE_COPY_PASTE));

  // Check string 1.
  std::string output1;
  clipboard().ReadData(kFormat1, &output1);
  ASSERT_FALSE(output1.empty());

  Pickle read_pickle1(output1.data(), output1.size());
  PickleIterator iter1(read_pickle1);
  std::string unpickled_string1;
  ASSERT_TRUE(read_pickle1.ReadString(&iter1, &unpickled_string1));
  EXPECT_EQ(payload1, unpickled_string1);
}

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
TEST_F(ClipboardTest, HyperlinkTest) {
  const std::string kTitle("The <Example> Company's \"home page\"");
  const std::string kUrl("http://www.example.com?x=3&lt=3#\"'<>");
  const std::string kExpectedHtml(
      "<a href=\"http://www.example.com?x=3&amp;lt=3#&quot;&#39;&lt;&gt;\">"
      "The &lt;Example&gt; Company&#39;s &quot;home page&quot;</a>");

  std::string url_result;
  base::string16 html_result;
  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteHyperlink(ASCIIToUTF16(kTitle), kUrl);
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                            CLIPBOARD_TYPE_COPY_PASTE));
  uint32 ignored;
  clipboard().ReadHTML(CLIPBOARD_TYPE_COPY_PASTE, &html_result, &url_result,
                       &ignored, &ignored);
  EXPECT_PRED2(MarkupMatches, ASCIIToUTF16(kExpectedHtml), html_result);
}
#endif

#if defined(OS_WIN)  // Windows only tests.
TEST_F(ClipboardTest, WebSmartPasteTest) {
  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteWebSmartPaste();
  }

  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetWebKitSmartPasteFormatType(), CLIPBOARD_TYPE_COPY_PASTE));
}

void HtmlTestHelper(const std::string& cf_html,
                    const std::string& expected_html) {
  std::string html;
  ClipboardUtil::CFHtmlToHtml(cf_html, &html, NULL);
  EXPECT_EQ(html, expected_html);
}

TEST_F(ClipboardTest, HtmlTest) {
  // Test converting from CF_HTML format data with <!--StartFragment--> and
  // <!--EndFragment--> comments, like from MS Word.
  HtmlTestHelper("Version:1.0\r\n"
                 "StartHTML:0000000105\r\n"
                 "EndHTML:0000000199\r\n"
                 "StartFragment:0000000123\r\n"
                 "EndFragment:0000000161\r\n"
                 "\r\n"
                 "<html>\r\n"
                 "<body>\r\n"
                 "<!--StartFragment-->\r\n"
                 "\r\n"
                 "<p>Foo</p>\r\n"
                 "\r\n"
                 "<!--EndFragment-->\r\n"
                 "</body>\r\n"
                 "</html>\r\n\r\n",
                 "<p>Foo</p>");

  // Test converting from CF_HTML format data without <!--StartFragment--> and
  // <!--EndFragment--> comments, like from OpenOffice Writer.
  HtmlTestHelper("Version:1.0\r\n"
                 "StartHTML:0000000105\r\n"
                 "EndHTML:0000000151\r\n"
                 "StartFragment:0000000121\r\n"
                 "EndFragment:0000000131\r\n"
                 "<html>\r\n"
                 "<body>\r\n"
                 "<p>Foo</p>\r\n"
                 "</body>\r\n"
                 "</html>\r\n\r\n",
                 "<p>Foo</p>");
}
#endif  // defined(OS_WIN)

// Test writing all formats we have simultaneously.
TEST_F(ClipboardTest, WriteEverything) {
  {
    ScopedClipboardWriter writer(&clipboard(), CLIPBOARD_TYPE_COPY_PASTE);
    writer.WriteText(UTF8ToUTF16("foo"));
    writer.WriteURL(UTF8ToUTF16("foo"));
    writer.WriteHTML(UTF8ToUTF16("foo"), "bar");
    writer.WriteBookmark(UTF8ToUTF16("foo"), "bar");
    writer.WriteHyperlink(ASCIIToUTF16("foo"), "bar");
    writer.WriteWebSmartPaste();
    // Left out: WriteFile, WriteFiles, WriteBitmapFromPixels, WritePickledData.
  }

  // Passes if we don't crash.
}

// TODO(dcheng): Fix this test for Android. It's rather involved, since the
// clipboard change listener is posted to the Java message loop, and spinning
// that loop from C++ to trigger the callback in the test requires a non-trivial
// amount of additional work.
#if !defined(OS_ANDROID)
// Simple test that the sequence number appears to change when the clipboard is
// written to.
// TODO(dcheng): Add a version to test CLIPBOARD_TYPE_SELECTION.
TEST_F(ClipboardTest, GetSequenceNumber) {
  const uint64 first_sequence_number =
      clipboard().GetSequenceNumber(CLIPBOARD_TYPE_COPY_PASTE);

  {
    ScopedClipboardWriter writer(&clipboard(), CLIPBOARD_TYPE_COPY_PASTE);
    writer.WriteText(UTF8ToUTF16("World"));
  }

  // On some platforms, the sequence number is updated by a UI callback so pump
  // the message loop to make sure we get the notification.
  base::RunLoop().RunUntilIdle();

  const uint64 second_sequence_number =
      clipboard().GetSequenceNumber(CLIPBOARD_TYPE_COPY_PASTE);

  EXPECT_NE(first_sequence_number, second_sequence_number);
}
#endif

#if defined(OS_ANDROID)

// Test that if another application writes some text to the pasteboard the
// clipboard properly invalidates other types.
TEST_F(ClipboardTest, InternalClipboardInvalidation) {
  // Write a Webkit smart paste tag to our clipboard.
  {
    ScopedClipboardWriter clipboard_writer(&clipboard(),
                                           CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteWebSmartPaste();
  }
  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetWebKitSmartPasteFormatType(), CLIPBOARD_TYPE_COPY_PASTE));

  //
  // Simulate that another application copied something in the Clipboard
  //
  std::string new_value("Some text copied by some other app");
  using base::android::ConvertUTF8ToJavaString;
  using base::android::MethodID;
  using base::android::ScopedJavaLocalRef;

  JNIEnv* env = base::android::AttachCurrentThread();
  ASSERT_TRUE(env);

  jobject context = base::android::GetApplicationContext();
  ASSERT_TRUE(context);

  ScopedJavaLocalRef<jclass> context_class =
      base::android::GetClass(env, "android/content/Context");

  jmethodID get_system_service = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, context_class.obj(), "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;");

  // Retrieve the system service.
  ScopedJavaLocalRef<jstring> service_name = ConvertUTF8ToJavaString(
      env, "clipboard");
  ScopedJavaLocalRef<jobject> clipboard_manager(
      env, env->CallObjectMethod(
        context, get_system_service, service_name.obj()));
  ASSERT_TRUE(clipboard_manager.obj() && !base::android::ClearException(env));

  ScopedJavaLocalRef<jclass> clipboard_class =
      base::android::GetClass(env, "android/text/ClipboardManager");
  jmethodID set_text = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, clipboard_class.obj(), "setText", "(Ljava/lang/CharSequence;)V");
  ScopedJavaLocalRef<jstring> new_value_string = ConvertUTF8ToJavaString(
      env, new_value.c_str());

  // Will need to call toString as CharSequence is not always a String.
  env->CallVoidMethod(clipboard_manager.obj(),
                      set_text,
                      new_value_string.obj());

  // The WebKit smart paste tag should now be gone.
  EXPECT_FALSE(clipboard().IsFormatAvailable(
      Clipboard::GetWebKitSmartPasteFormatType(), CLIPBOARD_TYPE_COPY_PASTE));

  // Make sure some text is available
  EXPECT_TRUE(clipboard().IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), CLIPBOARD_TYPE_COPY_PASTE));

  // Make sure the text is what we inserted while simulating the other app
  std::string contents;
  clipboard().ReadAsciiText(CLIPBOARD_TYPE_COPY_PASTE, &contents);
  EXPECT_EQ(contents, new_value);
}
#endif
}  // namespace ui
