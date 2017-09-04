// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/MediaTypeNames.h"
#include "core/css/MediaValuesCached.h"
#include "core/html/HTMLDocument.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/html/parser/ResourcePreloader.h"
#include "core/html/parser/TextResourceDecoderForFuzzing.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/testing/FuzzedDataProvider.h"

namespace blink {

std::unique_ptr<CachedDocumentParameters> cachedDocumentParametersForFuzzing(
    FuzzedDataProvider& fuzzedData) {
  std::unique_ptr<CachedDocumentParameters> documentParameters =
      CachedDocumentParameters::create();
  documentParameters->doHtmlPreloadScanning = fuzzedData.ConsumeBool();
  documentParameters->doDocumentWritePreloadScanning = fuzzedData.ConsumeBool();
  // TODO(csharrison): How should this be fuzzed?
  documentParameters->defaultViewportMinWidth = Length();
  documentParameters->viewportMetaZeroValuesQuirk = fuzzedData.ConsumeBool();
  documentParameters->viewportMetaEnabled = fuzzedData.ConsumeBool();
  return documentParameters;
}

class MockResourcePreloader : public ResourcePreloader {
  void preload(std::unique_ptr<PreloadRequest>,
               const NetworkHintsInterface&) override {}
};

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzedData(data, size);

  HTMLParserOptions options;
  options.scriptEnabled = fuzzedData.ConsumeBool();
  options.pluginsEnabled = fuzzedData.ConsumeBool();

  std::unique_ptr<CachedDocumentParameters> documentParameters =
      cachedDocumentParametersForFuzzing(fuzzedData);

  KURL documentURL(ParsedURLString, "http://whatever.test/");

  // Copied from HTMLPreloadScannerTest. May be worthwhile to fuzz.
  MediaValuesCached::MediaValuesCachedData mediaData;
  mediaData.viewportWidth = 500;
  mediaData.viewportHeight = 600;
  mediaData.deviceWidth = 700;
  mediaData.deviceHeight = 800;
  mediaData.devicePixelRatio = 2.0;
  mediaData.colorBitsPerComponent = 24;
  mediaData.monochromeBitsPerComponent = 0;
  mediaData.primaryPointerType = PointerTypeFine;
  mediaData.defaultFontSize = 16;
  mediaData.threeDEnabled = true;
  mediaData.mediaType = MediaTypeNames::screen;
  mediaData.strictMode = true;
  mediaData.displayMode = WebDisplayModeBrowser;

  MockResourcePreloader preloader;

  std::unique_ptr<HTMLPreloadScanner> scanner = HTMLPreloadScanner::create(
      options, documentURL, std::move(documentParameters), mediaData);

  TextResourceDecoderForFuzzing decoder(fuzzedData);
  CString bytes = fuzzedData.ConsumeRemainingBytes();
  String decodedBytes = decoder.decode(bytes.data(), bytes.length());
  scanner->appendToEnd(decodedBytes);
  scanner->scanAndPreload(&preloader, documentURL, nullptr);
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  blink::InitializeBlinkFuzzTest(argc, argv);
  return 0;
}
