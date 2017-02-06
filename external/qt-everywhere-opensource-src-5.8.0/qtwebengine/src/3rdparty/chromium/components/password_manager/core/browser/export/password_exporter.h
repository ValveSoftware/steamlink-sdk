// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_EXPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_EXPORTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace autofill {
struct PasswordForm;
}

namespace base {
class TaskRunner;
}

namespace password_manager {

// Static-only class bundling together the API for exporting passwords into a
// file.
class PasswordExporter {
 public:
  // Exports |passwords| into a file at |path|, overwriting any existing file.
  // Blocking IO tasks will be posted to |blocking_task_runner|. The format of
  // the export will be selected based on the file extension in |path|.
  static void Export(
      const base::FilePath& path,
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& passwords,
      scoped_refptr<base::TaskRunner> blocking_task_runner);

  // Returns the file extensions corresponding to supported formats.
  // Inner vector indicates equivalent extensions. For example:
  //   { { "html", "htm" }, { "csv" } }
  static std::vector<std::vector<base::FilePath::StringType>>
  GetSupportedFileExtensions();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PasswordExporter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_PASSWORD_EXPORTER_H_
