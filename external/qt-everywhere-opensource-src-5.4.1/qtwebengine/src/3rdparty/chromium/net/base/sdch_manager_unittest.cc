// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/sdch_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

//------------------------------------------------------------------------------
// Provide sample data and compression results with a sample VCDIFF dictionary.
// Note an SDCH dictionary has extra meta-data before the VCDIFF dictionary.
static const char kTestVcdiffDictionary[] = "DictionaryFor"
    "SdchCompression1SdchCompression2SdchCompression3SdchCompression\n";

//------------------------------------------------------------------------------

class SdchManagerTest : public testing::Test {
 protected:
  SdchManagerTest()
    : sdch_manager_(new SdchManager) {
  }

  SdchManager* sdch_manager() { return sdch_manager_.get(); }

  // Reset globals back to default state.
  virtual void TearDown() {
    SdchManager::EnableSdchSupport(true);
    SdchManager::EnableSecureSchemeSupport(false);
  }

 private:
  scoped_ptr<SdchManager> sdch_manager_;
};

//------------------------------------------------------------------------------
static std::string NewSdchDictionary(const std::string& domain) {
  std::string dictionary;
  if (!domain.empty()) {
    dictionary.append("Domain: ");
    dictionary.append(domain);
    dictionary.append("\n");
  }
  dictionary.append("\n");
  dictionary.append(kTestVcdiffDictionary, sizeof(kTestVcdiffDictionary) - 1);
  return dictionary;
}

TEST_F(SdchManagerTest, DomainSupported) {
  GURL google_url("http://www.google.com");

  SdchManager::EnableSdchSupport(false);
  EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(google_url));
  SdchManager::EnableSdchSupport(true);
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(google_url));
}

TEST_F(SdchManagerTest, DomainBlacklisting) {
  GURL test_url("http://www.test.com");
  GURL google_url("http://www.google.com");

  sdch_manager()->BlacklistDomain(test_url);
  EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(test_url));
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(google_url));

  sdch_manager()->BlacklistDomain(google_url);
  EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(google_url));
}

TEST_F(SdchManagerTest, DomainBlacklistingCaseSensitivity) {
  GURL test_url("http://www.TesT.com");
  GURL test2_url("http://www.tEst.com");

  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(test_url));
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(test2_url));
  sdch_manager()->BlacklistDomain(test_url);
  EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(test2_url));
}

TEST_F(SdchManagerTest, BlacklistingReset) {
  GURL gurl("http://mytest.DoMain.com");
  std::string domain(gurl.host());

  sdch_manager()->ClearBlacklistings();
  EXPECT_EQ(sdch_manager()->BlackListDomainCount(domain), 0);
  EXPECT_EQ(sdch_manager()->BlacklistDomainExponential(domain), 0);
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(gurl));
}

TEST_F(SdchManagerTest, BlacklistingSingleBlacklist) {
  GURL gurl("http://mytest.DoMain.com");
  std::string domain(gurl.host());
  sdch_manager()->ClearBlacklistings();

  sdch_manager()->BlacklistDomain(gurl);
  EXPECT_EQ(sdch_manager()->BlackListDomainCount(domain), 1);
  EXPECT_EQ(sdch_manager()->BlacklistDomainExponential(domain), 1);

  // Check that any domain lookup reduces the blacklist counter.
  EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(gurl));
  EXPECT_EQ(sdch_manager()->BlackListDomainCount(domain), 0);
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(gurl));
}

TEST_F(SdchManagerTest, BlacklistingExponential) {
  GURL gurl("http://mytest.DoMain.com");
  std::string domain(gurl.host());
  sdch_manager()->ClearBlacklistings();

  int exponential = 1;
  for (int i = 1; i < 100; ++i) {
    sdch_manager()->BlacklistDomain(gurl);
    EXPECT_EQ(sdch_manager()->BlacklistDomainExponential(domain), exponential);

    EXPECT_EQ(sdch_manager()->BlackListDomainCount(domain), exponential);
    EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(gurl));
    EXPECT_EQ(sdch_manager()->BlackListDomainCount(domain), exponential - 1);

    // Simulate a large number of domain checks (which eventually remove the
    // blacklisting).
    sdch_manager()->ClearDomainBlacklisting(domain);
    EXPECT_EQ(sdch_manager()->BlackListDomainCount(domain), 0);
    EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(gurl));

    // Predict what exponential backoff will be.
    exponential = 1 + 2 * exponential;
    if (exponential < 0)
      exponential = INT_MAX;  // We don't wrap.
  }
}

