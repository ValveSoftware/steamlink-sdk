// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_CHROMEOS_H_
#define EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_CHROMEOS_H_

#include "base/macros.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "extensions/browser/api/document_scan/document_scan_interface.h"

namespace extensions {

namespace api {

class DocumentScanInterfaceChromeos : public DocumentScanInterface {
 public:
  DocumentScanInterfaceChromeos();
  ~DocumentScanInterfaceChromeos() override;

  void ListScanners(const ListScannersResultsCallback& callback) override;
  void Scan(const std::string& scanner_name,
            ScanMode mode,
            int resolution_dpi,
            const ScanResultsCallback& callback) override;

 private:
  friend class DocumentScanInterfaceChromeosTest;

  void OnScannerListReceived(
      const ListScannersResultsCallback& callback,
      bool succeeded,
      const chromeos::LorgnetteManagerClient::ScannerTable& scanners);
  void OnScanCompleted(const ScanResultsCallback& callback,
                       bool succeeded,
                       const std::string& image_data);
  chromeos::LorgnetteManagerClient* GetLorgnetteManagerClient();

  // Guaranteed to outlive |this|.
  chromeos::LorgnetteManagerClient* lorgnette_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(DocumentScanInterfaceChromeos);
};

}  // namespace api

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_INTERFACE_CHROMEOS_H_
