// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_exporter.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"

namespace password_manager {

namespace {

const base::FilePath::CharType kFileExtension[] = FILE_PATH_LITERAL("csv");

void WriteToFile(const base::FilePath& path,
                 std::unique_ptr<std::string> data) {
  base::WriteFile(path, data->c_str(), data->size());
}

}  // namespace

// static
void PasswordExporter::Export(
    const base::FilePath& path,
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& passwords,
    scoped_refptr<base::TaskRunner> blocking_task_runner) {
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&WriteToFile, path,
                 base::Passed(base::MakeUnique<std::string>(
                     PasswordCSVWriter::SerializePasswords(passwords)))));
}

// static
std::vector<std::vector<base::FilePath::StringType>>
PasswordExporter::GetSupportedFileExtensions() {
  return std::vector<std::vector<base::FilePath::StringType>>(
      1, std::vector<base::FilePath::StringType>(1, kFileExtension));
}

}  // namespace password_manager