TEST_F(SdchManagerTest, CanSetExactMatchDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Perfect match should work.
  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));
}

TEST_F(SdchManagerTest, CanAdvertiseDictionaryOverHTTP) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));

  std::string dictionary_list;
  // HTTP target URL can advertise dictionary.
  sdch_manager()->GetAvailDictionaryList(
      GURL("http://" + dictionary_domain + "/test"),
      &dictionary_list);
  EXPECT_FALSE(dictionary_list.empty());
}

TEST_F(SdchManagerTest, CanNotAdvertiseDictionaryOverHTTPS) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));

  std::string dictionary_list;
  // HTTPS target URL should NOT advertise dictionary.
  sdch_manager()->GetAvailDictionaryList(
      GURL("https://" + dictionary_domain + "/test"),
      &dictionary_list);
  EXPECT_TRUE(dictionary_list.empty());
}

TEST_F(SdchManagerTest, CanUseHTTPSDictionaryOverHTTPSIfEnabled) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  EXPECT_FALSE(sdch_manager()->AddSdchDictionary(
      dictionary_text, GURL("https://" + dictionary_domain)));
  SdchManager::EnableSecureSchemeSupport(true);
  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(
      dictionary_text, GURL("https://" + dictionary_domain)));

  GURL target_url("https://" + dictionary_domain + "/test");
  std::string dictionary_list;
  // HTTPS target URL should advertise dictionary if secure scheme support is
  // enabled.
  sdch_manager()->GetAvailDictionaryList(target_url, &dictionary_list);
  EXPECT_FALSE(dictionary_list.empty());

  // Dictionary should be available.
  scoped_refptr<SdchManager::Dictionary> dictionary;
  std::string client_hash;
  std::string server_hash;
  sdch_manager()->GenerateHash(dictionary_text, &client_hash, &server_hash);
  sdch_manager()->GetVcdiffDictionary(server_hash, target_url, &dictionary);
  EXPECT_TRUE(dictionary != NULL);
}

TEST_F(SdchManagerTest, CanNotUseHTTPDictionaryOverHTTPS) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));

  GURL target_url("https://" + dictionary_domain + "/test");
  std::string dictionary_list;
  // HTTPS target URL should not advertise dictionary acquired over HTTP even if
  // secure scheme support is enabled.
  SdchManager::EnableSecureSchemeSupport(true);
  sdch_manager()->GetAvailDictionaryList(target_url, &dictionary_list);
  EXPECT_TRUE(dictionary_list.empty());

  scoped_refptr<SdchManager::Dictionary> dictionary;
  std::string client_hash;
  std::string server_hash;
  sdch_manager()->GenerateHash(dictionary_text, &client_hash, &server_hash);
  sdch_manager()->GetVcdiffDictionary(server_hash, target_url, &dictionary);
  EXPECT_TRUE(dictionary == NULL);
}

TEST_F(SdchManagerTest, FailToSetDomainMismatchDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Fail the "domain match" requirement.
  EXPECT_FALSE(sdch_manager()->AddSdchDictionary(dictionary_text,
               GURL("http://y.z.google.com")));
}

TEST_F(SdchManagerTest, FailToSetDotHostPrefixDomainDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Fail the HD with D being the domain and H having a dot requirement.
  EXPECT_FALSE(sdch_manager()->AddSdchDictionary(dictionary_text,
               GURL("http://w.x.y.z.google.com")));
}

TEST_F(SdchManagerTest, FailToSetRepeatPrefixWithDotDictionary) {
  // Make sure that a prefix that matches the domain postfix won't confuse
  // the validation checks.
  std::string dictionary_domain("www.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Fail the HD with D being the domain and H having a dot requirement.
  EXPECT_FALSE(sdch_manager()->AddSdchDictionary(dictionary_text,
               GURL("http://www.google.com.www.google.com")));
}

TEST_F(SdchManagerTest, CanSetLeadingDotDomainDictionary) {
  // Make sure that a prefix that matches the domain postfix won't confuse
  // the validation checks.
  std::string dictionary_domain(".google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Verify that a leading dot in the domain is acceptable, as long as the host
  // name does not contain any dots preceding the matched domain name.
  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(dictionary_text,
               GURL("http://www.google.com")));
}

// Make sure the order of the tests is not helping us or confusing things.
// See test CanSetExactMatchDictionary above for first try.
TEST_F(SdchManagerTest, CanStillSetExactMatchDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Perfect match should *STILL* work.
  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));
}

