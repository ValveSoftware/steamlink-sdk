// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store_unittest.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_monster_store_test.h"  // For CookieStore mock
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

using base::Time;
using base::TimeDelta;

namespace {

// TODO(erikwright): Replace the pre-existing MockPersistentCookieStore (and
// brethren) with this one, and remove the 'New' prefix.
class NewMockPersistentCookieStore
    : public CookieMonster::PersistentCookieStore {
 public:
  MOCK_METHOD1(Load, void(const LoadedCallback& loaded_callback));
  MOCK_METHOD2(LoadCookiesForKey, void(const std::string& key,
                                       const LoadedCallback& loaded_callback));
  MOCK_METHOD1(AddCookie, void(const CanonicalCookie& cc));
  MOCK_METHOD1(UpdateCookieAccessTime, void(const CanonicalCookie& cc));
  MOCK_METHOD1(DeleteCookie, void(const CanonicalCookie& cc));
  virtual void Flush(const base::Closure& callback) {
    if (!callback.is_null())
      base::MessageLoop::current()->PostTask(FROM_HERE, callback);
  }
  MOCK_METHOD0(SetForceKeepSessionState, void());

 private:
  virtual ~NewMockPersistentCookieStore() {}
};

const char* kTopLevelDomainPlus1 = "http://www.harvard.edu";
const char* kTopLevelDomainPlus2 = "http://www.math.harvard.edu";
const char* kTopLevelDomainPlus2Secure = "https://www.math.harvard.edu";
const char* kTopLevelDomainPlus3 =
    "http://www.bourbaki.math.harvard.edu";
const char* kOtherDomain = "http://www.mit.edu";
const char kUrlGoogleSpecific[] = "http://www.gmail.google.izzle";

class GetCookieListCallback : public CookieCallback {
 public:
  GetCookieListCallback() {}
  explicit GetCookieListCallback(Thread* run_in_thread)
      : CookieCallback(run_in_thread) {}

  void Run(const CookieList& cookies) {
    cookies_ = cookies;
    CallbackEpilogue();
  }

  const CookieList& cookies() { return cookies_; }

 private:
  CookieList cookies_;
};

struct CookieMonsterTestTraits {
  static scoped_refptr<CookieStore> Create() {
    return new CookieMonster(NULL, NULL);
  }

  static const bool is_cookie_monster              = true;
  static const bool supports_http_only             = true;
  static const bool supports_non_dotted_domains    = true;
  static const bool supports_trailing_dots         = true;
  static const bool filters_schemes                = true;
  static const bool has_path_prefix_bug            = false;
  static const int creation_time_granularity_in_ms = 0;
};

INSTANTIATE_TYPED_TEST_CASE_P(CookieMonster,
                              CookieStoreTest,
                              CookieMonsterTestTraits);

INSTANTIATE_TYPED_TEST_CASE_P(CookieMonster,
                              MultiThreadedCookieStoreTest,
                              CookieMonsterTestTraits);

class CookieMonsterTest : public CookieStoreTest<CookieMonsterTestTraits> {
 protected:

