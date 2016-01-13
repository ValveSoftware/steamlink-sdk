// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURA_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURA_H_

#include <map>

#include "base/files/file_path.h"
#include "base/pickle.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector2d.h"
#include "url/gurl.h"

namespace ui {

class Clipboard;

// OSExchangeData::Provider implementation for aura on linux.
class UI_BASE_EXPORT OSExchangeDataProviderAura
    : public OSExchangeData::Provider {
 public:
  OSExchangeDataProviderAura();
  virtual ~OSExchangeDataProviderAura();

  // Overridden from OSExchangeData::Provider:
  virtual Provider* Clone() const OVERRIDE;
  virtual void MarkOriginatedFromRenderer() OVERRIDE;
  virtual bool DidOriginateFromRenderer() const OVERRIDE;
  virtual void SetString(const base::string16& data) OVERRIDE;
  virtual void SetURL(const GURL& url, const base::string16& title) OVERRIDE;
  virtual void SetFilename(const base::FilePath& path) OVERRIDE;
  virtual void SetFilenames(const std::vector<FileInfo>& filenames) OVERRIDE;
  virtual void SetPickledData(const OSExchangeData::CustomFormat& format,
                              const Pickle& data) OVERRIDE;
  virtual bool GetString(base::string16* data) const OVERRIDE;
  virtual bool GetURLAndTitle(OSExchangeData::FilenameToURLPolicy policy,
                              GURL* url,
                              base::string16* title) const OVERRIDE;
  virtual bool GetFilename(base::FilePath* path) const OVERRIDE;
  virtual bool GetFilenames(std::vector<FileInfo>* filenames) const OVERRIDE;
  virtual bool GetPickledData(const OSExchangeData::CustomFormat& format,
                              Pickle* data) const OVERRIDE;
  virtual bool HasString() const OVERRIDE;
  virtual bool HasURL(OSExchangeData::FilenameToURLPolicy policy) const
      OVERRIDE;
  virtual bool HasFile() const OVERRIDE;
  virtual bool HasCustomFormat(const OSExchangeData::CustomFormat& format) const
      OVERRIDE;

  virtual void SetHtml(const base::string16& html,
                       const GURL& base_url) OVERRIDE;
  virtual bool GetHtml(base::string16* html, GURL* base_url) const OVERRIDE;
  virtual bool HasHtml() const OVERRIDE;
  virtual void SetDragImage(const gfx::ImageSkia& image,
                            const gfx::Vector2d& cursor_offset) OVERRIDE;
  virtual const gfx::ImageSkia& GetDragImage() const OVERRIDE;
  virtual const gfx::Vector2d& GetDragImageOffset() const OVERRIDE;

 private:
  typedef std::map<OSExchangeData::CustomFormat, Pickle>  PickleData;

  // Returns true if |formats_| contains a string format and the string can be
  // parsed as a URL.
  bool GetPlainTextURL(GURL* url) const;

  // Actual formats that have been set. See comment above |known_formats_|
  // for details.
  int formats_;

  // String contents.
  base::string16 string_;

  // URL contents.
  GURL url_;
  base::string16 title_;

  // File name.
  std::vector<FileInfo> filenames_;

  // PICKLED_DATA contents.
  PickleData pickle_data_;

  // Drag image and offset data.
  gfx::ImageSkia drag_image_;
  gfx::Vector2d drag_image_offset_;

  // For HTML format
  base::string16 html_;
  GURL base_url_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderAura);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURA_H_