// Make sure the DOS protection precludes the addition of too many dictionaries.
TEST_F(SdchManagerTest, TooManyDictionaries) {
  std::string dictionary_domain(".google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  size_t count = 0;
  while (count <= SdchManager::kMaxDictionaryCount + 1) {
    if (!sdch_manager()->AddSdchDictionary(dictionary_text,
                                          GURL("http://www.google.com")))
      break;

    dictionary_text += " ";  // Create dictionary with different SHA signature.
    ++count;
  }
  EXPECT_EQ(SdchManager::kMaxDictionaryCount, count);
}

TEST_F(SdchManagerTest, DictionaryNotTooLarge) {
  std::string dictionary_domain(".google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  dictionary_text.append(
      SdchManager::kMaxDictionarySize  - dictionary_text.size(), ' ');
  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));
}

TEST_F(SdchManagerTest, DictionaryTooLarge) {
  std::string dictionary_domain(".google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  dictionary_text.append(
      SdchManager::kMaxDictionarySize + 1 - dictionary_text.size(), ' ');
  EXPECT_FALSE(sdch_manager()->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));
}

TEST_F(SdchManagerTest, PathMatch) {
  bool (*PathMatch)(const std::string& path, const std::string& restriction) =
      SdchManager::Dictionary::PathMatch;
  // Perfect match is supported.
  EXPECT_TRUE(PathMatch("/search", "/search"));
  EXPECT_TRUE(PathMatch("/search/", "/search/"));

  // Prefix only works if last character of restriction is a slash, or first
  // character in path after a match is a slash.  Validate each case separately.

  // Rely on the slash in the path (not at the end of the restriction).
  EXPECT_TRUE(PathMatch("/search/something", "/search"));
  EXPECT_TRUE(PathMatch("/search/s", "/search"));
  EXPECT_TRUE(PathMatch("/search/other", "/search"));
  EXPECT_TRUE(PathMatch("/search/something", "/search"));

  // Rely on the slash at the end of the restriction.
  EXPECT_TRUE(PathMatch("/search/something", "/search/"));
  EXPECT_TRUE(PathMatch("/search/s", "/search/"));
  EXPECT_TRUE(PathMatch("/search/other", "/search/"));
  EXPECT_TRUE(PathMatch("/search/something", "/search/"));

  // Make sure less that sufficient prefix match is false.
  EXPECT_FALSE(PathMatch("/sear", "/search"));
  EXPECT_FALSE(PathMatch("/", "/search"));
  EXPECT_FALSE(PathMatch(std::string(), "/search"));

  // Add examples with several levels of direcories in the restriction.
  EXPECT_FALSE(PathMatch("/search/something", "search/s"));
  EXPECT_FALSE(PathMatch("/search/", "/search/s"));

  // Make sure adding characters to path will also fail.
  EXPECT_FALSE(PathMatch("/searching", "/search/"));
  EXPECT_FALSE(PathMatch("/searching", "/search"));

  // Make sure we're case sensitive.
  EXPECT_FALSE(PathMatch("/ABC", "/abc"));
  EXPECT_FALSE(PathMatch("/abc", "/ABC"));
}

// The following are only applicable while we have a latency test in the code,
// and can be removed when that functionality is stripped.
TEST_F(SdchManagerTest, LatencyTestControls) {
  GURL url("http://www.google.com");
  GURL url2("http://www.google2.com");

  // First make sure we default to false.
  EXPECT_FALSE(sdch_manager()->AllowLatencyExperiment(url));
  EXPECT_FALSE(sdch_manager()->AllowLatencyExperiment(url2));

  // That we can set each to true.
  sdch_manager()->SetAllowLatencyExperiment(url, true);
  EXPECT_TRUE(sdch_manager()->AllowLatencyExperiment(url));
  EXPECT_FALSE(sdch_manager()->AllowLatencyExperiment(url2));

  sdch_manager()->SetAllowLatencyExperiment(url2, true);
  EXPECT_TRUE(sdch_manager()->AllowLatencyExperiment(url));
  EXPECT_TRUE(sdch_manager()->AllowLatencyExperiment(url2));

  // And can reset them to false.
  sdch_manager()->SetAllowLatencyExperiment(url, false);
  EXPECT_FALSE(sdch_manager()->AllowLatencyExperiment(url));
  EXPECT_TRUE(sdch_manager()->AllowLatencyExperiment(url2));

  sdch_manager()->SetAllowLatencyExperiment(url2, false);
  EXPECT_FALSE(sdch_manager()->AllowLatencyExperiment(url));
  EXPECT_FALSE(sdch_manager()->AllowLatencyExperiment(url2));
}