  CookieList GetAllCookies(CookieMonster* cm) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetAllCookiesAsync(
        base::Bind(&GetCookieListCallback::Run,
                   base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookies();
  }

  CookieList GetAllCookiesForURL(CookieMonster* cm,
                                 const GURL& url) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetAllCookiesForURLAsync(
        url, base::Bind(&GetCookieListCallback::Run,
                        base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookies();
  }

  CookieList GetAllCookiesForURLWithOptions(CookieMonster* cm,
                                            const GURL& url,
                                            const CookieOptions& options) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetAllCookiesForURLWithOptionsAsync(
        url, options, base::Bind(&GetCookieListCallback::Run,
                                 base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookies();
  }

  bool SetCookieWithDetails(CookieMonster* cm,
                            const GURL& url,
                            const std::string& name,
                            const std::string& value,
                            const std::string& domain,
                            const std::string& path,
                            const base::Time& expiration_time,
                            bool secure,
                            bool http_only,
                            CookiePriority priority) {
    DCHECK(cm);
    ResultSavingCookieCallback<bool> callback;
    cm->SetCookieWithDetailsAsync(
        url, name, value, domain, path, expiration_time, secure, http_only,
        priority,
        base::Bind(
            &ResultSavingCookieCallback<bool>::Run,
            base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  int DeleteAll(CookieMonster*cm) {
    DCHECK(cm);
    ResultSavingCookieCallback<int> callback;
    cm->DeleteAllAsync(
        base::Bind(
            &ResultSavingCookieCallback<int>::Run,
            base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  int DeleteAllCreatedBetween(CookieMonster*cm,
                              const base::Time& delete_begin,
                              const base::Time& delete_end) {
    DCHECK(cm);
    ResultSavingCookieCallback<int> callback;
    cm->DeleteAllCreatedBetweenAsync(
        delete_begin, delete_end,
        base::Bind(
            &ResultSavingCookieCallback<int>::Run,
            base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  int DeleteAllCreatedBetweenForHost(CookieMonster* cm,
                                     const base::Time delete_begin,
                                     const base::Time delete_end,
                                     const GURL& url) {
    DCHECK(cm);
    ResultSavingCookieCallback<int> callback;
    cm->DeleteAllCreatedBetweenForHostAsync(
        delete_begin, delete_end, url,
        base::Bind(
            &ResultSavingCookieCallback<int>::Run,
            base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  int DeleteAllForHost(CookieMonster* cm,
                       const GURL& url) {
    DCHECK(cm);
    ResultSavingCookieCallback<int> callback;
    cm->DeleteAllForHostAsync(
        url, base::Bind(&ResultSavingCookieCallback<int>::Run,
                        base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  bool DeleteCanonicalCookie(CookieMonster* cm, const CanonicalCookie& cookie) {
    DCHECK(cm);
    ResultSavingCookieCallback<bool> callback;
    cm->DeleteCanonicalCookieAsync(
        cookie,
        base::Bind(&ResultSavingCookieCallback<bool>::Run,
                   base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  // Helper for DeleteAllForHost test; repopulates CM with same layout
  // each time.
  void PopulateCmForDeleteAllForHost(scoped_refptr<CookieMonster> cm) {
    GURL url_top_level_domain_plus_1(kTopLevelDomainPlus1);
    GURL url_top_level_domain_plus_2(kTopLevelDomainPlus2);
    GURL url_top_level_domain_plus_2_secure(kTopLevelDomainPlus2Secure);
    GURL url_top_level_domain_plus_3(kTopLevelDomainPlus3);
    GURL url_other(kOtherDomain);

    DeleteAll(cm.get());

    // Static population for probe:
    //    * Three levels of domain cookie (.b.a, .c.b.a, .d.c.b.a)
    //    * Three levels of host cookie (w.b.a, w.c.b.a, w.d.c.b.a)
    //    * http_only cookie (w.c.b.a)
    //    * Two secure cookies (.c.b.a, w.c.b.a)
    //    * Two domain path cookies (.c.b.a/dir1, .c.b.a/dir1/dir2)
    //    * Two host path cookies (w.c.b.a/dir1, w.c.b.a/dir1/dir2)

    // Domain cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_1,
                                           "dom_1",
                                           "X",
                                           ".harvard.edu",
                                           "/",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2,
                                           "dom_2",
                                           "X",
                                           ".math.harvard.edu",
                                           "/",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_3,
                                           "dom_3",
                                           "X",
                                           ".bourbaki.math.harvard.edu",
                                           "/",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));

    // Host cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_1,
                                           "host_1",
                                           "X",
                                           std::string(),
                                           "/",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2,
                                           "host_2",
                                           "X",
                                           std::string(),
                                           "/",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_3,
                                           "host_3",
                                           "X",
                                           std::string(),
                                           "/",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));

    // Http_only cookie
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2,
                                           "httpo_check",
                                           "X",
                                           std::string(),
                                           "/",
                                           base::Time(),
                                           false,
                                           true,
                                           COOKIE_PRIORITY_DEFAULT));

    // Secure cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2_secure,
                                           "sec_dom",
                                           "X",
                                           ".math.harvard.edu",
                                           "/",
                                           base::Time(),
                                           true,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2_secure,
                                           "sec_host",
                                           "X",
                                           std::string(),
                                           "/",
                                           base::Time(),
                                           true,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));

    // Domain path cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2,
                                           "dom_path_1",
                                           "X",
                                           ".math.harvard.edu",
                                           "/dir1",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2,
                                           "dom_path_2",
                                           "X",
                                           ".math.harvard.edu",
                                           "/dir1/dir2",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));

    // Host path cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2,
                                           "host_path_1",
                                           "X",
                                           std::string(),
                                           "/dir1",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));
    EXPECT_TRUE(this->SetCookieWithDetails(cm.get(),
                                           url_top_level_domain_plus_2,
                                           "host_path_2",
                                           "X",
                                           std::string(),
                                           "/dir1/dir2",
                                           base::Time(),
                                           false,
                                           false,
                                           COOKIE_PRIORITY_DEFAULT));

    EXPECT_EQ(13U, this->GetAllCookies(cm.get()).size());
  }

  Time GetFirstCookieAccessDate(CookieMonster* cm) {
    const CookieList all_cookies(this->GetAllCookies(cm));
    return all_cookies.front().LastAccessDate();
  }

  bool FindAndDeleteCookie(CookieMonster* cm,
                           const std::string& domain,
                           const std::string& name) {
    CookieList cookies = this->GetAllCookies(cm);
    for (CookieList::iterator it = cookies.begin();
         it != cookies.end(); ++it)
      if (it->Domain() == domain && it->Name() == name)
        return this->DeleteCanonicalCookie(cm, *it);
    return false;
  }

  int CountInString(const std::string& str, char c) {
    return std::count(str.begin(), str.end(), c);
  }

  void TestHostGarbageCollectHelper() {
    int domain_max_cookies = CookieMonster::kDomainMaxCookies;
    int domain_purge_cookies = CookieMonster::kDomainPurgeCookies;
    const int more_than_enough_cookies =
        (domain_max_cookies + domain_purge_cookies) * 2;
    // Add a bunch of cookies on a single host, should purge them.
    {
      scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
      for (int i = 0; i < more_than_enough_cookies; ++i) {
        std::string cookie = base::StringPrintf("a%03d=b", i);
        EXPECT_TRUE(SetCookie(cm.get(), url_google_, cookie));
        std::string cookies = this->GetCookies(cm.get(), url_google_);
        // Make sure we find it in the cookies.
        EXPECT_NE(cookies.find(cookie), std::string::npos);
        // Count the number of cookies.
        EXPECT_LE(CountInString(cookies, '='), domain_max_cookies);
      }
    }

    // Add a bunch of cookies on multiple hosts within a single eTLD.
    // Should keep at least kDomainMaxCookies - kDomainPurgeCookies
    // between them.  We shouldn't go above kDomainMaxCookies for both together.
    GURL url_google_specific(kUrlGoogleSpecific);
    {
      scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
      for (int i = 0; i < more_than_enough_cookies; ++i) {
        std::string cookie_general = base::StringPrintf("a%03d=b", i);
        EXPECT_TRUE(SetCookie(cm.get(), url_google_, cookie_general));
        std::string cookie_specific = base::StringPrintf("c%03d=b", i);
        EXPECT_TRUE(SetCookie(cm.get(), url_google_specific, cookie_specific));
        std::string cookies_general = this->GetCookies(cm.get(), url_google_);
        EXPECT_NE(cookies_general.find(cookie_general), std::string::npos);
        std::string cookies_specific =
            this->GetCookies(cm.get(), url_google_specific);
        EXPECT_NE(cookies_specific.find(cookie_specific), std::string::npos);
        EXPECT_LE((CountInString(cookies_general, '=') +
                   CountInString(cookies_specific, '=')),
                  domain_max_cookies);
      }
      // After all this, there should be at least
      // kDomainMaxCookies - kDomainPurgeCookies for both URLs.
      std::string cookies_general = this->GetCookies(cm.get(), url_google_);
      std::string cookies_specific =
          this->GetCookies(cm.get(), url_google_specific);
      int total_cookies = (CountInString(cookies_general, '=') +
                           CountInString(cookies_specific, '='));
      EXPECT_GE(total_cookies, domain_max_cookies - domain_purge_cookies);
      EXPECT_LE(total_cookies, domain_max_cookies);
    }
  }

  CookiePriority CharToPriority(char ch) {
    switch (ch) {
      case 'L':
        return COOKIE_PRIORITY_LOW;
      case 'M':
        return COOKIE_PRIORITY_MEDIUM;
      case 'H':
        return COOKIE_PRIORITY_HIGH;
    }
    NOTREACHED();
    return COOKIE_PRIORITY_DEFAULT;
  }

  // Instantiates a CookieMonster, adds multiple cookies (to url_google_) with
  // priorities specified by |coded_priority_str|, and tests priority-aware
  // domain cookie eviction.
  // |coded_priority_str| specifies a run-length-encoded string of priorities.
  // Example: "2M 3L M 4H" means "MMLLLMHHHH", and speicifies sequential (i.e.,
  // from least- to most-recently accessed) insertion of 2 medium-priority
  // cookies, 3 low-priority cookies, 1 medium-priority cookie, and 4
  // high-priority cookies.
  // Within each priority, only the least-accessed cookies should be evicted.
  // Thus, to describe expected suriving cookies, it suffices to specify the
  // expected population of surviving cookies per priority, i.e.,
  // |expected_low_count|, |expected_medium_count|, and |expected_high_count|.
  void TestPriorityCookieCase(CookieMonster* cm,
                              const std::string& coded_priority_str,
                              size_t expected_low_count,
                              size_t expected_medium_count,
                              size_t expected_high_count) {
    DeleteAll(cm);
    int next_cookie_id = 0;
    std::vector<CookiePriority> priority_list;
    std::vector<int> id_list[3];  // Indexed by CookiePriority.

    // Parse |coded_priority_str| and add cookies.
    std::vector<std::string> priority_tok_list;
    base::SplitString(coded_priority_str, ' ', &priority_tok_list);
    for (std::vector<std::string>::iterator it = priority_tok_list.begin();
         it != priority_tok_list.end(); ++it) {
      size_t len = it->length();
      DCHECK_NE(len, 0U);
      // Take last character as priority.
      CookiePriority priority = CharToPriority((*it)[len - 1]);
      std::string priority_str = CookiePriorityToString(priority);
      // The rest of the string (possibly empty) specifies repetition.
      int rep = 1;
      if (!it->empty()) {
        bool result = base::StringToInt(
            base::StringPiece(it->begin(), it->end() - 1), &rep);
        DCHECK(result);
      }
      for (; rep > 0; --rep, ++next_cookie_id) {
        std::string cookie = base::StringPrintf(
            "a%d=b;priority=%s", next_cookie_id, priority_str.c_str());
        EXPECT_TRUE(SetCookie(cm, url_google_, cookie));
        priority_list.push_back(priority);
        id_list[priority].push_back(next_cookie_id);
      }
    }

    int num_cookies = static_cast<int>(priority_list.size());
    std::vector<int> surviving_id_list[3];  // Indexed by CookiePriority.

    // Parse the list of cookies
    std::string cookie_str = this->GetCookies(cm, url_google_);
    std::vector<std::string> cookie_tok_list;
    base::SplitString(cookie_str, ';', &cookie_tok_list);
    for (std::vector<std::string>::iterator it = cookie_tok_list.begin();
         it != cookie_tok_list.end(); ++it) {
      // Assuming *it is "a#=b", so extract and parse "#" portion.
      int id = -1;
      bool result = base::StringToInt(
          base::StringPiece(it->begin() + 1, it->end() - 2), &id);
      DCHECK(result);
      DCHECK_GE(id, 0);
      DCHECK_LT(id, num_cookies);
      surviving_id_list[priority_list[id]].push_back(id);
    }

    // Validate each priority.
    size_t expected_count[3] = {
      expected_low_count, expected_medium_count, expected_high_count
    };
    for (int i = 0; i < 3; ++i) {
      DCHECK_LE(surviving_id_list[i].size(), id_list[i].size());
      EXPECT_EQ(expected_count[i], surviving_id_list[i].size());
      // Verify that the remaining cookies are the most recent among those
      // with the same priorities.
      if (expected_count[i] == surviving_id_list[i].size()) {
        std::sort(surviving_id_list[i].begin(), surviving_id_list[i].end());
        EXPECT_TRUE(std::equal(surviving_id_list[i].begin(),
                               surviving_id_list[i].end(),
                               id_list[i].end() - expected_count[i]));
      }
    }
  }

  void TestPriorityAwareGarbageCollectHelper() {
    // Hard-coding limits in the test, but use DCHECK_EQ to enforce constraint.
    DCHECK_EQ(180U, CookieMonster::kDomainMaxCookies);
    DCHECK_EQ(150U, CookieMonster::kDomainMaxCookies -
              CookieMonster::kDomainPurgeCookies);
    DCHECK_EQ(30U, CookieMonster::kDomainCookiesQuotaLow);
    DCHECK_EQ(50U, CookieMonster::kDomainCookiesQuotaMedium);
    DCHECK_EQ(70U, CookieMonster::kDomainCookiesQuotaHigh);

    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

    // Each test case adds 181 cookies, so 31 cookies are evicted.
    // Cookie same priority, repeated for each priority.
    TestPriorityCookieCase(cm.get(), "181L", 150U, 0U, 0U);
    TestPriorityCookieCase(cm.get(), "181M", 0U, 150U, 0U);
    TestPriorityCookieCase(cm.get(), "181H", 0U, 0U, 150U);

    // Pairwise scenarios.
    // Round 1 => none; round2 => 31M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "10H 171M", 0U, 140U, 10U);
    // Round 1 => 10L; round2 => 21M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "141M 40L", 30U, 120U, 0U);
    // Round 1 => none; round2 => none; round 3 => 31H.
    TestPriorityCookieCase(cm.get(), "101H 80M", 0U, 80U, 70U);

    // For {low, medium} priorities right on quota, different orders.
    // Round 1 => 1L; round 2 => none, round3 => 30L.
    TestPriorityCookieCase(cm.get(), "31L 50M 100H", 0U, 50U, 100U);
    // Round 1 => none; round 2 => 1M, round3 => 30M.
    TestPriorityCookieCase(cm.get(), "51M 100H 30L", 30U, 20U, 100U);
    // Round 1 => none; round 2 => none; round3 => 31H.
    TestPriorityCookieCase(cm.get(), "101H 50M 30L", 30U, 50U, 70U);

    // Round 1 => 10L; round 2 => 10M; round3 => 11H.
    TestPriorityCookieCase(cm.get(), "81H 60M 40L", 30U, 50U, 70U);

    // More complex scenarios.
    // Round 1 => 10L; round 2 => 10M; round 3 => 11H.
    TestPriorityCookieCase(cm.get(), "21H 60M 40L 60H", 30U, 50U, 70U);
    // Round 1 => 10L; round 2 => 11M, 10L; round 3 => none.
    TestPriorityCookieCase(
        cm.get(), "11H 10M 20L 110M 20L 10H", 20U, 109U, 21U);
    // Round 1 => none; round 2 => none; round 3 => 11L, 10M, 10H.
    TestPriorityCookieCase(cm.get(), "11L 10M 140H 10M 10L", 10U, 10U, 130U);
    // Round 1 => none; round 2 => 1M; round 3 => 10L, 10M, 10H.
    TestPriorityCookieCase(cm.get(), "11M 10H 10L 60M 90H", 0U, 60U, 90U);
    // Round 1 => none; round 2 => 10L, 21M; round 3 => none.
    TestPriorityCookieCase(cm.get(), "11M 10H 10L 90M 60H", 0U, 80U, 70U);
  }

  // Function for creating a CM with a number of cookies in it,
  // no store (and hence no ability to affect access time).
  CookieMonster* CreateMonsterForGC(int num_cookies) {
    CookieMonster* cm(new CookieMonster(NULL, NULL));
    for (int i = 0; i < num_cookies; i++) {
      SetCookie(cm, GURL(base::StringPrintf("http://h%05d.izzle", i)), "a=1");
    }
    return cm;
  }
};

// TODO(erikwright): Replace the other callbacks and synchronous helper methods
// in this test suite with these Mocks.
template<typename T, typename C> class MockCookieCallback {
 public:
  C AsCallback() {
    return base::Bind(&T::Invoke, base::Unretained(static_cast<T*>(this)));
  }
};

class MockGetCookiesCallback
  : public MockCookieCallback<MockGetCookiesCallback,
                              CookieStore::GetCookiesCallback> {
 public:
  MOCK_METHOD1(Invoke, void(const std::string& cookies));
};

class MockSetCookiesCallback
  : public MockCookieCallback<MockSetCookiesCallback,
                              CookieStore::SetCookiesCallback> {
 public:
  MOCK_METHOD1(Invoke, void(bool success));
};

class MockClosure
  : public MockCookieCallback<MockClosure, base::Closure> {
 public:
  MOCK_METHOD0(Invoke, void(void));
};

class MockGetCookieListCallback
  : public MockCookieCallback<MockGetCookieListCallback,
                              CookieMonster::GetCookieListCallback> {
 public:
  MOCK_METHOD1(Invoke, void(const CookieList& cookies));
};

class MockDeleteCallback
  : public MockCookieCallback<MockDeleteCallback,
                              CookieMonster::DeleteCallback> {
 public:
  MOCK_METHOD1(Invoke, void(int num_deleted));
};

class MockDeleteCookieCallback
  : public MockCookieCallback<MockDeleteCookieCallback,
                              CookieMonster::DeleteCookieCallback> {
 public:
  MOCK_METHOD1(Invoke, void(bool success));
};

struct CookiesInputInfo {
  const GURL url;
  const std::string name;
  const std::string value;
  const std::string domain;
  const std::string path;
  const base::Time expiration_time;
  bool secure;
  bool http_only;
  CookiePriority priority;
};

ACTION(QuitCurrentMessageLoop) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

// TODO(erikwright): When the synchronous helpers 'GetCookies' etc. are removed,
// rename these, removing the 'Action' suffix.
ACTION_P4(DeleteCookieAction, cookie_monster, url, name, callback) {
  cookie_monster->DeleteCookieAsync(url, name, callback->AsCallback());
}
ACTION_P3(GetCookiesAction, cookie_monster, url, callback) {
  cookie_monster->GetCookiesWithOptionsAsync(
      url, CookieOptions(), callback->AsCallback());
}
ACTION_P4(SetCookieAction, cookie_monster, url, cookie_line, callback) {
  cookie_monster->SetCookieWithOptionsAsync(
      url, cookie_line, CookieOptions(), callback->AsCallback());
}
ACTION_P4(DeleteAllCreatedBetweenAction,
          cookie_monster, delete_begin, delete_end, callback) {
  cookie_monster->DeleteAllCreatedBetweenAsync(
      delete_begin, delete_end, callback->AsCallback());
}
ACTION_P3(SetCookieWithDetailsAction, cookie_monster, cc, callback) {
  cookie_monster->SetCookieWithDetailsAsync(
      cc.url, cc.name, cc.value, cc.domain, cc.path, cc.expiration_time,
      cc.secure, cc.http_only, cc.priority,
      callback->AsCallback());
}

ACTION_P2(GetAllCookiesAction, cookie_monster, callback) {
  cookie_monster->GetAllCookiesAsync(callback->AsCallback());
}

ACTION_P3(DeleteAllForHostAction, cookie_monster, url, callback) {
  cookie_monster->DeleteAllForHostAsync(url, callback->AsCallback());
}

ACTION_P3(DeleteCanonicalCookieAction, cookie_monster, cookie, callback) {
  cookie_monster->DeleteCanonicalCookieAsync(cookie, callback->AsCallback());
}

ACTION_P2(DeleteAllAction, cookie_monster, callback) {
  cookie_monster->DeleteAllAsync(callback->AsCallback());
}

ACTION_P3(GetAllCookiesForUrlWithOptionsAction, cookie_monster, url, callback) {
  cookie_monster->GetAllCookiesForURLWithOptionsAsync(
      url, CookieOptions(), callback->AsCallback());
}

ACTION_P3(GetAllCookiesForUrlAction, cookie_monster, url, callback) {
  cookie_monster->GetAllCookiesForURLAsync(url, callback->AsCallback());
}

ACTION_P(PushCallbackAction, callback_vector) {
  callback_vector->push(arg1);
}

ACTION_P2(DeleteSessionCookiesAction, cookie_monster, callback) {
  cookie_monster->DeleteSessionCookiesAsync(callback->AsCallback());
}

}  // namespace

// This test suite verifies the task deferral behaviour of the CookieMonster.
// Specifically, for each asynchronous method, verify that:
// 1. invoking it on an uninitialized cookie store causes the store to begin
//    chain-loading its backing data or loading data for a specific domain key
//    (eTLD+1).
// 2. The initial invocation does not complete until the loading completes.
// 3. Invocations after the loading has completed complete immediately.
class DeferredCookieTaskTest : public CookieMonsterTest {
 protected:
  DeferredCookieTaskTest() {
    persistent_store_ = new NewMockPersistentCookieStore();
    cookie_monster_ = new CookieMonster(persistent_store_.get(), NULL);
  }

  // Defines a cookie to be returned from PersistentCookieStore::Load
  void DeclareLoadedCookie(const std::string& key,
                           const std::string& cookie_line,
                           const base::Time& creation_time) {
    AddCookieToList(key, cookie_line, creation_time, &loaded_cookies_);
  }

  // Runs the message loop, waiting until PersistentCookieStore::Load is called.
  // Call CompleteLoadingAndWait to cause the load to complete.
  void WaitForLoadCall() {
    RunFor(kTimeout);

    // Verify that PeristentStore::Load was called.
    testing::Mock::VerifyAndClear(persistent_store_.get());
  }

  // Invokes the PersistentCookieStore::LoadCookiesForKey completion callbacks
  // and PersistentCookieStore::Load completion callback and waits
  // until the message loop is quit.
  void CompleteLoadingAndWait() {
    while (!loaded_for_key_callbacks_.empty()) {
      loaded_for_key_callbacks_.front().Run(loaded_cookies_);
      loaded_cookies_.clear();
      loaded_for_key_callbacks_.pop();
    }

    loaded_callback_.Run(loaded_cookies_);
    RunFor(kTimeout);
  }

  // Performs the provided action, expecting it to cause a call to
  // PersistentCookieStore::Load. Call WaitForLoadCall to verify the load call
  // is received.
  void BeginWith(testing::Action<void(void)> action) {
    EXPECT_CALL(*this, Begin()).WillOnce(action);
    ExpectLoadCall();
    Begin();
  }

  void BeginWithForDomainKey(std::string key,
                             testing::Action<void(void)> action) {
    EXPECT_CALL(*this, Begin()).WillOnce(action);
    ExpectLoadCall();
    ExpectLoadForKeyCall(key, false);
    Begin();
  }

  // Declares an expectation that PersistentCookieStore::Load will be called,
  // saving the provided callback and sending a quit to the message loop.
  void ExpectLoadCall() {
    EXPECT_CALL(*persistent_store_.get(), Load(testing::_))
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&loaded_callback_),
                                 QuitCurrentMessageLoop()));
  }

  // Declares an expectation that PersistentCookieStore::LoadCookiesForKey
  // will be called, saving the provided callback and sending a quit to the
  // message loop.
  void ExpectLoadForKeyCall(std::string key, bool quit_queue) {
    if (quit_queue)
      EXPECT_CALL(*persistent_store_.get(), LoadCookiesForKey(key, testing::_))
          .WillOnce(
               testing::DoAll(PushCallbackAction(&loaded_for_key_callbacks_),
                              QuitCurrentMessageLoop()));
    else
      EXPECT_CALL(*persistent_store_.get(), LoadCookiesForKey(key, testing::_))
          .WillOnce(PushCallbackAction(&loaded_for_key_callbacks_));
  }

  // Invokes the initial action.
  MOCK_METHOD0(Begin, void(void));

  // Returns the CookieMonster instance under test.
  CookieMonster& cookie_monster() { return *cookie_monster_.get(); }

 private:
  // Declares that mock expectations in this test suite are strictly ordered.
  testing::InSequence in_sequence_;
  // Holds cookies to be returned from PersistentCookieStore::Load or
  // PersistentCookieStore::LoadCookiesForKey.
  std::vector<CanonicalCookie*> loaded_cookies_;
  // Stores the callback passed from the CookieMonster to the
  // PersistentCookieStore::Load
  CookieMonster::PersistentCookieStore::LoadedCallback loaded_callback_;
  // Stores the callback passed from the CookieMonster to the
  // PersistentCookieStore::LoadCookiesForKey
  std::queue<CookieMonster::PersistentCookieStore::LoadedCallback>
    loaded_for_key_callbacks_;

  // Stores the CookieMonster under test.
  scoped_refptr<CookieMonster> cookie_monster_;
  // Stores the mock PersistentCookieStore.
  scoped_refptr<NewMockPersistentCookieStore> persistent_store_;
};

TEST_F(DeferredCookieTaskTest, DeferredGetCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookiesCallback get_cookies_callback;

  BeginWithForDomainKey("google.izzle", GetCookiesAction(
      &cookie_monster(), url_google_, &get_cookies_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookies_callback, Invoke("X=1")).WillOnce(
      GetCookiesAction(&cookie_monster(), url_google_, &get_cookies_callback));
  EXPECT_CALL(get_cookies_callback, Invoke("X=1")).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredSetCookie) {
  MockSetCookiesCallback set_cookies_callback;

  BeginWithForDomainKey("google.izzle", SetCookieAction(
      &cookie_monster(), url_google_, "A=B", &set_cookies_callback));

  WaitForLoadCall();

  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      SetCookieAction(
          &cookie_monster(), url_google_, "X=Y", &set_cookies_callback));
  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteCookie) {
  MockClosure delete_cookie_callback;

  BeginWithForDomainKey("google.izzle", DeleteCookieAction(
      &cookie_monster(), url_google_, "A", &delete_cookie_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_cookie_callback, Invoke()).WillOnce(
      DeleteCookieAction(
          &cookie_monster(), url_google_, "X", &delete_cookie_callback));
  EXPECT_CALL(delete_cookie_callback, Invoke()).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredSetCookieWithDetails) {
  MockSetCookiesCallback set_cookies_callback;

  CookiesInputInfo cookie_info = {
    url_google_foo_, "A", "B", std::string(), "/foo",
    base::Time(), false, false, COOKIE_PRIORITY_DEFAULT
  };
  BeginWithForDomainKey("google.izzle", SetCookieWithDetailsAction(
      &cookie_monster(), cookie_info, &set_cookies_callback));

  WaitForLoadCall();

  CookiesInputInfo cookie_info_exp = {
    url_google_foo_, "A", "B", std::string(), "/foo",
    base::Time(), false, false, COOKIE_PRIORITY_DEFAULT
  };
  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      SetCookieWithDetailsAction(
          &cookie_monster(), cookie_info_exp, &set_cookies_callback));
  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookieListCallback get_cookie_list_callback;

  BeginWith(GetAllCookiesAction(
      &cookie_monster(), &get_cookie_list_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      GetAllCookiesAction(&cookie_monster(), &get_cookie_list_callback));
  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllForUrlCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookieListCallback get_cookie_list_callback;

  BeginWithForDomainKey("google.izzle", GetAllCookiesForUrlAction(
      &cookie_monster(), url_google_, &get_cookie_list_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      GetAllCookiesForUrlAction(
          &cookie_monster(), url_google_, &get_cookie_list_callback));
  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllForUrlWithOptionsCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookieListCallback get_cookie_list_callback;

  BeginWithForDomainKey("google.izzle", GetAllCookiesForUrlWithOptionsAction(
      &cookie_monster(), url_google_, &get_cookie_list_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      GetAllCookiesForUrlWithOptionsAction(
          &cookie_monster(), url_google_, &get_cookie_list_callback));
  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllCookies) {
  MockDeleteCallback delete_callback;

  BeginWith(DeleteAllAction(
      &cookie_monster(), &delete_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      DeleteAllAction(&cookie_monster(), &delete_callback));
  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllCreatedBetweenCookies) {
  MockDeleteCallback delete_callback;

  BeginWith(DeleteAllCreatedBetweenAction(
      &cookie_monster(), base::Time(), base::Time::Now(), &delete_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      DeleteAllCreatedBetweenAction(
          &cookie_monster(), base::Time(), base::Time::Now(),
          &delete_callback));
  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllForHostCookies) {
  MockDeleteCallback delete_callback;

  BeginWithForDomainKey("google.izzle", DeleteAllForHostAction(
      &cookie_monster(), url_google_, &delete_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      DeleteAllForHostAction(
          &cookie_monster(), url_google_, &delete_callback));
  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteCanonicalCookie) {
  std::vector<CanonicalCookie*> cookies;
  CanonicalCookie cookie = BuildCanonicalCookie(
      "www.google.com", "X=1; path=/", base::Time::Now());

  MockDeleteCookieCallback delete_cookie_callback;

  BeginWith(DeleteCanonicalCookieAction(
      &cookie_monster(), cookie, &delete_cookie_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_cookie_callback, Invoke(false)).WillOnce(
      DeleteCanonicalCookieAction(
      &cookie_monster(), cookie, &delete_cookie_callback));
  EXPECT_CALL(delete_cookie_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteSessionCookies) {
  MockDeleteCallback delete_callback;

  BeginWith(DeleteSessionCookiesAction(
      &cookie_monster(), &delete_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      DeleteSessionCookiesAction(&cookie_monster(), &delete_callback));
  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

// Verify that a series of queued tasks are executed in order upon loading of
// the backing store and that new tasks received while the queued tasks are
// being dispatched go to the end of the queue.
TEST_F(DeferredCookieTaskTest, DeferredTaskOrder) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookiesCallback get_cookies_callback;
  MockSetCookiesCallback set_cookies_callback;
  MockGetCookiesCallback get_cookies_callback_deferred;

  EXPECT_CALL(*this, Begin()).WillOnce(testing::DoAll(
      GetCookiesAction(
          &cookie_monster(), url_google_, &get_cookies_callback),
      SetCookieAction(
          &cookie_monster(), url_google_, "A=B", &set_cookies_callback)));
  ExpectLoadCall();
  ExpectLoadForKeyCall("google.izzle", false);
  Begin();

  WaitForLoadCall();
  EXPECT_CALL(get_cookies_callback, Invoke("X=1")).WillOnce(
      GetCookiesAction(
          &cookie_monster(), url_google_, &get_cookies_callback_deferred));
  EXPECT_CALL(set_cookies_callback, Invoke(true));
  EXPECT_CALL(get_cookies_callback_deferred, Invoke("A=B; X=1")).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(CookieMonsterTest, TestCookieDeleteAll) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store.get(), NULL));
  CookieOptions options;
  options.set_include_httponly();

  EXPECT_TRUE(SetCookie(cm.get(), url_google_, kValidCookieLine));
  EXPECT_EQ("A=B", GetCookies(cm.get(), url_google_));

  EXPECT_TRUE(
      SetCookieWithOptions(cm.get(), url_google_, "C=D; httponly", options));
  EXPECT_EQ("A=B; C=D", GetCookiesWithOptions(cm.get(), url_google_, options));

  EXPECT_EQ(2, DeleteAll(cm.get()));
  EXPECT_EQ("", GetCookiesWithOptions(cm.get(), url_google_, options));
  EXPECT_EQ(0u, store->commands().size());

  // Create a persistent cookie.
  EXPECT_TRUE(SetCookie(
      cm.get(),
      url_google_,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  EXPECT_EQ(1, DeleteAll(cm.get()));  // sync_to_store = true.
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);

  EXPECT_EQ("", GetCookiesWithOptions(cm.get(), url_google_, options));
}

TEST_F(CookieMonsterTest, TestCookieDeleteAllCreatedBetweenTimestamps) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(
      0,
      DeleteAllCreatedBetween(cm.get(), now - TimeDelta::FromDays(99), Time()));

  // Create 3 cookies with creation date of today, yesterday and the day before.
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-0=Now", now));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-1=Yesterday",
                                            now - TimeDelta::FromDays(1)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-2=DayBefore",
                                            now - TimeDelta::FromDays(2)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-3=ThreeDays",
                                            now - TimeDelta::FromDays(3)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-7=LastWeek",
                                            now - TimeDelta::FromDays(7)));

  // Try to delete threedays and the daybefore.
  EXPECT_EQ(2,
            DeleteAllCreatedBetween(cm.get(),
                                    now - TimeDelta::FromDays(3),
                                    now - TimeDelta::FromDays(1)));

  // Try to delete yesterday, also make sure that delete_end is not
  // inclusive.
  EXPECT_EQ(
      1, DeleteAllCreatedBetween(cm.get(), now - TimeDelta::FromDays(2), now));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(
      1, DeleteAllCreatedBetween(cm.get(), now - TimeDelta::FromDays(7), now));

  // Delete the last (now) item.
  EXPECT_EQ(1, DeleteAllCreatedBetween(cm.get(), Time(), Time()));

  // Really make sure everything is gone.
  EXPECT_EQ(0, DeleteAll(cm.get()));
}

static const int kAccessDelayMs = kLastAccessThresholdMilliseconds + 20;

TEST_F(CookieMonsterTest, TestLastAccess) {
  scoped_refptr<CookieMonster> cm(
      new CookieMonster(NULL, NULL, kLastAccessThresholdMilliseconds));

  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B"));
  const Time last_access_date(GetFirstCookieAccessDate(cm.get()));

  // Reading the cookie again immediately shouldn't update the access date,
  // since we're inside the threshold.
  EXPECT_EQ("A=B", GetCookies(cm.get(), url_google_));
  EXPECT_TRUE(last_access_date == GetFirstCookieAccessDate(cm.get()));

  // Reading after a short wait should update the access date.
  base::PlatformThread::Sleep(
      base::TimeDelta::FromMilliseconds(kAccessDelayMs));
  EXPECT_EQ("A=B", GetCookies(cm.get(), url_google_));
  EXPECT_FALSE(last_access_date == GetFirstCookieAccessDate(cm.get()));
}

TEST_F(CookieMonsterTest, TestHostGarbageCollection) {
  TestHostGarbageCollectHelper();
}

TEST_F(CookieMonsterTest, TestPriorityAwareGarbageCollection) {
  TestPriorityAwareGarbageCollectHelper();
}

TEST_F(CookieMonsterTest, TestDeleteSingleCookie) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "C=D"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "E=F"));
  EXPECT_EQ("A=B; C=D; E=F", GetCookies(cm.get(), url_google_));

  EXPECT_TRUE(FindAndDeleteCookie(cm.get(), url_google_.host(), "C"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm.get(), url_google_));

  EXPECT_FALSE(FindAndDeleteCookie(cm.get(), "random.host", "E"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm.get(), url_google_));
}

TEST_F(CookieMonsterTest, SetCookieableSchemes) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  scoped_refptr<CookieMonster> cm_foo(new CookieMonster(NULL, NULL));

  // Only cm_foo should allow foo:// cookies.
  const char* kSchemes[] = {"foo"};
  cm_foo->SetCookieableSchemes(kSchemes, 1);

  GURL foo_url("foo://host/path");
  GURL http_url("http://host/path");

  EXPECT_TRUE(SetCookie(cm.get(), http_url, "x=1"));
  EXPECT_FALSE(SetCookie(cm.get(), foo_url, "x=1"));
  EXPECT_TRUE(SetCookie(cm_foo.get(), foo_url, "x=1"));
  EXPECT_FALSE(SetCookie(cm_foo.get(), http_url, "x=1"));
}

TEST_F(CookieMonsterTest, GetAllCookiesForURL) {
  scoped_refptr<CookieMonster> cm(
      new CookieMonster(NULL, NULL, kLastAccessThresholdMilliseconds));

  // Create an httponly cookie.
  CookieOptions options;
  options.set_include_httponly();

  EXPECT_TRUE(
      SetCookieWithOptions(cm.get(), url_google_, "A=B; httponly", options));
  EXPECT_TRUE(SetCookieWithOptions(
      cm.get(), url_google_, "C=D; domain=.google.izzle", options));
  EXPECT_TRUE(SetCookieWithOptions(cm.get(),
                                   url_google_secure_,
                                   "E=F; domain=.google.izzle; secure",
                                   options));

  const Time last_access_date(GetFirstCookieAccessDate(cm.get()));

  base::PlatformThread::Sleep(
      base::TimeDelta::FromMilliseconds(kAccessDelayMs));

  // Check cookies for url.
  CookieList cookies = GetAllCookiesForURL(cm.get(), url_google_);
  CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Check cookies for url excluding http-only cookies.
  cookies =
      GetAllCookiesForURLWithOptions(cm.get(), url_google_, CookieOptions());
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Test secure cookies.
  cookies = GetAllCookiesForURL(cm.get(), url_google_secure_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("E", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Reading after a short wait should not update the access date.
  EXPECT_TRUE(last_access_date == GetFirstCookieAccessDate(cm.get()));
}

TEST_F(CookieMonsterTest, GetAllCookiesForURLPathMatching) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;

  EXPECT_TRUE(SetCookieWithOptions(
      cm.get(), url_google_foo_, "A=B; path=/foo;", options));
  EXPECT_TRUE(SetCookieWithOptions(
      cm.get(), url_google_bar_, "C=D; path=/bar;", options));
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "E=F;", options));

  CookieList cookies = GetAllCookiesForURL(cm.get(), url_google_foo_);
  CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("/foo", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());

  cookies = GetAllCookiesForURL(cm.get(), url_google_bar_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("/bar", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());
}

TEST_F(CookieMonsterTest, DeleteCookieByName) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=A1; path=/"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=A2; path=/foo"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=A3; path=/bar"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "B=B1; path=/"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "B=B2; path=/foo"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "B=B3; path=/bar"));

  DeleteCookie(cm.get(), GURL(std::string(kUrlGoogle) + "/foo/bar"), "A");

  CookieList cookies = GetAllCookies(cm.get());
  size_t expected_size = 4;
  EXPECT_EQ(expected_size, cookies.size());
  for (CookieList::iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    EXPECT_NE("A1", it->Value());
    EXPECT_NE("A2", it->Value());
  }
}

TEST_F(CookieMonsterTest, InitializeFromCookieMonster) {
  scoped_refptr<CookieMonster> cm_1(new CookieMonster(NULL, NULL));
  CookieOptions options;

  EXPECT_TRUE(SetCookieWithOptions(cm_1.get(), url_google_foo_,
                                               "A1=B; path=/foo;",
                                               options));
  EXPECT_TRUE(SetCookieWithOptions(cm_1.get(), url_google_bar_,
                                               "A2=D; path=/bar;",
                                               options));
  EXPECT_TRUE(SetCookieWithOptions(cm_1.get(), url_google_,
                                               "A3=F;",
                                               options));

  CookieList cookies_1 = GetAllCookies(cm_1.get());
  scoped_refptr<CookieMonster> cm_2(new CookieMonster(NULL, NULL));
  ASSERT_TRUE(cm_2->InitializeFrom(cookies_1));
  CookieList cookies_2 = GetAllCookies(cm_2.get());

  size_t expected_size = 3;
  EXPECT_EQ(expected_size, cookies_2.size());

  CookieList::iterator it = cookies_2.begin();

  ASSERT_TRUE(it != cookies_2.end());
  EXPECT_EQ("A1", it->Name());
  EXPECT_EQ("/foo", it->Path());

  ASSERT_TRUE(++it != cookies_2.end());
  EXPECT_EQ("A2", it->Name());
  EXPECT_EQ("/bar", it->Path());

  ASSERT_TRUE(++it != cookies_2.end());
  EXPECT_EQ("A3", it->Name());
  EXPECT_EQ("/", it->Path());
}

// Tests importing from a persistent cookie store that contains duplicate
// equivalent cookies. This situation should be handled by removing the
// duplicate cookie (both from the in-memory cache, and from the backing store).
//
// This is a regression test for: http://crbug.com/17855.
TEST_F(CookieMonsterTest, DontImportDuplicateCookies) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);

  // We will fill some initial cookies into the PersistentCookieStore,
  // to simulate a database with 4 duplicates.  Note that we need to
  // be careful not to have any duplicate creation times at all (as it's a
  // violation of a CookieMonster invariant) even if Time::Now() doesn't
  // move between calls.
  std::vector<CanonicalCookie*> initial_cookies;

  // Insert 4 cookies with name "X" on path "/", with varying creation
  // dates. We expect only the most recent one to be preserved following
  // the import.

  AddCookieToList("www.google.com",
                  "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(3),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=2; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(1),
                  &initial_cookies);

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList("www.google.com",
                  "X=3; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(4),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=4; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now(),
                  &initial_cookies);

  // Insert 2 cookies with name "X" on path "/2", with varying creation
  // dates. We expect only the most recent one to be preserved the import.

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList("www.google.com",
                  "X=a1; path=/2; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(9),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=a2; path=/2; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(2),
                  &initial_cookies);

  // Insert 1 cookie with name "Y" on path "/".
  AddCookieToList("www.google.com",
                  "Y=a; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(10),
                  &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<CookieMonster> cm(new CookieMonster(store.get(), NULL));

  // Verify that duplicates were not imported for path "/".
  // (If this had failed, GetCookies() would have also returned X=1, X=2, X=4).
  EXPECT_EQ("X=3; Y=a", GetCookies(cm.get(), GURL("http://www.google.com/")));

  // Verify that same-named cookie on a different path ("/x2") didn't get
  // messed up.
  EXPECT_EQ("X=a1; X=3; Y=a",
            GetCookies(cm.get(), GURL("http://www.google.com/2/x")));

  // Verify that the PersistentCookieStore was told to kill its 4 duplicates.
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[0].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[2].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
}

// Tests importing from a persistent cookie store that contains cookies
// with duplicate creation times.  This situation should be handled by
// dropping the cookies before insertion/visibility to user.
//
// This is a regression test for: http://crbug.com/43188.
TEST_F(CookieMonsterTest, DontImportDuplicateCreationTimes) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);

  Time now(Time::Now());
  Time earlier(now - TimeDelta::FromDays(1));

  // Insert 8 cookies, four with the current time as creation times, and
  // four with the earlier time as creation times.  We should only get
  // two cookies remaining, but which two (other than that there should
  // be one from each set) will be random.
  std::vector<CanonicalCookie*> initial_cookies;
  AddCookieToList("www.google.com", "X=1; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=2; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=3; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=4; path=/", now, &initial_cookies);

  AddCookieToList("www.google.com", "Y=1; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=2; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=3; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=4; path=/", earlier, &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<CookieMonster> cm(new CookieMonster(store.get(), NULL));

  CookieList list(GetAllCookies(cm.get()));
  EXPECT_EQ(2U, list.size());
  // Confirm that we have one of each.
  std::string name1(list[0].Name());
  std::string name2(list[1].Name());
  EXPECT_TRUE(name1 == "X" || name2 == "X");
  EXPECT_TRUE(name1 == "Y" || name2 == "Y");
  EXPECT_NE(name1, name2);
}

TEST_F(CookieMonsterTest, CookieMonsterDelegate) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<MockCookieMonsterDelegate> delegate(
      new MockCookieMonsterDelegate);
  scoped_refptr<CookieMonster> cm(
      new CookieMonster(store.get(), delegate.get()));

  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "C=D"));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "E=F"));
  EXPECT_EQ("A=B; C=D; E=F", GetCookies(cm.get(), url_google_));
  ASSERT_EQ(3u, delegate->changes().size());
  EXPECT_FALSE(delegate->changes()[0].second);
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_EQ("A", delegate->changes()[0].first.Name());
  EXPECT_EQ("B", delegate->changes()[0].first.Value());
  EXPECT_EQ(url_google_.host(), delegate->changes()[1].first.Domain());
  EXPECT_FALSE(delegate->changes()[1].second);
  EXPECT_EQ("C", delegate->changes()[1].first.Name());
  EXPECT_EQ("D", delegate->changes()[1].first.Value());
  EXPECT_EQ(url_google_.host(), delegate->changes()[2].first.Domain());
  EXPECT_FALSE(delegate->changes()[2].second);
  EXPECT_EQ("E", delegate->changes()[2].first.Name());
  EXPECT_EQ("F", delegate->changes()[2].first.Value());
  delegate->reset();

  EXPECT_TRUE(FindAndDeleteCookie(cm.get(), url_google_.host(), "C"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm.get(), url_google_));
  ASSERT_EQ(1u, delegate->changes().size());
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_TRUE(delegate->changes()[0].second);
  EXPECT_EQ("C", delegate->changes()[0].first.Name());
  EXPECT_EQ("D", delegate->changes()[0].first.Value());
  delegate->reset();

  EXPECT_FALSE(FindAndDeleteCookie(cm.get(), "random.host", "E"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm.get(), url_google_));
  EXPECT_EQ(0u, delegate->changes().size());

  // Insert a cookie "a" for path "/path1"
  EXPECT_TRUE(SetCookie(cm.get(),
                        url_google_,
                        "a=val1; path=/path1; "
                        "expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  ASSERT_EQ(1u, delegate->changes().size());
  EXPECT_FALSE(delegate->changes()[0].second);
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_EQ("a", delegate->changes()[0].first.Name());
  EXPECT_EQ("val1", delegate->changes()[0].first.Value());
  delegate->reset();

  // Insert a cookie "a" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(SetCookieWithOptions(cm.get(),
                                   url_google_,
                                   "a=val2; path=/path1; httponly; "
                                   "expires=Mon, 18-Apr-22 22:50:14 GMT",
                                   allow_httponly));
  ASSERT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  ASSERT_EQ(2u, delegate->changes().size());
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_TRUE(delegate->changes()[0].second);
  EXPECT_EQ("a", delegate->changes()[0].first.Name());
  EXPECT_EQ("val1", delegate->changes()[0].first.Value());
  EXPECT_EQ(url_google_.host(), delegate->changes()[1].first.Domain());
  EXPECT_FALSE(delegate->changes()[1].second);
  EXPECT_EQ("a", delegate->changes()[1].first.Name());
  EXPECT_EQ("val2", delegate->changes()[1].first.Value());
  delegate->reset();
}

