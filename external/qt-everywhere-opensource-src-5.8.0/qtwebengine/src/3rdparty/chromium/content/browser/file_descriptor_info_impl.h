// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_DESCRIPTOR_INFO_IMPL_H_
#define CONTENT_BROWSER_FILE_DESCRIPTOR_INFO_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/file_descriptor_info.h"

namespace content {

class FileDescriptorInfoImpl : public FileDescriptorInfo {
 public:
  CONTENT_EXPORT static std::unique_ptr<FileDescriptorInfo> Create();

  ~FileDescriptorInfoImpl() override;
  void Share(int id, base::PlatformFile fd) override;
  void Transfer(int id, base::ScopedFD fd) override;
  const base::FileHandleMappingVector& GetMapping() const override;
  base::FileHandleMappingVector GetMappingWithIDAdjustment(
      int delta) const override;
  base::PlatformFile GetFDAt(size_t i) const override;
  int GetIDAt(size_t i) const override;
  size_t GetMappingSize() const override;
  bool OwnsFD(base::PlatformFile file) const override;
  base::ScopedFD ReleaseFD(base::PlatformFile file) override;

 private:
  FileDescriptorInfoImpl();

  void AddToMapping(int id, base::PlatformFile fd);
  bool HasID(int id) const;
  base::FileHandleMappingVector mapping_;
  std::vector<base::ScopedFD> owned_descriptors_;
};
}

#endif  // CONTENT_BROWSER_FILE_DESCRIPTOR_INFO_IMPL_H_
