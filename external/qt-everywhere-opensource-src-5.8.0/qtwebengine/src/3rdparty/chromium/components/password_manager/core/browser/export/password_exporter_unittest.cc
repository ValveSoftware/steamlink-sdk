// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_exporter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class PasswordExporterTest : public testing::Test {
 public:
  PasswordExporterTest() {}

 protected:
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  ConstructTestPasswordForms() {
    std::unique_ptr<autofill::PasswordForm> password_form_(
        new autofill::PasswordForm());
    password_form_->origin = GURL("http://accounts.google.com/a/LoginAuth");
    password_form_->username_value = base::ASCIIToUTF16("test@gmail.com");
    password_form_->password_value = base::ASCIIToUTF16("test1");

    std::vector<std::unique_ptr<autofill::PasswordForm>> password_forms;
    password_forms.push_back(std::move(password_form_));
    return password_forms;
  }

  void StartExportAndWaitUntilCompleteThenReadOutput(
      const base::FilePath::StringType& provided_extension,
      const base::FilePath::StringType& expected_extension,
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& passwords,
      std::string* output) {
    base::FilePath temporary_dir;
    ASSERT_TRUE(base::CreateNewTempDirectory(base::FilePath::StringType(),
                                             &temporary_dir));
    base::FilePath output_file =
        temporary_dir.AppendASCII("passwords").AddExtension(provided_extension);

    PasswordExporter::Export(output_file, passwords,
                             message_loop_.task_runner());

    base::RunLoop run_loop;
    run_loop.RunUntilIdle();

    if (provided_extension != expected_extension) {
      output_file = output_file.ReplaceExtension(expected_extension);
    }

    EXPECT_TRUE(base::ReadFileToString(output_file, output));
    base::DeleteFile(temporary_dir, true);
  }

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PasswordExporterTest);
};

TEST_F(PasswordExporterTest, CSVExport) {
#if defined(OS_WIN)
  const char kLineEnding[] = "\r\n";
#else
  const char kLineEnding[] = "\n";
#endif
  std::string kExpectedCSVOutput = base::StringPrintf(
      "name,url,username,password%s"
      "accounts.google.com,http://accounts.google.com/a/"
      "LoginAuth,test@gmail.com,test1%s",
      kLineEnding, kLineEnding);

  std::string output;
  ASSERT_NO_FATAL_FAILURE(StartExportAndWaitUntilCompleteThenReadOutput(
      FILE_PATH_LITERAL(".csv"), FILE_PATH_LITERAL(".csv"),
      ConstructTestPasswordForms(), &output));
  EXPECT_EQ(kExpectedCSVOutput, output);
}

}  // namespace password_manager
