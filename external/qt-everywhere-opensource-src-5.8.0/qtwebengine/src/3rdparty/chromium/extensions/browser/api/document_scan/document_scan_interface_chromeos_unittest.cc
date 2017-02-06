// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/document_scan_interface_chromeos.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "chromeos/dbus/mock_lorgnette_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;

namespace extensions {

namespace api {

// Tests of networking_private_crypto support for Networking Private API.
class DocumentScanInterfaceChromeosTest : public testing::Test {
 public:
  DocumentScanInterfaceChromeosTest()
      : client_(new chromeos::MockLorgnetteManagerClient()) {}
  ~DocumentScanInterfaceChromeosTest() override {}

  void SetUp() override {
    scan_interface_.lorgnette_manager_client_ = client_.get();
  }

  MOCK_METHOD2(OnListScannersResultReceived,
               void(const std::vector<
                        DocumentScanInterface::ScannerDescription>& scanners,
                    const std::string& error));

  MOCK_METHOD3(OnScanCompleted,
               void(const std::string& scanned_image,
                    const std::string& mime_type,
                    const std::string& error));

 protected:
  DocumentScanInterfaceChromeos scan_interface_;
  std::unique_ptr<chromeos::MockLorgnetteManagerClient> client_;
};

ACTION_P2(InvokeListScannersCallback, scanner_list, error) {
  ::std::tr1::get<0>(args).Run(scanner_list, error);
}

ACTION_P2(InvokeScanCallback, succeeded, image_data) {
  ::std::tr1::get<2>(args).Run(succeeded, image_data);
}

MATCHER_P5(IsScannerDescription, name, manufacturer, model, type, mime, "") {
  return arg.name == name && arg.manufacturer == manufacturer &&
         arg.model == model && arg.scanner_type == type &&
         arg.image_mime_type == mime;
}

MATCHER_P2(IsScannerProperties, mode, resolution, "") {
  return arg.mode == mode && arg.resolution_dpi == resolution;
}

TEST_F(DocumentScanInterfaceChromeosTest, ListScanners) {
  chromeos::LorgnetteManagerClient::ScannerTable scanners;
  const char kScannerName[] = "Monet";
  chromeos::LorgnetteManagerClient::ScannerTableEntry entry;
  const char kScannerManufacturer[] = "Jacques-Louis David";
  entry[lorgnette::kScannerPropertyManufacturer] = kScannerManufacturer;
  const char kScannerModel[] = "Le Havre";
  entry[lorgnette::kScannerPropertyModel] = kScannerModel;
  const char kScannerType[] = "Impressionism";
  entry[lorgnette::kScannerPropertyType] = kScannerType;
  scanners[kScannerName] = entry;
  EXPECT_CALL(*client_, ListScanners(_))
      .WillOnce(InvokeListScannersCallback(true, scanners));
  EXPECT_CALL(*this, OnListScannersResultReceived(
                         testing::ElementsAre(IsScannerDescription(
                             kScannerName, kScannerManufacturer, kScannerModel,
                             kScannerType, "image/png")),
                         ""));
  scan_interface_.ListScanners(base::Bind(
      &DocumentScanInterfaceChromeosTest::OnListScannersResultReceived,
      base::Unretained(this)));
}

TEST_F(DocumentScanInterfaceChromeosTest, ScanFailure) {
  const char kScannerName[] = "Monet";
  const int kResolution = 4096;
  EXPECT_CALL(*client_, ScanImageToString(
                            kScannerName,
                            IsScannerProperties(
                                lorgnette::kScanPropertyModeColor, kResolution),
                            _)).WillOnce(InvokeScanCallback(false, ""));
  EXPECT_CALL(*this, OnScanCompleted("data:image/png;base64,", "image/png",
                                     "Image scan failed"));
  scan_interface_.Scan(
      kScannerName, DocumentScanInterface::kScanModeColor, kResolution,
      base::Bind(&DocumentScanInterfaceChromeosTest::OnScanCompleted,
                 base::Unretained(this)));
}

TEST_F(DocumentScanInterfaceChromeosTest, ScanSuccess) {
  const char kScannerName[] = "Monet";
  const int kResolution = 4096;
  EXPECT_CALL(
      *client_,
      ScanImageToString(
          kScannerName,
          IsScannerProperties(lorgnette::kScanPropertyModeColor, kResolution),
          _)).WillOnce(InvokeScanCallback(true, std::string("PrettyPicture")));

  // Data URL plus base64 representation of "PrettyPicture".
  const char kExpectedImageData[] =
      "data:image/png;base64,UHJldHR5UGljdHVyZQ==";

  EXPECT_CALL(*this, OnScanCompleted(kExpectedImageData, "image/png", ""));
  scan_interface_.Scan(
      kScannerName, DocumentScanInterface::kScanModeColor, kResolution,
      base::Bind(&DocumentScanInterfaceChromeosTest::OnScanCompleted,
                 base::Unretained(this)));
}

}  // namespace api

}  // namespace extensions