TEST_F(CookieMonsterTest, SetCookieWithDetails) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookieWithDetails(cm.get(),
                                   url_google_foo_,
                                   "A",
                                   "B",
                                   std::string(),
                                   "/foo",
                                   base::Time(),
                                   false,
                                   false,
                                   COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(SetCookieWithDetails(cm.get(),
                                   url_google_bar_,
                                   "C",
                                   "D",
                                   "google.izzle",
                                   "/bar",
                                   base::Time(),
                                   false,
                                   true,
                                   COOKIE_PRIORITY_DEFAULT));
  EXPECT_TRUE(SetCookieWithDetails(cm.get(),
                                   url_google_,
                                   "E",
                                   "F",
                                   std::string(),
                                   std::string(),
                                   base::Time(),
                                   true,
                                   false,
                                   COOKIE_PRIORITY_DEFAULT));

  // Test that malformed attributes fail to set the cookie.
  EXPECT_FALSE(SetCookieWithDetails(cm.get(),
                                    url_google_foo_,
                                    " A",
                                    "B",
                                    std::string(),
                                    "/foo",
                                    base::Time(),
                                    false,
                                    false,
                                    COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(SetCookieWithDetails(cm.get(),
                                    url_google_foo_,
                                    "A;",
                                    "B",
                                    std::string(),
                                    "/foo",
                                    base::Time(),
                                    false,
                                    false,
                                    COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(SetCookieWithDetails(cm.get(),
                                    url_google_foo_,
                                    "A=",
                                    "B",
                                    std::string(),
                                    "/foo",
                                    base::Time(),
                                    false,
                                    false,
                                    COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(SetCookieWithDetails(cm.get(),
                                    url_google_foo_,
                                    "A",
                                    "B",
                                    "google.ozzzzzzle",
                                    "foo",
                                    base::Time(),
                                    false,
                                    false,
                                    COOKIE_PRIORITY_DEFAULT));
  EXPECT_FALSE(SetCookieWithDetails(cm.get(),
                                    url_google_foo_,
                                    "A=",
                                    "B",
                                    std::string(),
                                    "foo",
                                    base::Time(),
                                    false,
                                    false,
                                    COOKIE_PRIORITY_DEFAULT));

  CookieList cookies = GetAllCookiesForURL(cm.get(), url_google_foo_);
  CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("B", it->Value());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("/foo", it->Path());
  EXPECT_FALSE(it->IsPersistent());
  EXPECT_FALSE(it->IsSecure());
  EXPECT_FALSE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());

  cookies = GetAllCookiesForURL(cm.get(), url_google_bar_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("D", it->Value());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("/bar", it->Path());
  EXPECT_FALSE(it->IsSecure());
  EXPECT_TRUE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());

  cookies = GetAllCookiesForURL(cm.get(), url_google_secure_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("F", it->Value());
  EXPECT_EQ("/", it->Path());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_TRUE(it->IsSecure());
  EXPECT_FALSE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());
}

TEST_F(CookieMonsterTest, DeleteAllForHost) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  // Test probes:
  //    * Non-secure URL, mid-level (http://w.c.b.a)
  //    * Secure URL, mid-level (https://w.c.b.a)
  //    * URL with path, mid-level (https:/w.c.b.a/dir1/xx)
  // All three tests should nuke only the midlevel host cookie,
  // the http_only cookie, the host secure cookie, and the two host
  // path cookies.  http_only, secure, and paths are ignored by
  // this call, and domain cookies arent touched.
  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; host_2=X; sec_dom=X; sec_host=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; host_path_2=X; dom_path_1=X; host_path_1=X; "
            "dom_1=X; dom_2=X; host_2=X; sec_dom=X; sec_host=X",
            GetCookies(cm.get(),
                       GURL(kTopLevelDomainPlus2Secure +
                            std::string("/dir1/dir2/xxx"))));

  EXPECT_EQ(5, DeleteAllForHost(cm.get(), GURL(kTopLevelDomainPlus2)));
  EXPECT_EQ(8U, GetAllCookies(cm.get()).size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm.get(),
                       GURL(kTopLevelDomainPlus2Secure +
                            std::string("/dir1/dir2/xxx"))));

  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ(5, DeleteAllForHost(cm.get(), GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ(8U, GetAllCookies(cm.get()).size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm.get(),
                       GURL(kTopLevelDomainPlus2Secure +
                            std::string("/dir1/dir2/xxx"))));

  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ(5,
            DeleteAllForHost(
                cm.get(),
                GURL(kTopLevelDomainPlus2Secure + std::string("/dir1/xxx"))));
  EXPECT_EQ(8U, GetAllCookies(cm.get()).size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X",
            GetCookies(cm.get(), GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm.get(),
                       GURL(kTopLevelDomainPlus2Secure +
                            std::string("/dir1/dir2/xxx"))));
}

TEST_F(CookieMonsterTest, UniqueCreationTime) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;

  // Add in three cookies through every public interface to the
  // CookieMonster and confirm that none of them have duplicate
  // creation times.

  // SetCookieWithCreationTime and SetCookieWithCreationTimeAndOptions
  // are not included as they aren't going to be public for very much
  // longer.

  // SetCookie, SetCookieWithOptions, SetCookieWithDetails

  SetCookie(cm.get(), url_google_, "SetCookie1=A");
  SetCookie(cm.get(), url_google_, "SetCookie2=A");
  SetCookie(cm.get(), url_google_, "SetCookie3=A");

  SetCookieWithOptions(
      cm.get(), url_google_, "setCookieWithOptions1=A", options);
  SetCookieWithOptions(
      cm.get(), url_google_, "setCookieWithOptions2=A", options);
  SetCookieWithOptions(
      cm.get(), url_google_, "setCookieWithOptions3=A", options);

  SetCookieWithDetails(cm.get(),
                       url_google_,
                       "setCookieWithDetails1",
                       "A",
                       ".google.com",
                       "/",
                       Time(),
                       false,
                       false,
                       COOKIE_PRIORITY_DEFAULT);
  SetCookieWithDetails(cm.get(),
                       url_google_,
                       "setCookieWithDetails2",
                       "A",
                       ".google.com",
                       "/",
                       Time(),
                       false,
                       false,
                       COOKIE_PRIORITY_DEFAULT);
  SetCookieWithDetails(cm.get(),
                       url_google_,
                       "setCookieWithDetails3",
                       "A",
                       ".google.com",
                       "/",
                       Time(),
                       false,
                       false,
                       COOKIE_PRIORITY_DEFAULT);

  // Now we check
  CookieList cookie_list(GetAllCookies(cm.get()));
  typedef std::map<int64, CanonicalCookie> TimeCookieMap;
  TimeCookieMap check_map;
  for (CookieList::const_iterator it = cookie_list.begin();
       it != cookie_list.end(); it++) {
    const int64 creation_date = it->CreationDate().ToInternalValue();
    TimeCookieMap::const_iterator
        existing_cookie_it(check_map.find(creation_date));
    EXPECT_TRUE(existing_cookie_it == check_map.end())
        << "Cookie " << it->Name() << " has same creation date ("
        << it->CreationDate().ToInternalValue()
        << ") as previously entered cookie "
        << existing_cookie_it->second.Name();

    if (existing_cookie_it == check_map.end()) {
      check_map.insert(TimeCookieMap::value_type(
          it->CreationDate().ToInternalValue(), *it));
    }
  }
}

// Mainly a test of GetEffectiveDomain, or more specifically, of the
// expected behavior of GetEffectiveDomain within the CookieMonster.
TEST_F(CookieMonsterTest, GetKey) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  // This test is really only interesting if GetKey() actually does something.
  EXPECT_EQ("google.com", cm->GetKey("www.google.com"));
  EXPECT_EQ("google.izzie", cm->GetKey("www.google.izzie"));
  EXPECT_EQ("google.izzie", cm->GetKey(".google.izzie"));
  EXPECT_EQ("bbc.co.uk", cm->GetKey("bbc.co.uk"));
  EXPECT_EQ("bbc.co.uk", cm->GetKey("a.b.c.d.bbc.co.uk"));
  EXPECT_EQ("apple.com", cm->GetKey("a.b.c.d.apple.com"));
  EXPECT_EQ("apple.izzie", cm->GetKey("a.b.c.d.apple.izzie"));

  // Cases where the effective domain is null, so we use the host
  // as the key.
  EXPECT_EQ("co.uk", cm->GetKey("co.uk"));
  const std::string extension_name("iehocdgbbocmkdidlbnnfbmbinnahbae");
  EXPECT_EQ(extension_name, cm->GetKey(extension_name));
  EXPECT_EQ("com", cm->GetKey("com"));
  EXPECT_EQ("hostalias", cm->GetKey("hostalias"));
  EXPECT_EQ("localhost", cm->GetKey("localhost"));
}

// Test that cookies transfer from/to the backing store correctly.
TEST_F(CookieMonsterTest, BackingStoreCommunication) {
  // Store details for cookies transforming through the backing store interface.

  base::Time current(base::Time::Now());
  scoped_refptr<MockSimplePersistentCookieStore> store(
      new MockSimplePersistentCookieStore);
  base::Time new_access_time;
  base::Time expires(base::Time::Now() + base::TimeDelta::FromSeconds(100));

  const CookiesInputInfo input_info[] = {
    {GURL("http://a.b.google.com"), "a", "1", "", "/path/to/cookie", expires,
     false, false, COOKIE_PRIORITY_DEFAULT},
    {GURL("https://www.google.com"), "b", "2", ".google.com",
     "/path/from/cookie", expires + TimeDelta::FromSeconds(10),
     true, true, COOKIE_PRIORITY_DEFAULT},
    {GURL("https://google.com"), "c", "3", "", "/another/path/to/cookie",
     base::Time::Now() + base::TimeDelta::FromSeconds(100),
     true, false, COOKIE_PRIORITY_DEFAULT}
  };
  const int INPUT_DELETE = 1;

  // Create new cookies and flush them to the store.
  {
    scoped_refptr<CookieMonster> cmout(new CookieMonster(store.get(), NULL));
    for (const CookiesInputInfo* p = input_info;
         p < &input_info[ARRAYSIZE_UNSAFE(input_info)];
         p++) {
      EXPECT_TRUE(SetCookieWithDetails(cmout.get(),
                                       p->url,
                                       p->name,
                                       p->value,
                                       p->domain,
                                       p->path,
                                       p->expiration_time,
                                       p->secure,
                                       p->http_only,
                                       p->priority));
    }
    GURL del_url(input_info[INPUT_DELETE].url.Resolve(
                     input_info[INPUT_DELETE].path).spec());
    DeleteCookie(cmout.get(), del_url, input_info[INPUT_DELETE].name);
  }

  // Create a new cookie monster and make sure that everything is correct
  {
    scoped_refptr<CookieMonster> cmin(new CookieMonster(store.get(), NULL));
    CookieList cookies(GetAllCookies(cmin.get()));
    ASSERT_EQ(2u, cookies.size());
    // Ordering is path length, then creation time.  So second cookie
    // will come first, and we need to swap them.
    std::swap(cookies[0], cookies[1]);
    for (int output_index = 0; output_index < 2; output_index++) {
      int input_index = output_index * 2;
      const CookiesInputInfo* input = &input_info[input_index];
      const CanonicalCookie* output = &cookies[output_index];

      EXPECT_EQ(input->name, output->Name());
      EXPECT_EQ(input->value, output->Value());
      EXPECT_EQ(input->url.host(), output->Domain());
      EXPECT_EQ(input->path, output->Path());
      EXPECT_LE(current.ToInternalValue(),
                output->CreationDate().ToInternalValue());
      EXPECT_EQ(input->secure, output->IsSecure());
      EXPECT_EQ(input->http_only, output->IsHttpOnly());
      EXPECT_TRUE(output->IsPersistent());
      EXPECT_EQ(input->expiration_time.ToInternalValue(),
                output->ExpiryDate().ToInternalValue());
    }
  }
}

TEST_F(CookieMonsterTest, CookieListOrdering) {
  // Put a random set of cookies into a monster and make sure
  // they're returned in the right order.
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(
      SetCookie(cm.get(), GURL("http://d.c.b.a.google.com/aa/x.html"), "c=1"));
  EXPECT_TRUE(SetCookie(cm.get(),
                        GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                        "d=1; domain=b.a.google.com"));
  EXPECT_TRUE(SetCookie(cm.get(),
                        GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                        "a=4; domain=b.a.google.com"));
  EXPECT_TRUE(SetCookie(cm.get(),
                        GURL("http://c.b.a.google.com/aa/bb/cc/x.html"),
                        "e=1; domain=c.b.a.google.com"));
  EXPECT_TRUE(SetCookie(
      cm.get(), GURL("http://d.c.b.a.google.com/aa/bb/x.html"), "b=1"));
  EXPECT_TRUE(SetCookie(
      cm.get(), GURL("http://news.bbc.co.uk/midpath/x.html"), "g=10"));
  {
    unsigned int i = 0;
    CookieList cookies(GetAllCookiesForURL(
        cm.get(), GURL("http://d.c.b.a.google.com/aa/bb/cc/dd")));
    ASSERT_EQ(5u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }

  {
    unsigned int i = 0;
    CookieList cookies(GetAllCookies(cm.get()));
    ASSERT_EQ(6u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("g", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }
}

// This test and CookieMonstertest.TestGCTimes (in cookie_monster_perftest.cc)
// are somewhat complementary twins.  This test is probing for whether
// garbage collection always happens when it should (i.e. that we actually
// get rid of cookies when we should).  The perftest is probing for
// whether garbage collection happens when it shouldn't.  See comments
// before that test for more details.

// Disabled on Windows, see crbug.com/126095
#if defined(OS_WIN)
#define MAYBE_GarbageCollectionTriggers DISABLED_GarbageCollectionTriggers
#else
#define MAYBE_GarbageCollectionTriggers GarbageCollectionTriggers
#endif

TEST_F(CookieMonsterTest, MAYBE_GarbageCollectionTriggers) {
  // First we check to make sure that a whole lot of recent cookies
  // doesn't get rid of anything after garbage collection is checked for.
  {
    scoped_refptr<CookieMonster> cm(
        CreateMonsterForGC(CookieMonster::kMaxCookies * 2));
    EXPECT_EQ(CookieMonster::kMaxCookies * 2, GetAllCookies(cm.get()).size());
    SetCookie(cm.get(), GURL("http://newdomain.com"), "b=2");
    EXPECT_EQ(CookieMonster::kMaxCookies * 2 + 1,
              GetAllCookies(cm.get()).size());
  }

  // Now we explore a series of relationships between cookie last access
  // time and size of store to make sure we only get rid of cookies when
  // we really should.
  const struct TestCase {
    size_t num_cookies;
    size_t num_old_cookies;
    size_t expected_initial_cookies;
    // Indexed by ExpiryAndKeyScheme
    size_t expected_cookies_after_set;
  } test_cases[] = {
    {
      // A whole lot of recent cookies; gc shouldn't happen.
      CookieMonster::kMaxCookies * 2,
      0,
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies * 2 + 1
    }, {
      // Some old cookies, but still overflowing max.
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies / 2,
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies * 2 - CookieMonster::kMaxCookies / 2 + 1
    }, {
      // Old cookies enough to bring us right down to our purge line.
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies + CookieMonster::kPurgeCookies + 1,
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies
    }, {
      // Old cookies enough to bring below our purge line (which we
      // shouldn't do).
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies * 3 / 2,
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies
    }
  };

  for (int ci = 0; ci < static_cast<int>(ARRAYSIZE_UNSAFE(test_cases)); ++ci) {
    const TestCase *test_case = &test_cases[ci];
    scoped_refptr<CookieMonster> cm(
        CreateMonsterFromStoreForGC(
            test_case->num_cookies, test_case->num_old_cookies,
            CookieMonster::kSafeFromGlobalPurgeDays * 2));
    EXPECT_EQ(test_case->expected_initial_cookies,
              GetAllCookies(cm.get()).size()) << "For test case " << ci;
    // Will trigger GC
    SetCookie(cm.get(), GURL("http://newdomain.com"), "b=2");
    EXPECT_EQ(test_case->expected_cookies_after_set,
              GetAllCookies(cm.get()).size()) << "For test case " << ci;
  }
}

// This test checks that keep expired cookies flag is working.
TEST_F(CookieMonsterTest, KeepExpiredCookies) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  cm->SetKeepExpiredCookies();
  CookieOptions options;

  // Set a persistent cookie.
  ASSERT_TRUE(SetCookieWithOptions(
      cm.get(),
      url_google_,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-22 22:50:13 GMT",
      options));

  // Get the canonical cookie.
  CookieList cookie_list = GetAllCookies(cm.get());
  ASSERT_EQ(1U, cookie_list.size());

  // Use a past expiry date to delete the cookie.
  ASSERT_TRUE(SetCookieWithOptions(
      cm.get(),
      url_google_,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-1977 22:50:13 GMT",
      options));

  // Check that the cookie with the past expiry date is still there.
  // GetAllCookies() also triggers garbage collection.
  cookie_list = GetAllCookies(cm.get());
  ASSERT_EQ(1U, cookie_list.size());
  ASSERT_TRUE(cookie_list[0].IsExpired(Time::Now()));
}

namespace {

// Mock PersistentCookieStore that keeps track of the number of Flush() calls.
class FlushablePersistentStore : public CookieMonster::PersistentCookieStore {
 public:
  FlushablePersistentStore() : flush_count_(0) {}

  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE {
    std::vector<CanonicalCookie*> out_cookies;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&net::LoadedCallbackTask::Run,
                   new net::LoadedCallbackTask(loaded_callback, out_cookies)));
  }

  virtual void LoadCookiesForKey(
      const std::string& key,
      const LoadedCallback& loaded_callback) OVERRIDE {
    Load(loaded_callback);
  }

  virtual void AddCookie(const CanonicalCookie&) OVERRIDE {}
  virtual void UpdateCookieAccessTime(const CanonicalCookie&) OVERRIDE {}
  virtual void DeleteCookie(const CanonicalCookie&) OVERRIDE {}
  virtual void SetForceKeepSessionState() OVERRIDE {}

  virtual void Flush(const base::Closure& callback) OVERRIDE {
    ++flush_count_;
    if (!callback.is_null())
      callback.Run();
  }

  int flush_count() {
    return flush_count_;
  }

 private:
  virtual ~FlushablePersistentStore() {}

  volatile int flush_count_;
};

// Counts the number of times Callback() has been run.
class CallbackCounter : public base::RefCountedThreadSafe<CallbackCounter> {
 public:
  CallbackCounter() : callback_count_(0) {}

  void Callback() {
    ++callback_count_;
  }

  int callback_count() {
    return callback_count_;
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackCounter>;
  ~CallbackCounter() {}

  volatile int callback_count_;
};

}  // namespace

// Test that FlushStore() is forwarded to the store and callbacks are posted.
TEST_F(CookieMonsterTest, FlushStore) {
  scoped_refptr<CallbackCounter> counter(new CallbackCounter());
  scoped_refptr<FlushablePersistentStore> store(new FlushablePersistentStore());
  scoped_refptr<CookieMonster> cm(new CookieMonster(store.get(), NULL));

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(0, counter->callback_count());

  // Before initialization, FlushStore() should just run the callback.
  cm->FlushStore(base::Bind(&CallbackCounter::Callback, counter.get()));
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // NULL callback is safe.
  cm->FlushStore(base::Closure());
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // After initialization, FlushStore() should delegate to the store.
  GetAllCookies(cm.get());  // Force init.
  cm->FlushStore(base::Bind(&CallbackCounter::Callback, counter.get()));
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(1, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // NULL callback is still safe.
  cm->FlushStore(base::Closure());
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(2, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // If there's no backing store, FlushStore() is always a safe no-op.
  cm = new CookieMonster(NULL, NULL);
  GetAllCookies(cm.get());  // Force init.
  cm->FlushStore(base::Closure());
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(2, counter->callback_count());

  cm->FlushStore(base::Bind(&CallbackCounter::Callback, counter.get()));
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(3, counter->callback_count());
}

TEST_F(CookieMonsterTest, HistogramCheck) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  // Should match call in InitializeHistograms, but doesn't really matter
  // since the histogram should have been initialized by the CM construction
  // above.
  base::HistogramBase* expired_histogram =
      base::Histogram::FactoryGet(
          "Cookie.ExpirationDurationMinutes", 1, 10 * 365 * 24 * 60, 50,
          base::Histogram::kUmaTargetedHistogramFlag);

  scoped_ptr<base::HistogramSamples> samples1(
      expired_histogram->SnapshotSamples());
  ASSERT_TRUE(
      SetCookieWithDetails(cm.get(),
                           GURL("http://fake.a.url"),
                           "a",
                           "b",
                           "a.url",
                           "/",
                           base::Time::Now() + base::TimeDelta::FromMinutes(59),
                           false,
                           false,
                           COOKIE_PRIORITY_DEFAULT));

  scoped_ptr<base::HistogramSamples> samples2(
      expired_histogram->SnapshotSamples());
  EXPECT_EQ(samples1->TotalCount() + 1, samples2->TotalCount());

  // kValidCookieLine creates a session cookie.
  ASSERT_TRUE(SetCookie(cm.get(), url_google_, kValidCookieLine));

  scoped_ptr<base::HistogramSamples> samples3(
      expired_histogram->SnapshotSamples());
  EXPECT_EQ(samples2->TotalCount(), samples3->TotalCount());
}

namespace {

class MultiThreadedCookieMonsterTest : public CookieMonsterTest {
 public:
  MultiThreadedCookieMonsterTest() : other_thread_("CMTthread") {}

  // Helper methods for calling the asynchronous CookieMonster methods
  // from a different thread.

  void GetAllCookiesTask(CookieMonster* cm,
                         GetCookieListCallback* callback) {
    cm->GetAllCookiesAsync(
        base::Bind(&GetCookieListCallback::Run, base::Unretained(callback)));
  }

  void GetAllCookiesForURLTask(CookieMonster* cm,
                               const GURL& url,
                               GetCookieListCallback* callback) {
    cm->GetAllCookiesForURLAsync(
        url,
        base::Bind(&GetCookieListCallback::Run, base::Unretained(callback)));
  }

  void GetAllCookiesForURLWithOptionsTask(CookieMonster* cm,
                                          const GURL& url,
                                          const CookieOptions& options,
                                          GetCookieListCallback* callback) {
    cm->GetAllCookiesForURLWithOptionsAsync(
        url, options,
        base::Bind(&GetCookieListCallback::Run, base::Unretained(callback)));
  }

  void SetCookieWithDetailsTask(CookieMonster* cm, const GURL& url,
                                ResultSavingCookieCallback<bool>* callback) {
    // Define the parameters here instead of in the calling fucntion.
    // The maximum number of parameters for Bind function is 6.
    std::string name = "A";
    std::string value = "B";
    std::string domain = std::string();
    std::string path = "/foo";
    base::Time expiration_time = base::Time();
    bool secure = false;
    bool http_only = false;
    CookiePriority priority = COOKIE_PRIORITY_DEFAULT;
    cm->SetCookieWithDetailsAsync(
        url, name, value, domain, path, expiration_time, secure, http_only,
        priority,
        base::Bind(
            &ResultSavingCookieCallback<bool>::Run,
            base::Unretained(callback)));
  }

  void DeleteAllCreatedBetweenTask(CookieMonster* cm,
                                   const base::Time& delete_begin,
                                   const base::Time& delete_end,
                                   ResultSavingCookieCallback<int>* callback) {
    cm->DeleteAllCreatedBetweenAsync(
        delete_begin, delete_end,
        base::Bind(
            &ResultSavingCookieCallback<int>::Run, base::Unretained(callback)));
  }

  void DeleteAllForHostTask(CookieMonster* cm,
                            const GURL& url,
                            ResultSavingCookieCallback<int>* callback) {
    cm->DeleteAllForHostAsync(
        url,
        base::Bind(
            &ResultSavingCookieCallback<int>::Run, base::Unretained(callback)));
  }

  void DeleteAllCreatedBetweenForHostTask(
      CookieMonster* cm,
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      ResultSavingCookieCallback<int>* callback) {
    cm->DeleteAllCreatedBetweenForHostAsync(
        delete_begin, delete_end, url,
        base::Bind(
            &ResultSavingCookieCallback<int>::Run,
            base::Unretained(callback)));
  }

  void DeleteCanonicalCookieTask(CookieMonster* cm,
                                 const CanonicalCookie& cookie,
                                 ResultSavingCookieCallback<bool>* callback) {
    cm->DeleteCanonicalCookieAsync(
        cookie,
        base::Bind(
            &ResultSavingCookieCallback<bool>::Run,
            base::Unretained(callback)));
  }

 protected:
  void RunOnOtherThread(const base::Closure& task) {
    other_thread_.Start();
    other_thread_.message_loop()->PostTask(FROM_HERE, task);
    RunFor(kTimeout);
    other_thread_.Stop();
  }

  Thread other_thread_;
};

}  // namespace

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetAllCookies) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B"));
  CookieList cookies = GetAllCookies(cm.get());
  CookieList::const_iterator it = cookies.begin();
  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == cookies.end());
  GetCookieListCallback callback(&other_thread_);
  base::Closure task =
      base::Bind(&net::MultiThreadedCookieMonsterTest::GetAllCookiesTask,
                 base::Unretained(this),
                 cm, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  it = callback.cookies().begin();
  ASSERT_TRUE(it != callback.cookies().end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == callback.cookies().end());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetAllCookiesForURL) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B"));
  CookieList cookies = GetAllCookiesForURL(cm.get(), url_google_);
  CookieList::const_iterator it = cookies.begin();
  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == cookies.end());
  GetCookieListCallback callback(&other_thread_);
  base::Closure task =
      base::Bind(&net::MultiThreadedCookieMonsterTest::GetAllCookiesForURLTask,
                 base::Unretained(this),
                 cm, url_google_, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  it = callback.cookies().begin();
  ASSERT_TRUE(it != callback.cookies().end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == callback.cookies().end());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetAllCookiesForURLWithOpt) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B"));
  CookieOptions options;
  CookieList cookies =
      GetAllCookiesForURLWithOptions(cm.get(), url_google_, options);
  CookieList::const_iterator it = cookies.begin();
  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == cookies.end());
  GetCookieListCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::GetAllCookiesForURLWithOptionsTask,
      base::Unretained(this),
      cm, url_google_, options, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  it = callback.cookies().begin();
  ASSERT_TRUE(it != callback.cookies().end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == callback.cookies().end());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckSetCookieWithDetails) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookieWithDetails(cm.get(),
                                   url_google_foo_,
                                   "A",
                                   "B",
                                   std::string(),
                                   "/foo",
                                   base::Time(),
                                   false,
                                   false,
                                   COOKIE_PRIORITY_DEFAULT));
  ResultSavingCookieCallback<bool> callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::SetCookieWithDetailsTask,
      base::Unretained(this),
      cm, url_google_foo_, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_TRUE(callback.result());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckDeleteAllCreatedBetween) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  Time now = Time::Now();
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  EXPECT_EQ(
      1,
      DeleteAllCreatedBetween(cm.get(), now - TimeDelta::FromDays(99), Time()));
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  ResultSavingCookieCallback<int> callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteAllCreatedBetweenTask,
      base::Unretained(this),
      cm, now - TimeDelta::FromDays(99),
      Time(), &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1, callback.result());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckDeleteAllForHost) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  EXPECT_EQ(1, DeleteAllForHost(cm.get(), url_google_));
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  ResultSavingCookieCallback<int> callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteAllForHostTask,
      base::Unretained(this),
      cm, url_google_, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1, callback.result());
}

