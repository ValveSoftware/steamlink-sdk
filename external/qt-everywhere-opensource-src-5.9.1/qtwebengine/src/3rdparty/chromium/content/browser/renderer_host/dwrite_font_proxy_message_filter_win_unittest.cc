// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_proxy_message_filter_win.h"

#include <dwrite.h>
#include <dwrite_2.h>

#include <memory>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "content/common/dwrite_font_proxy_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/win/direct_write.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

class FilterWithFakeSender : public DWriteFontProxyMessageFilter {
 public:
  bool Send(IPC::Message* msg) override {
    EXPECT_EQ(nullptr, reply_message_.get());
    reply_message_.reset(msg);
    return true;
  }

  IPC::Message* GetReply() { return reply_message_.get(); }

  void ResetReply() { reply_message_.reset(nullptr); }

 private:
  ~FilterWithFakeSender() override = default;

  std::unique_ptr<IPC::Message> reply_message_;
};

class DWriteFontProxyMessageFilterUnitTest : public testing::Test {
 public:
  DWriteFontProxyMessageFilterUnitTest() {
    filter_ = new FilterWithFakeSender();
  }

  void Send(IPC::SyncMessage* message) {
    std::unique_ptr<IPC::SyncMessage> deleter(message);
    std::unique_ptr<IPC::MessageReplyDeserializer> serializer(
        message->GetReplyDeserializer());
    filter_->OnMessageReceived(*message);
    base::RunLoop().RunUntilIdle();
    ASSERT_NE(nullptr, filter_->GetReply());
    serializer->SerializeOutputParameters(*(filter_->GetReply()));
  }

  bool IsDWrite2Available() {
    mswr::ComPtr<IDWriteFactory> factory;
    gfx::win::CreateDWriteFactory(&factory);
    mswr::ComPtr<IDWriteFactory2> factory2;
    factory.As<IDWriteFactory2>(&factory2);

    if (!factory2.Get()) {
      // IDWriteFactory2 is expected to not be available before Win8.1
      EXPECT_LT(base::win::GetVersion(), base::win::VERSION_WIN8_1);
    }
    return factory2.Get();
  }

