// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_
#define COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "components/user_manager/user_manager_export.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace user_manager {

// Wrapper class storing a still image and its bytes representation for
// WebUI in a web-compatible format such as JPEG. Could be used for storing
// profile images and user wallpapers.
class USER_MANAGER_EXPORT UserImage {
 public:
  // Used to store bytes representation for WebUI.
  // TODO(ivankr): replace with RefCountedMemory to prevent copying.
  typedef std::vector<unsigned char> Bytes;

  // Encodes the given bitmap to bytes representation for WebUI. Returns null
  // on failure.
  static std::unique_ptr<Bytes> Encode(const SkBitmap& bitmap);

  // Creates a new instance from a given still frame and tries to encode it
  // to bytes representation for WebUI. Always returns a non-null result.
  // TODO(ivankr): remove eventually.
  static std::unique_ptr<UserImage> CreateAndEncode(
      const gfx::ImageSkia& image);

  // Create instance with an empty still frame and no bytes
  // representation for WebUI.
  UserImage();

  // Creates a new instance from a given still frame without any bytes
  // representation for WebUI.
  explicit UserImage(const gfx::ImageSkia& image);

  // Creates a new instance from a given still frame and bytes
  // representation for WebUI.
  // TODO(crbug.com/593251): Remove the data copy via |image_bytes|.
  UserImage(const gfx::ImageSkia& image, const Bytes& image_bytes);

  virtual ~UserImage();

  const gfx::ImageSkia& image() const { return image_; }

  // Optional bytes representation of the still image for WebUI.
  bool has_image_bytes() const { return has_image_bytes_; }
  const Bytes& image_bytes() const { return image_bytes_; }

  // URL from which this image was originally downloaded, if any.
  void set_url(const GURL& url) { url_ = url; }
  GURL url() const { return url_; }

  // Whether |image_bytes| contains data in format that is considered safe to
  // decode in sensitive environment (on Login screen).
  bool is_safe_format() const { return is_safe_format_; }
  void MarkAsSafe();

  const base::FilePath& file_path() const { return file_path_; }
  void set_file_path(const base::FilePath& file_path) {
    file_path_ = file_path;
  }

 private:
  gfx::ImageSkia image_;
  bool has_image_bytes_;
  Bytes image_bytes_;
  GURL url_;

  // If image was loaded from the local file, file path is stored here.
  base::FilePath file_path_;
  bool is_safe_format_;

  DISALLOW_COPY_AND_ASSIGN(UserImage);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_
