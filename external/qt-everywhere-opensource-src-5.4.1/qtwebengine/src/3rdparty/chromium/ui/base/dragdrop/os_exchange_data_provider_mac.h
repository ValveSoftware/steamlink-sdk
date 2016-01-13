// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_

#include "ui/base/dragdrop/os_exchange_data.h"

namespace ui {

// OSExchangeData::Provider implementation for Mac.
class UI_BASE_EXPORT OSExchangeDataProviderMac
    : public OSExchangeData::Provider {
 public:
  OSExchangeDataProviderMac();
  virtual ~OSExchangeDataProviderMac();

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
  virtual bool HasURL(
      OSExchangeData::FilenameToURLPolicy policy) const OVERRIDE;
  virtual bool HasFile() const OVERRIDE;
  virtual bool HasCustomFormat(
      const OSExchangeData::CustomFormat& format) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderMac);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_MAC_H_