TEST_F(MultiThreadedCookieMonsterTest,
       ThreadCheckDeleteAllCreatedBetweenForHost) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  GURL url_not_google("http://www.notgoogle.com");

  CookieOptions options;
  Time now = Time::Now();
  // ago1 < ago2 < ago3 < now.
  Time ago1 = now - TimeDelta::FromDays(101);
  Time ago2 = now - TimeDelta::FromDays(100);
  Time ago3 = now - TimeDelta::FromDays(99);

  // These 3 cookies match the first deletion.
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "C=D", options));
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "Y=Z", options));

  // This cookie does not match host.
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_not_google, "E=F", options));

  // This cookie does not match time range: [ago3, inf], for first deletion, but
  // matches for the second deletion.
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "G=H", ago2));

  // 1. First set of deletions.
  EXPECT_EQ(
      3,  // Deletes A=B, C=D, Y=Z
      DeleteAllCreatedBetweenForHost(
          cm.get(), ago3, Time::Max(), url_google_));

  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  ResultSavingCookieCallback<int> callback(&other_thread_);

  // 2. Second set of deletions.
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteAllCreatedBetweenForHostTask,
      base::Unretained(this),
      cm, ago1, Time(), url_google_,
      &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(2, callback.result());  // Deletes A=B, G=H.
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckDeleteCanonicalCookie) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  CookieList cookies = GetAllCookies(cm.get());
  CookieList::iterator it = cookies.begin();
  EXPECT_TRUE(DeleteCanonicalCookie(cm.get(), *it));

  EXPECT_TRUE(SetCookieWithOptions(cm.get(), url_google_, "A=B", options));
  ResultSavingCookieCallback<bool> callback(&other_thread_);
  cookies = GetAllCookies(cm.get());
  it = cookies.begin();
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteCanonicalCookieTask,
      base::Unretained(this),
      cm, *it, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_TRUE(callback.result());
}

