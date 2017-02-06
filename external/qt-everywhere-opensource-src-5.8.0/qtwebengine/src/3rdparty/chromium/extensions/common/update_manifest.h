// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_UPDATE_MANIFEST_H_
#define EXTENSIONS_COMMON_UPDATE_MANIFEST_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

class UpdateManifest {
 public:
  // An update manifest looks like this:
  //
  // <?xml version="1.0" encoding="UTF-8"?>
  // <gupdate xmlns="http://www.google.com/update2/response" protocol="2.0">
  //  <daystart elapsed_seconds="300" />
  //  <app appid="12345" status="ok">
  //   <updatecheck codebase="http://example.com/extension_1.2.3.4.crx"
  //                hash="12345" size="9854" status="ok" version="1.2.3.4"
  //                prodversionmin="2.0.143.0"
  //                codebasediff="http://example.com/diff_1.2.3.4.crx"
  //                hashdiff="123" sizediff="101"
  //                fp="1.123" />
  //  </app>
  // </gupdate>
  //
  // The <daystart> tag contains a "elapsed_seconds" attribute which refers to
  // the server's notion of how many seconds it has been since midnight.
  //
  // The "appid" attribute of the <app> tag refers to the unique id of the
  // extension. The "codebase" attribute of the <updatecheck> tag is the url to
  // fetch the updated crx file, and the "prodversionmin" attribute refers to
  // the minimum version of the chrome browser that the update applies to.

  // The diff data members correspond to the differential update package, if
  // a differential update is specified in the response.

  // The result of parsing one <app> tag in an xml update check manifest.
  struct Result {
    Result();
    Result(const Result& other);
    ~Result();

    std::string extension_id;
    std::string version;
    std::string browser_min_version;

    // Attributes for the full update.
    GURL crx_url;
    std::string package_hash;
    int size;
    std::string package_fingerprint;

    // Attributes for the differential update.
    GURL diff_crx_url;
    std::string diff_package_hash;
    int diff_size;
  };

  static const int kNoDaystart = -1;
  struct Results {
    Results();
    ~Results();

    std::vector<Result> list;
    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_seconds;
  };

  UpdateManifest();
  ~UpdateManifest();

  // Parses an update manifest xml string into Result data. Returns a bool
  // indicating success or failure. On success, the results are available by
  // calling results(). The details for any failures are available by calling
  // errors().
  bool Parse(const std::string& manifest_xml);

  const Results& results() { return results_; }
  const std::string& errors() { return errors_; }

 private:
  Results results_;
  std::string errors_;

  // Helper function that adds parse error details to our errors_ string.
  void ParseError(const char* details, ...);

  DISALLOW_COPY_AND_ASSIGN(UpdateManifest);
};

#endif  // EXTENSIONS_COMMON_UPDATE_MANIFEST_H_
