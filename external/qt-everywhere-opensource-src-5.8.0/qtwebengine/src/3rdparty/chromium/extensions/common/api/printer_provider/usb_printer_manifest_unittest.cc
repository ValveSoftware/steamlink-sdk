// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_filter.h"
#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/value_builder.h"

namespace extensions {

class UsbPrinterManifestTest : public ManifestTest {
 public:
  UsbPrinterManifestTest() {}
  ~UsbPrinterManifestTest() override {}
};

TEST_F(UsbPrinterManifestTest, Filters) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("usb_printers_filters.json");
  const UsbPrinterManifestData* manifest_data =
      UsbPrinterManifestData::Get(extension.get());
  ASSERT_TRUE(manifest_data);
  EXPECT_EQ(2u, manifest_data->filters().size());
  EXPECT_TRUE(DictionaryBuilder()
                  .Set("vendorId", 1)
                  .Set("productId", 2)
                  .Build()
                  ->Equals(manifest_data->filters()[0].ToValue().get()));
  EXPECT_TRUE(DictionaryBuilder()
                  .Set("vendorId", 1)
                  .Set("interfaceClass", 2)
                  .Set("interfaceSubclass", 3)
                  .Set("interfaceProtocol", 4)
                  .Build()
                  ->Equals(manifest_data->filters()[1].ToValue().get()));
}

TEST_F(UsbPrinterManifestTest, InvalidFilter) {
  LoadAndExpectError(
      "usb_printers_invalid_filter.json",
      "Only one of productId or interfaceClass may be specified.");
}

}  // namespace extensions