TEST_F(CookieMonsterTest, InvalidExpiryTime) {
  std::string cookie_line =
      std::string(kValidCookieLine) + "; expires=Blarg arg arg";
  scoped_ptr<CanonicalCookie> cookie(
      CanonicalCookie::Create(url_google_, cookie_line, Time::Now(),
                              CookieOptions()));
  ASSERT_FALSE(cookie->IsPersistent());
}

// Test that CookieMonster writes session cookies into the underlying
// CookieStore if the "persist session cookies" option is on.
TEST_F(CookieMonsterTest, PersistSessionCookies) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store.get(), NULL));
  cm->SetPersistSessionCookies(true);

  // All cookies set with SetCookie are session cookies.
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B"));
  EXPECT_EQ("A=B", GetCookies(cm.get(), url_google_));

  // The cookie was written to the backing store.
  EXPECT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  EXPECT_EQ("A", store->commands()[0].cookie.Name());
  EXPECT_EQ("B", store->commands()[0].cookie.Value());

  // Modify the cookie.
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=C"));
  EXPECT_EQ("A=C", GetCookies(cm.get(), url_google_));
  EXPECT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ("A", store->commands()[1].cookie.Name());
  EXPECT_EQ("B", store->commands()[1].cookie.Value());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  EXPECT_EQ("A", store->commands()[2].cookie.Name());
  EXPECT_EQ("C", store->commands()[2].cookie.Value());

  // Delete the cookie.
  DeleteCookie(cm.get(), url_google_, "A");
  EXPECT_EQ("", GetCookies(cm.get(), url_google_));
  EXPECT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
  EXPECT_EQ("A", store->commands()[3].cookie.Name());
  EXPECT_EQ("C", store->commands()[3].cookie.Value());
}

