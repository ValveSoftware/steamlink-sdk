// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/mus/public/interfaces/clipboard.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/cpp/shell_test.h"

using mojo::Array;
using mojo::Map;
using mojo::String;
using mus::mojom::Clipboard;

namespace mus {
namespace clipboard {
namespace {

const char* kUninitialized = "Uninitialized data";
const char* kPlainTextData = "Some plain data";
const char* kHtmlData = "<html>data</html>";

}  // namespace

class ClipboardAppTest : public shell::test::ShellTest {
 public:
  ClipboardAppTest() : ShellTest("exe:mus_clipboard_unittests") {}
  ~ClipboardAppTest() override {}

  // Overridden from shell::test::ShellTest:
  void SetUp() override {
    ShellTest::SetUp();

    connector()->ConnectToInterface("mojo:mus", &clipboard_);
    ASSERT_TRUE(clipboard_);
  }

  uint64_t GetSequenceNumber() {
    uint64_t sequence_num = 999999;
    EXPECT_TRUE(clipboard_->GetSequenceNumber(
        Clipboard::Type::COPY_PASTE, &sequence_num));
    return sequence_num;
  }

  std::vector<std::string> GetAvailableFormatMimeTypes() {
    uint64_t sequence_num = 999999;
    Array<String> types;
    types.push_back(kUninitialized);
    clipboard_->GetAvailableMimeTypes(
        Clipboard::Type::COPY_PASTE,
        &sequence_num, &types);
    return types.To<std::vector<std::string>>();
  }

  bool GetDataOfType(const std::string& mime_type, std::string* data) {
    bool valid = false;
    Array<uint8_t> raw_data;
    uint64_t sequence_number = 0;
    clipboard_->ReadClipboardData(Clipboard::Type::COPY_PASTE, mime_type,
                                  &sequence_number, &raw_data);
    valid = !raw_data.is_null();
    *data = raw_data.is_null() ? "" : raw_data.To<std::string>();
    return valid;
  }

  void SetStringText(const std::string& data) {
    uint64_t sequence_number;
    Map<String, Array<uint8_t>> mime_data;
    mime_data[mojom::kMimeTypeText] = Array<uint8_t>::From(data);
    clipboard_->WriteClipboardData(Clipboard::Type::COPY_PASTE,
                                   std::move(mime_data),
                                   &sequence_number);
  }

 protected:
  mojom::ClipboardPtr clipboard_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardAppTest);
};

TEST_F(ClipboardAppTest, EmptyClipboardOK) {
  EXPECT_EQ(0ul, GetSequenceNumber());
  EXPECT_TRUE(GetAvailableFormatMimeTypes().empty());
  std::string data;
  EXPECT_FALSE(GetDataOfType(mojom::kMimeTypeText, &data));
}

TEST_F(ClipboardAppTest, CanReadBackText) {
  std::string data;
  EXPECT_EQ(0ul, GetSequenceNumber());
  EXPECT_FALSE(GetDataOfType(mojom::kMimeTypeText, &data));

  SetStringText(kPlainTextData);
  EXPECT_EQ(1ul, GetSequenceNumber());

  EXPECT_TRUE(GetDataOfType(mojom::kMimeTypeText, &data));
  EXPECT_EQ(kPlainTextData, data);
}

TEST_F(ClipboardAppTest, CanSetMultipleDataTypesAtOnce) {
  Map<String, Array<uint8_t>> mime_data;
  mime_data[mojom::kMimeTypeText] =
      Array<uint8_t>::From(std::string(kPlainTextData));
  mime_data[mojom::kMimeTypeHTML] =
      Array<uint8_t>::From(std::string(kHtmlData));

  uint64_t sequence_num = 0;
  clipboard_->WriteClipboardData(Clipboard::Type::COPY_PASTE,
                                 std::move(mime_data),
                                 &sequence_num);
  EXPECT_EQ(1ul, sequence_num);

  std::string data;
  EXPECT_TRUE(GetDataOfType(mojom::kMimeTypeText, &data));
  EXPECT_EQ(kPlainTextData, data);
  EXPECT_TRUE(GetDataOfType(mojom::kMimeTypeHTML, &data));
  EXPECT_EQ(kHtmlData, data);
}

TEST_F(ClipboardAppTest, CanClearClipboardWithZeroArray) {
  std::string data;
  SetStringText(kPlainTextData);
  EXPECT_EQ(1ul, GetSequenceNumber());

  EXPECT_TRUE(GetDataOfType(mojom::kMimeTypeText, &data));
  EXPECT_EQ(kPlainTextData, data);

  Map<String, Array<uint8_t>> mime_data;
  uint64_t sequence_num = 0;
  clipboard_->WriteClipboardData(Clipboard::Type::COPY_PASTE,
                                 std::move(mime_data),
                                 &sequence_num);

  EXPECT_EQ(2ul, sequence_num);
  EXPECT_FALSE(GetDataOfType(mojom::kMimeTypeText, &data));
}

}  // namespace clipboard
}  // namespace mus