  scoped_refptr<FilterWithFakeSender> filter_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFamilyCount) {
  UINT32 family_count = 0;
  Send(new DWriteFontProxyMsg_GetFamilyCount(&family_count));
  EXPECT_NE(0u, family_count);  // Assume there's some fonts on the test system.
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, FindFamily) {
  UINT32 arial_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Arial", &arial_index));
  EXPECT_NE(UINT_MAX, arial_index);

  filter_->ResetReply();
  UINT32 times_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Times New Roman", &times_index));
  EXPECT_NE(UINT_MAX, times_index);
  EXPECT_NE(arial_index, times_index);

  filter_->ResetReply();
  UINT32 unknown_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Not a font family", &unknown_index));
  EXPECT_EQ(UINT_MAX, unknown_index);
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFamilyNames) {
  UINT32 arial_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Arial", &arial_index));
  filter_->ResetReply();

  std::vector<DWriteStringPair> names;
  Send(new DWriteFontProxyMsg_GetFamilyNames(arial_index, &names));

  EXPECT_LT(0u, names.size());
  for (const auto& pair : names) {
    EXPECT_STRNE(L"", pair.first.c_str());
    EXPECT_STRNE(L"", pair.second.c_str());
  }
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFamilyNamesIndexOutOfBounds) {
  std::vector<DWriteStringPair> names;
  UINT32 invalid_index = 1000000;
  Send(new DWriteFontProxyMsg_GetFamilyNames(invalid_index, &names));

  EXPECT_EQ(0u, names.size());
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFontFiles) {
  UINT32 arial_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Arial", &arial_index));
  filter_->ResetReply();

  std::vector<base::string16> files;
  std::vector<IPC::PlatformFileForTransit> handles;
  Send(new DWriteFontProxyMsg_GetFontFiles(arial_index, &files, &handles));

  EXPECT_LT(0u, files.size());
  for (const base::string16& file : files) {
    EXPECT_STRNE(L"", file.c_str());
  }
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFontFilesIndexOutOfBounds) {
  std::vector<base::string16> files;
  std::vector<IPC::PlatformFileForTransit> handles;
  UINT32 invalid_index = 1000000;
  Send(new DWriteFontProxyMsg_GetFontFiles(invalid_index, &files, &handles));

  EXPECT_EQ(0u, files.size());
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, MapCharacter) {
  if (!IsDWrite2Available())
    return;

  DWriteFontStyle font_style;
  font_style.font_weight = DWRITE_FONT_WEIGHT_NORMAL;
  font_style.font_slant = DWRITE_FONT_STYLE_NORMAL;
  font_style.font_stretch = DWRITE_FONT_STRETCH_NORMAL;

  MapCharactersResult result;
  Send(new DWriteFontProxyMsg_MapCharacters(
      L"abc", font_style, L"", DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"",
      &result));

  EXPECT_NE(UINT32_MAX, result.family_index);
  EXPECT_STRNE(L"", result.family_name.c_str());
  EXPECT_EQ(3u, result.mapped_length);
  EXPECT_NE(0.0, result.scale);
  EXPECT_NE(0, result.font_style.font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result.font_style.font_slant);
  EXPECT_NE(0, result.font_style.font_stretch);
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, MapCharacterInvalidCharacter) {
  if (!IsDWrite2Available())
    return;

  DWriteFontStyle font_style;
  font_style.font_weight = DWRITE_FONT_WEIGHT_NORMAL;
  font_style.font_slant = DWRITE_FONT_STYLE_NORMAL;
  font_style.font_stretch = DWRITE_FONT_STRETCH_NORMAL;

  MapCharactersResult result;
  Send(new DWriteFontProxyMsg_MapCharacters(
      L"\ufffe\uffffabc", font_style, L"en-us",
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result));

  EXPECT_EQ(UINT32_MAX, result.family_index);
  EXPECT_STREQ(L"", result.family_name.c_str());
  EXPECT_EQ(2u, result.mapped_length);
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, MapCharacterInvalidAfterValid) {
  if (!IsDWrite2Available())
    return;

  DWriteFontStyle font_style;
  font_style.font_weight = DWRITE_FONT_WEIGHT_NORMAL;
  font_style.font_slant = DWRITE_FONT_STYLE_NORMAL;
  font_style.font_stretch = DWRITE_FONT_STRETCH_NORMAL;

  MapCharactersResult result;
  Send(new DWriteFontProxyMsg_MapCharacters(
      L"abc\ufffe\uffff", font_style, L"en-us",
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, L"", &result));

  EXPECT_NE(UINT32_MAX, result.family_index);
  EXPECT_STRNE(L"", result.family_name.c_str());
  EXPECT_EQ(3u, result.mapped_length);
  EXPECT_NE(0.0, result.scale);
  EXPECT_NE(0, result.font_style.font_weight);
  EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, result.font_style.font_slant);
  EXPECT_NE(0, result.font_style.font_stretch);
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, TestCustomFontFiles) {
  // Override windows fonts path to force the custom font file codepath.
  filter_->SetWindowsFontsPathForTesting(L"X:\\NotWindowsFonts");
  // Set the peer process so the filter duplicates handles into the current
  // process.
  filter_->set_peer_process_for_testing(base::Process::Current());

  UINT32 arial_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Arial", &arial_index));
  filter_->ResetReply();

  std::vector<base::string16> files;
  std::vector<IPC::PlatformFileForTransit> handles;
  Send(new DWriteFontProxyMsg_GetFontFiles(arial_index, &files, &handles));

  EXPECT_TRUE(files.empty());
  EXPECT_FALSE(handles.empty());
  for (const IPC::PlatformFileForTransit& handle : handles) {
    EXPECT_TRUE(handle.IsValid());
    base::File file = IPC::PlatformFileForTransitToFile(handle);
    EXPECT_LT(0, file.GetLength());  // Check the file exists
  }
}

}  // namespace

}  // namespace content