// Test the commands sent to the persistent cookie store.
TEST_F(CookieMonsterTest, PersisentCookieStorageTest) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store.get(), NULL));

  // Add a cookie.
  EXPECT_TRUE(SetCookie(
      cm.get(), url_google_, "A=B; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  this->MatchCookieLines("A=B", GetCookies(cm.get(), url_google_));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  // Remove it.
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "A=B; max-age=0"));
  this->MatchCookieLines(std::string(), GetCookies(cm.get(), url_google_));
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);

  // Add a cookie.
  EXPECT_TRUE(SetCookie(
      cm.get(), url_google_, "A=B; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  this->MatchCookieLines("A=B", GetCookies(cm.get(), url_google_));
  ASSERT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  // Overwrite it.
  EXPECT_TRUE(SetCookie(
      cm.get(), url_google_, "A=Foo; expires=Mon, 18-Apr-22 22:50:14 GMT"));
  this->MatchCookieLines("A=Foo", GetCookies(cm.get(), url_google_));
  ASSERT_EQ(5u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[4].type);

  // Create some non-persistent cookies and check that they don't go to the
  // persistent storage.
  EXPECT_TRUE(SetCookie(cm.get(), url_google_, "B=Bar"));
  this->MatchCookieLines("A=Foo; B=Bar", GetCookies(cm.get(), url_google_));
  EXPECT_EQ(5u, store->commands().size());
}

// Test to assure that cookies with control characters are purged appropriately.
// See http://crbug.com/238041 for background.
TEST_F(CookieMonsterTest, ControlCharacterPurge) {
  const Time now1(Time::Now());
  const Time now2(Time::Now() + TimeDelta::FromSeconds(1));
  const Time now3(Time::Now() + TimeDelta::FromSeconds(2));
  const Time later(now1 + TimeDelta::FromDays(1));
  const GURL url("http://host/path");
  const std::string domain("host");
  const std::string path("/path");

  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);

  std::vector<CanonicalCookie*> initial_cookies;

  AddCookieToList(domain,
                  "foo=bar; path=" + path,
                  now1,
                  &initial_cookies);

  // We have to manually build this cookie because it contains a control
  // character, and our cookie line parser rejects control characters.
  CanonicalCookie *cc = new CanonicalCookie(url, "baz", "\x05" "boo", domain,
                                            path, now2, later, now2, false,
                                            false, COOKIE_PRIORITY_DEFAULT);
  initial_cookies.push_back(cc);

  AddCookieToList(domain,
                  "hello=world; path=" + path,
                  now3,
                  &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<CookieMonster> cm(new CookieMonster(store.get(), NULL));

  EXPECT_EQ("foo=bar; hello=world", GetCookies(cm.get(), url));
}

}  // namespace net
