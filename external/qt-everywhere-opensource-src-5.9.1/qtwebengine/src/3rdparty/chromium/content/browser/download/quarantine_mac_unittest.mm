// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/xattr.h>

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/download/quarantine.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace content {
namespace {

class QuarantineMacTest : public testing::Test {
 public:
  QuarantineMacTest()
      : source_url_("http://www.source.com"),
        referrer_url_("http://www.referrer.com") {}

 protected:
  void SetUp() override {
    if (base::mac::IsAtMostOS10_9())
      return;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file_));
    file_url_.reset([[NSURL alloc]
        initFileURLWithPath:base::SysUTF8ToNSString(test_file_.value())]);

    base::scoped_nsobject<NSMutableDictionary> properties(
        [[NSMutableDictionary alloc] init]);
    [properties
        setValue:@"com.google.Chrome"
          forKey:static_cast<NSString*>(kLSQuarantineAgentBundleIdentifierKey)];
    [properties setValue:@"Google Chrome.app"
                  forKey:static_cast<NSString*>(kLSQuarantineAgentNameKey)];
    [properties setValue:@(1) forKey:@"kLSQuarantineIsOwnedByCurrentUserKey"];
// NSURLQuarantinePropertiesKey is only available on macOS 10.10+.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
    bool success = [file_url_ setResourceValue:properties
                                        forKey:NSURLQuarantinePropertiesKey
                                         error:nullptr];
#pragma clang diagnostic pop
    ASSERT_TRUE(success);
  }

  void VerifyAttributesAreSetCorrectly() {
    base::scoped_nsobject<NSURL> file_url([[NSURL alloc]
        initFileURLWithPath:base::SysUTF8ToNSString(test_file_.value())]);
    ASSERT_TRUE(file_url);

    NSError* error = nil;
    NSDictionary* properties = nil;
// NSURLQuarantinePropertiesKey is only available on macOS 10.10+.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
    BOOL success = [file_url getResourceValue:&properties
                                       forKey:NSURLQuarantinePropertiesKey
                                        error:&error];
#pragma clang diagnostic pop
    ASSERT_TRUE(success);
    ASSERT_TRUE(properties);
    ASSERT_NSEQ(
        [[properties valueForKey:static_cast<NSString*>(
                                     kLSQuarantineOriginURLKey)] description],
        base::SysUTF8ToNSString(referrer_url_.spec()));
    ASSERT_NSEQ([[properties
                    valueForKey:static_cast<NSString*>(kLSQuarantineDataURLKey)]
                    description],
                base::SysUTF8ToNSString(source_url_.spec()));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_;
  GURL source_url_;
  GURL referrer_url_;
  base::scoped_nsobject<NSURL> file_url_;
};

TEST_F(QuarantineMacTest, CheckMetadataSetCorrectly) {
  if (base::mac::IsAtMostOS10_9())
    return;
  QuarantineFile(test_file_, source_url_, referrer_url_, "");
  VerifyAttributesAreSetCorrectly();
}

TEST_F(QuarantineMacTest, SetMetadataMultipleTimes) {
  if (base::mac::IsAtMostOS10_9())
    return;
  GURL dummy_url("http://www.dummy.com");
  QuarantineFile(test_file_, source_url_, referrer_url_, "");
  QuarantineFile(test_file_, dummy_url, dummy_url, "");
  VerifyAttributesAreSetCorrectly();
}

}  // namespace
}  // namespace content
