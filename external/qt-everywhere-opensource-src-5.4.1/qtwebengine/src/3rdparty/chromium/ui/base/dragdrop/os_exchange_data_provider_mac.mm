// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"

#include "base/logging.h"

namespace ui {

OSExchangeDataProviderMac::OSExchangeDataProviderMac() {
}

OSExchangeDataProviderMac::~OSExchangeDataProviderMac() {
}

OSExchangeData::Provider* OSExchangeDataProviderMac::Clone() const {
  NOTIMPLEMENTED();
  return new OSExchangeDataProviderMac();
}

void OSExchangeDataProviderMac::MarkOriginatedFromRenderer() {
  NOTIMPLEMENTED();
}

bool OSExchangeDataProviderMac::DidOriginateFromRenderer() const {
  NOTIMPLEMENTED();
  return false;
}

void OSExchangeDataProviderMac::SetString(const base::string16& string) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderMac::SetURL(const GURL& url,
                                       const base::string16& title) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderMac::SetFilename(const base::FilePath& path) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderMac::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  NOTIMPLEMENTED();
}

void OSExchangeDataProviderMac::SetPickledData(
    const OSExchangeData::CustomFormat& format,
    const Pickle& data) {
  NOTIMPLEMENTED();
}

bool OSExchangeDataProviderMac::GetString(base::string16* data) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::GetURLAndTitle(
    OSExchangeData::FilenameToURLPolicy policy,
    GURL* url,
    base::string16* title) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::GetFilename(base::FilePath* path) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::GetFilenames(
    std::vector<FileInfo>* filenames) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::GetPickledData(
    const OSExchangeData::CustomFormat& format,
    Pickle* data) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::HasString() const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::HasURL(
    OSExchangeData::FilenameToURLPolicy policy) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::HasFile() const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::HasCustomFormat(
    const OSExchangeData::CustomFormat& format) const {
  NOTIMPLEMENTED();
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// OSExchangeData, public:

// static
OSExchangeData::Provider* OSExchangeData::CreateProvider() {
  return new OSExchangeDataProviderMac;
}

}  // namespace ui