TEST_F(SdchManagerTest, CanUseMultipleManagers) {
  SdchManager second_manager;

  std::string dictionary_domain_1("x.y.z.google.com");
  std::string dictionary_domain_2("x.y.z.chromium.org");

  std::string dictionary_text_1(NewSdchDictionary(dictionary_domain_1));
  std::string dictionary_text_2(NewSdchDictionary(dictionary_domain_2));

  std::string tmp_hash;
  std::string server_hash_1;
  std::string server_hash_2;

  SdchManager::GenerateHash(dictionary_text_1, &tmp_hash, &server_hash_1);
  SdchManager::GenerateHash(dictionary_text_2, &tmp_hash, &server_hash_2);

  // Confirm that if you add directories to one manager, you
  // can't get them from the other.
  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(
      dictionary_text_1, GURL("http://" + dictionary_domain_1)));
  scoped_refptr<SdchManager::Dictionary> dictionary;
  sdch_manager()->GetVcdiffDictionary(
      server_hash_1,
      GURL("http://" + dictionary_domain_1 + "/random_url"),
      &dictionary);
  EXPECT_TRUE(dictionary);

  EXPECT_TRUE(second_manager.AddSdchDictionary(
      dictionary_text_2, GURL("http://" + dictionary_domain_2)));
  second_manager.GetVcdiffDictionary(
      server_hash_2,
      GURL("http://" + dictionary_domain_2 + "/random_url"),
      &dictionary);
  EXPECT_TRUE(dictionary);

  sdch_manager()->GetVcdiffDictionary(
      server_hash_2,
      GURL("http://" + dictionary_domain_2 + "/random_url"),
      &dictionary);
  EXPECT_FALSE(dictionary);

  second_manager.GetVcdiffDictionary(
      server_hash_1,
      GURL("http://" + dictionary_domain_1 + "/random_url"),
      &dictionary);
  EXPECT_FALSE(dictionary);
}

TEST_F(SdchManagerTest, HttpsCorrectlySupported) {
  GURL url("http://www.google.com");
  GURL secure_url("https://www.google.com");

  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(url));
  EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(secure_url));

  SdchManager::EnableSecureSchemeSupport(true);
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(url));
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(secure_url));
}

TEST_F(SdchManagerTest, ClearDictionaryData) {
  std::string dictionary_domain("x.y.z.google.com");
  GURL blacklist_url("http://bad.chromium.org");

  std::string dictionary_text(NewSdchDictionary(dictionary_domain));
  std::string tmp_hash;
  std::string server_hash;

  SdchManager::GenerateHash(dictionary_text, &tmp_hash, &server_hash);

  EXPECT_TRUE(sdch_manager()->AddSdchDictionary(
      dictionary_text, GURL("http://" + dictionary_domain)));
  scoped_refptr<SdchManager::Dictionary> dictionary;
  sdch_manager()->GetVcdiffDictionary(
      server_hash,
      GURL("http://" + dictionary_domain + "/random_url"),
      &dictionary);
  EXPECT_TRUE(dictionary);

  sdch_manager()->BlacklistDomain(GURL(blacklist_url));
  EXPECT_FALSE(sdch_manager()->IsInSupportedDomain(blacklist_url));

  sdch_manager()->ClearData();

  dictionary = NULL;
  sdch_manager()->GetVcdiffDictionary(
      server_hash,
      GURL("http://" + dictionary_domain + "/random_url"),
      &dictionary);
  EXPECT_FALSE(dictionary);
  EXPECT_TRUE(sdch_manager()->IsInSupportedDomain(blacklist_url));
}

}  // namespace net

