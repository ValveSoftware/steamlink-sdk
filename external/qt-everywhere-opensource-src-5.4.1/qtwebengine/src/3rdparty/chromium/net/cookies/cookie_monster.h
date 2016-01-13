// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by the letter D and the number 2.

#ifndef NET_COOKIES_COOKIE_MONSTER_H_
#define NET_COOKIES_COOKIE_MONSTER_H_

#include <deque>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_store.h"

class GURL;

namespace base {
class Histogram;
class HistogramBase;
class TimeTicks;
}  // namespace base

namespace net {

class CookieMonsterDelegate;
class ParsedCookie;

// The cookie monster is the system for storing and retrieving cookies. It has
// an in-memory list of all cookies, and synchronizes non-session cookies to an
// optional permanent storage that implements the PersistentCookieStore
// interface.
//
// This class IS thread-safe. Normally, it is only used on the I/O thread, but
// is also accessed directly through Automation for UI testing.
//
// All cookie tasks are handled asynchronously. Tasks may be deferred if
// all affected cookies are not yet loaded from the backing store. Otherwise,
// the callback may be invoked immediately (prior to return of the asynchronous
// function).
//
// A cookie task is either pending loading of the entire cookie store, or
// loading of cookies for a specfic domain key(eTLD+1). In the former case, the
// cookie task will be queued in tasks_pending_ while PersistentCookieStore
// chain loads the cookie store on DB thread. In the latter case, the cookie
// task will be queued in tasks_pending_for_key_ while PermanentCookieStore
// loads cookies for the specified domain key(eTLD+1) on DB thread.
//
// Callbacks are guaranteed to be invoked on the calling thread.
//
// TODO(deanm) Implement CookieMonster, the cookie database.
//  - Verify that our domain enforcement and non-dotted handling is correct
class NET_EXPORT CookieMonster : public CookieStore {
 public:
  class PersistentCookieStore;
  typedef CookieMonsterDelegate Delegate;

  // Terminology:
  //    * The 'top level domain' (TLD) of an internet domain name is
  //      the terminal "." free substring (e.g. "com" for google.com
  //      or world.std.com).
  //    * The 'effective top level domain' (eTLD) is the longest
  //      "." initiated terminal substring of an internet domain name
  //      that is controlled by a general domain registrar.
  //      (e.g. "co.uk" for news.bbc.co.uk).
  //    * The 'effective top level domain plus one' (eTLD+1) is the
  //      shortest "." delimited terminal substring of an internet
  //      domain name that is not controlled by a general domain
  //      registrar (e.g. "bbc.co.uk" for news.bbc.co.uk, or
  //      "google.com" for news.google.com).  The general assumption
  //      is that all hosts and domains under an eTLD+1 share some
  //      administrative control.

  // CookieMap is the central data structure of the CookieMonster.  It
  // is a map whose values are pointers to CanonicalCookie data
  // structures (the data structures are owned by the CookieMonster
  // and must be destroyed when removed from the map).  The key is based on the
  // effective domain of the cookies.  If the domain of the cookie has an
  // eTLD+1, that is the key for the map.  If the domain of the cookie does not
  // have an eTLD+1, the key of the map is the host the cookie applies to (it is
  // not legal to have domain cookies without an eTLD+1).  This rule
  // excludes cookies for, e.g, ".com", ".co.uk", or ".internalnetwork".
  // This behavior is the same as the behavior in Firefox v 3.6.10.

  // NOTE(deanm):
  // I benchmarked hash_multimap vs multimap.  We're going to be query-heavy
  // so it would seem like hashing would help.  However they were very
  // close, with multimap being a tiny bit faster.  I think this is because
  // our map is at max around 1000 entries, and the additional complexity
  // for the hashing might not overcome the O(log(1000)) for querying
  // a multimap.  Also, multimap is standard, another reason to use it.
  // TODO(rdsmith): This benchmark should be re-done now that we're allowing
  // subtantially more entries in the map.
  typedef std::multimap<std::string, CanonicalCookie*> CookieMap;
  typedef std::pair<CookieMap::iterator, CookieMap::iterator> CookieMapItPair;
  typedef std::vector<CookieMap::iterator> CookieItVector;

  // Cookie garbage collection thresholds.  Based off of the Mozilla defaults.
  // When the number of cookies gets to k{Domain,}MaxCookies
  // purge down to k{Domain,}MaxCookies - k{Domain,}PurgeCookies.
  // It might seem scary to have a high purge value, but really it's not.
  // You just make sure that you increase the max to cover the increase
  // in purge, and we would have been purging the same number of cookies.
  // We're just going through the garbage collection process less often.
  // Note that the DOMAIN values are per eTLD+1; see comment for the
  // CookieMap typedef.  So, e.g., the maximum number of cookies allowed for
  // google.com and all of its subdomains will be 150-180.
  //
  // Any cookies accessed more recently than kSafeFromGlobalPurgeDays will not
  // be evicted by global garbage collection, even if we have more than
  // kMaxCookies.  This does not affect domain garbage collection.
  static const size_t kDomainMaxCookies;
  static const size_t kDomainPurgeCookies;
  static const size_t kMaxCookies;
  static const size_t kPurgeCookies;

  // Quota for cookies with {low, medium, high} priorities within a domain.
  static const size_t kDomainCookiesQuotaLow;
  static const size_t kDomainCookiesQuotaMedium;
  static const size_t kDomainCookiesQuotaHigh;

  // The store passed in should not have had Init() called on it yet. This
  // class will take care of initializing it. The backing store is NOT owned by
  // this class, but it must remain valid for the duration of the cookie
  // monster's existence. If |store| is NULL, then no backing store will be
  // updated. If |delegate| is non-NULL, it will be notified on
  // creation/deletion of cookies.
  CookieMonster(PersistentCookieStore* store, CookieMonsterDelegate* delegate);

  // Only used during unit testing.
  CookieMonster(PersistentCookieStore* store,
                CookieMonsterDelegate* delegate,
                int last_access_threshold_milliseconds);

  // Helper function that adds all cookies from |list| into this instance.
  bool InitializeFrom(const CookieList& list);

  typedef base::Callback<void(const CookieList& cookies)> GetCookieListCallback;
  typedef base::Callback<void(bool success)> DeleteCookieCallback;
  typedef base::Callback<void(bool cookies_exist)> HasCookiesForETLDP1Callback;

  // Sets a cookie given explicit user-provided cookie attributes. The cookie
  // name, value, domain, etc. are each provided as separate strings. This
  // function expects each attribute to be well-formed. It will check for
  // disallowed characters (e.g. the ';' character is disallowed within the
  // cookie value attribute) and will return false without setting the cookie
  // if such characters are found.
  void SetCookieWithDetailsAsync(const GURL& url,
                                 const std::string& name,
                                 const std::string& value,
                                 const std::string& domain,
                                 const std::string& path,
                                 const base::Time& expiration_time,
                                 bool secure,
                                 bool http_only,
                                 CookiePriority priority,
                                 const SetCookiesCallback& callback);


  // Returns all the cookies, for use in management UI, etc. This does not mark
  // the cookies as having been accessed.
  // The returned cookies are ordered by longest path, then by earliest
  // creation date.
  void GetAllCookiesAsync(const GetCookieListCallback& callback);

  // Returns all the cookies, for use in management UI, etc. Filters results
  // using given url scheme, host / domain and path and options. This does not
  // mark the cookies as having been accessed.
  // The returned cookies are ordered by longest path, then earliest
  // creation date.
  void GetAllCookiesForURLWithOptionsAsync(
      const GURL& url,
      const CookieOptions& options,
      const GetCookieListCallback& callback);

  // Deletes all of the cookies.
  void DeleteAllAsync(const DeleteCallback& callback);

  // Deletes all cookies that match the host of the given URL
  // regardless of path.  This includes all http_only and secure cookies,
  // but does not include any domain cookies that may apply to this host.
  // Returns the number of cookies deleted.
  void DeleteAllForHostAsync(const GURL& url,
                             const DeleteCallback& callback);

  // Deletes one specific cookie.
  void DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                  const DeleteCookieCallback& callback);

  // Checks whether for a given ETLD+1, there currently exist any cookies.
  void HasCookiesForETLDP1Async(const std::string& etldp1,
                                const HasCookiesForETLDP1Callback& callback);

  // Resets the list of cookieable schemes to the supplied schemes.
  // If this this method is called, it must be called before first use of
  // the instance (i.e. as part of the instance initialization process).
  void SetCookieableSchemes(const char* schemes[], size_t num_schemes);

  // Resets the list of cookieable schemes to kDefaultCookieableSchemes with or
  // without 'file' being included.
  //
  // There are some unknowns about how to correctly handle file:// cookies,
  // and our implementation for this is not robust enough. This allows you
  // to enable support, but it should only be used for testing. Bug 1157243.
  void SetEnableFileScheme(bool accept);

  // Instructs the cookie monster to not delete expired cookies. This is used
  // in cases where the cookie monster is used as a data structure to keep
  // arbitrary cookies.
  void SetKeepExpiredCookies();

  // Protects session cookies from deletion on shutdown.
  void SetForceKeepSessionState();

  // Flush the backing store (if any) to disk and post the given callback when
  // done.
  // WARNING: THE CALLBACK WILL RUN ON A RANDOM THREAD. IT MUST BE THREAD SAFE.
  // It may be posted to the current thread, or it may run on the thread that
  // actually does the flushing. Your Task should generally post a notification
  // to the thread you actually want to be notified on.
  void FlushStore(const base::Closure& callback);

  // CookieStore implementation.

  // Sets the cookies specified by |cookie_list| returned from |url|
  // with options |options| in effect.
  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const CookieOptions& options,
      const SetCookiesCallback& callback) OVERRIDE;

  // Gets all cookies that apply to |url| given |options|.
  // The returned cookies are ordered by longest path, then earliest
  // creation date.
  virtual void GetCookiesWithOptionsAsync(
      const GURL& url,
      const CookieOptions& options,
      const GetCookiesCallback& callback) OVERRIDE;

  // Invokes GetAllCookiesForURLWithOptions with options set to include HTTP
  // only cookies.
  virtual void GetAllCookiesForURLAsync(
      const GURL& url,
      const GetCookieListCallback& callback) OVERRIDE;

  // Deletes all cookies with that might apply to |url| that has |cookie_name|.
  virtual void DeleteCookieAsync(
      const GURL& url, const std::string& cookie_name,
      const base::Closure& callback) OVERRIDE;

  // Deletes all of the cookies that have a creation_date greater than or equal
  // to |delete_begin| and less than |delete_end|.
  // Returns the number of cookies that have been deleted.
  virtual void DeleteAllCreatedBetweenAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const DeleteCallback& callback) OVERRIDE;

  // Deletes all of the cookies that match the host of the given URL
  // regardless of path and that have a creation_date greater than or
  // equal to |delete_begin| and less then |delete_end|. This includes
  // all http_only and secure cookies, but does not include any domain
  // cookies that may apply to this host.
  // Returns the number of cookies deleted.
  virtual void DeleteAllCreatedBetweenForHostAsync(
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      const DeleteCallback& callback) OVERRIDE;

  virtual void DeleteSessionCookiesAsync(const DeleteCallback&) OVERRIDE;

  virtual CookieMonster* GetCookieMonster() OVERRIDE;

  // Enables writing session cookies into the cookie database. If this this
  // method is called, it must be called before first use of the instance
  // (i.e. as part of the instance initialization process).
  void SetPersistSessionCookies(bool persist_session_cookies);

  // Debugging method to perform various validation checks on the map.
  // Currently just checking that there are no null CanonicalCookie pointers
  // in the map.
  // Argument |arg| is to allow retaining of arbitrary data if the CHECKs
  // in the function trip.  TODO(rdsmith):Remove hack.
  void ValidateMap(int arg);

  // Determines if the scheme of the URL is a scheme that cookies will be
  // stored for.
  bool IsCookieableScheme(const std::string& scheme);

  // The default list of schemes the cookie monster can handle.
  static const char* kDefaultCookieableSchemes[];
  static const int kDefaultCookieableSchemesCount;

  // Copies all keys for the given |key| to another cookie monster |other|.
  // Both |other| and |this| must be loaded for this operation to succeed.
  // Furthermore, there may not be any cookies stored in |other| for |key|.
  // Returns false if any of these conditions is not met.
  bool CopyCookiesForKeyToOtherCookieMonster(std::string key,
                                             CookieMonster* other);

  // Find the key (for lookup in cookies_) based on the given domain.
  // See comment on keys before the CookieMap typedef.
  std::string GetKey(const std::string& domain) const;

  bool loaded();

 private:
  // For queueing the cookie monster calls.
  class CookieMonsterTask;
  template <typename Result> class DeleteTask;
  class DeleteAllCreatedBetweenTask;
  class DeleteAllCreatedBetweenForHostTask;
  class DeleteAllForHostTask;
  class DeleteAllTask;
  class DeleteCookieTask;
  class DeleteCanonicalCookieTask;
  class GetAllCookiesForURLWithOptionsTask;
  class GetAllCookiesTask;
  class GetCookiesWithOptionsTask;
  class SetCookieWithDetailsTask;
  class SetCookieWithOptionsTask;
  class DeleteSessionCookiesTask;
  class HasCookiesForETLDP1Task;

  // Testing support.
  // For SetCookieWithCreationTime.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest,
                           TestCookieDeleteAllCreatedBetweenTimestamps);
  // For SetCookieWithCreationTime.
  FRIEND_TEST_ALL_PREFIXES(MultiThreadedCookieMonsterTest,
                           ThreadCheckDeleteAllCreatedBetweenForHost);

  // For gargage collection constants.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestHostGarbageCollection);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestTotalGarbageCollection);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, GarbageCollectionTriggers);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestGCTimes);

  // For validation of key values.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestDomainTree);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestImport);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, GetKey);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestGetKey);

  // For FindCookiesForKey.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, ShortLivedSessionCookies);

  // Internal reasons for deletion, used to populate informative histograms
  // and to provide a public cause for onCookieChange notifications.
  //
  // If you add or remove causes from this list, please be sure to also update
  // the CookieMonsterDelegate::ChangeCause mapping inside ChangeCauseMapping.
  // Moreover, these are used as array indexes, so avoid reordering to keep the
  // histogram buckets consistent. New items (if necessary) should be added
  // at the end of the list, just before DELETE_COOKIE_LAST_ENTRY.
  enum DeletionCause {
    DELETE_COOKIE_EXPLICIT = 0,
    DELETE_COOKIE_OVERWRITE,
    DELETE_COOKIE_EXPIRED,
    DELETE_COOKIE_EVICTED,
    DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE,
    DELETE_COOKIE_DONT_RECORD,  // e.g. For final cleanup after flush to store.
    DELETE_COOKIE_EVICTED_DOMAIN,
    DELETE_COOKIE_EVICTED_GLOBAL,

    // Cookies evicted during domain level garbage collection that
    // were accessed longer ago than kSafeFromGlobalPurgeDays
    DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE,

    // Cookies evicted during domain level garbage collection that
    // were accessed more recently than kSafeFromGlobalPurgeDays
    // (and thus would have been preserved by global garbage collection).
    DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE,

    // A common idiom is to remove a cookie by overwriting it with an
    // already-expired expiration date. This captures that case.
    DELETE_COOKIE_EXPIRED_OVERWRITE,

    // Cookies are not allowed to contain control characters in the name or
    // value. However, we used to allow them, so we are now evicting any such
    // cookies as we load them. See http://crbug.com/238041.
    DELETE_COOKIE_CONTROL_CHAR,

    DELETE_COOKIE_LAST_ENTRY
  };

  // The number of days since last access that cookies will not be subject
  // to global garbage collection.
  static const int kSafeFromGlobalPurgeDays;

  // Record statistics every kRecordStatisticsIntervalSeconds of uptime.
  static const int kRecordStatisticsIntervalSeconds = 10 * 60;

  virtual ~CookieMonster();

  // The following are synchronous calls to which the asynchronous methods
  // delegate either immediately (if the store is loaded) or through a deferred
  // task (if the store is not yet loaded).
  bool SetCookieWithDetails(const GURL& url,
                            const std::string& name,
                            const std::string& value,
                            const std::string& domain,
                            const std::string& path,
                            const base::Time& expiration_time,
                            bool secure,
                            bool http_only,
                            CookiePriority priority);

  CookieList GetAllCookies();

  CookieList GetAllCookiesForURLWithOptions(const GURL& url,
                                            const CookieOptions& options);

  CookieList GetAllCookiesForURL(const GURL& url);

  int DeleteAll(bool sync_to_store);

  int DeleteAllCreatedBetween(const base::Time& delete_begin,
                              const base::Time& delete_end);

  int DeleteAllForHost(const GURL& url);
  int DeleteAllCreatedBetweenForHost(const base::Time delete_begin,
                                     const base::Time delete_end,
                                     const GURL& url);

  bool DeleteCanonicalCookie(const CanonicalCookie& cookie);

  bool SetCookieWithOptions(const GURL& url,
                            const std::string& cookie_line,
                            const CookieOptions& options);

  std::string GetCookiesWithOptions(const GURL& url,
                                    const CookieOptions& options);

  void DeleteCookie(const GURL& url, const std::string& cookie_name);

  bool SetCookieWithCreationTime(const GURL& url,
                                 const std::string& cookie_line,
                                 const base::Time& creation_time);

  int DeleteSessionCookies();

  bool HasCookiesForETLDP1(const std::string& etldp1);

  // Called by all non-static functions to ensure that the cookies store has
  // been initialized. This is not done during creating so it doesn't block
  // the window showing.
  // Note: this method should always be called with lock_ held.
  void InitIfNecessary() {
    if (!initialized_) {
      if (store_.get()) {
        InitStore();
      } else {
        loaded_ = true;
        ReportLoaded();
      }
      initialized_ = true;
    }
  }

  // Initializes the backing store and reads existing cookies from it.
  // Should only be called by InitIfNecessary().
  void InitStore();

  // Reports to the delegate that the cookie monster was loaded.
  void ReportLoaded();

  // Stores cookies loaded from the backing store and invokes any deferred
  // calls. |beginning_time| should be the moment PersistentCookieStore::Load
  // was invoked and is used for reporting histogram_time_blocked_on_load_.
  // See PersistentCookieStore::Load for details on the contents of cookies.
  void OnLoaded(base::TimeTicks beginning_time,
                const std::vector<CanonicalCookie*>& cookies);

  // Stores cookies loaded from the backing store and invokes the deferred
  // task(s) pending loading of cookies associated with the domain key
  // (eTLD+1). Called when all cookies for the domain key(eTLD+1) have been
  // loaded from DB. See PersistentCookieStore::Load for details on the contents
  // of cookies.
  void OnKeyLoaded(
    const std::string& key,
    const std::vector<CanonicalCookie*>& cookies);

  // Stores the loaded cookies.
  void StoreLoadedCookies(const std::vector<CanonicalCookie*>& cookies);

  // Invokes deferred calls.
  void InvokeQueue();

  // Checks that |cookies_| matches our invariants, and tries to repair any
  // inconsistencies. (In other words, it does not have duplicate cookies).
  void EnsureCookiesMapIsValid();

  // Checks for any duplicate cookies for CookieMap key |key| which lie between
  // |begin| and |end|. If any are found, all but the most recent are deleted.
  // Returns the number of duplicate cookies that were deleted.
  int TrimDuplicateCookiesForKey(const std::string& key,
                                 CookieMap::iterator begin,
                                 CookieMap::iterator end);

  void SetDefaultCookieableSchemes();

  void FindCookiesForHostAndDomain(const GURL& url,
                                   const CookieOptions& options,
                                   bool update_access_time,
                                   std::vector<CanonicalCookie*>* cookies);

  void FindCookiesForKey(const std::string& key,
                         const GURL& url,
                         const CookieOptions& options,
                         const base::Time& current,
                         bool update_access_time,
                         std::vector<CanonicalCookie*>* cookies);

  // Delete any cookies that are equivalent to |ecc| (same path, domain, etc).
  // If |skip_httponly| is true, httponly cookies will not be deleted.  The
  // return value with be true if |skip_httponly| skipped an httponly cookie.
  // |key| is the key to find the cookie in cookies_; see the comment before
  // the CookieMap typedef for details.
  // NOTE: There should never be more than a single matching equivalent cookie.
  bool DeleteAnyEquivalentCookie(const std::string& key,
                                 const CanonicalCookie& ecc,
                                 bool skip_httponly,
                                 bool already_expired);

  // Takes ownership of *cc. Returns an iterator that points to the inserted
  // cookie in cookies_. Guarantee: all iterators to cookies_ remain valid.
  CookieMap::iterator InternalInsertCookie(const std::string& key,
                                           CanonicalCookie* cc,
                                           bool sync_to_store);

  // Helper function that sets cookies with more control.
  // Not exposed as we don't want callers to have the ability
  // to specify (potentially duplicate) creation times.
  bool SetCookieWithCreationTimeAndOptions(const GURL& url,
                                           const std::string& cookie_line,
                                           const base::Time& creation_time,
                                           const CookieOptions& options);

  // Helper function that sets a canonical cookie, deleting equivalents and
  // performing garbage collection.
  bool SetCanonicalCookie(scoped_ptr<CanonicalCookie>* cc,
                          const base::Time& creation_time,
                          const CookieOptions& options);

  void InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                      const base::Time& current_time);

  // |deletion_cause| argument is used for collecting statistics and choosing
  // the correct CookieMonsterDelegate::ChangeCause for OnCookieChanged
  // notifications.  Guarantee: All iterators to cookies_ except to the
  // deleted entry remain vaild.
  void InternalDeleteCookie(CookieMap::iterator it, bool sync_to_store,
                            DeletionCause deletion_cause);

  // If the number of cookies for CookieMap key |key|, or globally, are
  // over the preset maximums above, garbage collect, first for the host and
  // then globally.  See comments above garbage collection threshold
  // constants for details.
  //
  // Returns the number of cookies deleted (useful for debugging).
  int GarbageCollect(const base::Time& current, const std::string& key);

  // Helper for GarbageCollect(); can be called directly as well.  Deletes
  // all expired cookies in |itpair|.  If |cookie_its| is non-NULL, it is
  // populated with all the non-expired cookies from |itpair|.
  //
  // Returns the number of cookies deleted.
  int GarbageCollectExpired(const base::Time& current,
                            const CookieMapItPair& itpair,
                            std::vector<CookieMap::iterator>* cookie_its);

  // Helper for GarbageCollect(). Deletes all cookies in the range specified by
  // [|it_begin|, |it_end|). Returns the number of cookies deleted.
  int GarbageCollectDeleteRange(const base::Time& current,
                                DeletionCause cause,
                                CookieItVector::iterator cookie_its_begin,
                                CookieItVector::iterator cookie_its_end);

  bool HasCookieableScheme(const GURL& url);

  // Statistics support

  // This function should be called repeatedly, and will record
  // statistics if a sufficient time period has passed.
  void RecordPeriodicStats(const base::Time& current_time);

  // Initialize the above variables; should only be called from
  // the constructor.
  void InitializeHistograms();

  // The resolution of our time isn't enough, so we do something
  // ugly and increment when we've seen the same time twice.
  base::Time CurrentTime();

  // Runs the task if, or defers the task until, the full cookie database is
  // loaded.
  void DoCookieTask(const scoped_refptr<CookieMonsterTask>& task_item);

  // Runs the task if, or defers the task until, the cookies for the given URL
  // are loaded.
  void DoCookieTaskForURL(const scoped_refptr<CookieMonsterTask>& task_item,
    const GURL& url);

  // Histogram variables; see CookieMonster::InitializeHistograms() in
  // cookie_monster.cc for details.
  base::HistogramBase* histogram_expiration_duration_minutes_;
  base::HistogramBase* histogram_between_access_interval_minutes_;
  base::HistogramBase* histogram_evicted_last_access_minutes_;
  base::HistogramBase* histogram_count_;
  base::HistogramBase* histogram_domain_count_;
  base::HistogramBase* histogram_etldp1_count_;
  base::HistogramBase* histogram_domain_per_etldp1_count_;
  base::HistogramBase* histogram_number_duplicate_db_cookies_;
  base::HistogramBase* histogram_cookie_deletion_cause_;
  base::HistogramBase* histogram_time_get_;
  base::HistogramBase* histogram_time_mac_;
  base::HistogramBase* histogram_time_blocked_on_load_;

  CookieMap cookies_;

  // Indicates whether the cookie store has been initialized. This happens
  // lazily in InitStoreIfNecessary().
  bool initialized_;

  // Indicates whether loading from the backend store is completed and
  // calls may be immediately processed.
  bool loaded_;

  // List of domain keys that have been loaded from the DB.
  std::set<std::string> keys_loaded_;

  // Map of domain keys to their associated task queues. These tasks are blocked
  // until all cookies for the associated domain key eTLD+1 are loaded from the
  // backend store.
  std::map<std::string, std::deque<scoped_refptr<CookieMonsterTask> > >
      tasks_pending_for_key_;

  // Queues tasks that are blocked until all cookies are loaded from the backend
  // store.
  std::queue<scoped_refptr<CookieMonsterTask> > tasks_pending_;

  scoped_refptr<PersistentCookieStore> store_;

  base::Time last_time_seen_;

  // Minimum delay after updating a cookie's LastAccessDate before we will
  // update it again.
  const base::TimeDelta last_access_threshold_;

  // Approximate date of access time of least recently accessed cookie
  // in |cookies_|.  Note that this is not guaranteed to be accurate, only a)
  // to be before or equal to the actual time, and b) to be accurate
  // immediately after a garbage collection that scans through all the cookies.
  // This value is used to determine whether global garbage collection might
  // find cookies to purge.
  // Note: The default Time() constructor will create a value that compares
  // earlier than any other time value, which is wanted.  Thus this
  // value is not initialized.
  base::Time earliest_access_time_;

  // During loading, holds the set of all loaded cookie creation times. Used to
  // avoid ever letting cookies with duplicate creation times into the store;
  // that way we don't have to worry about what sections of code are safe
  // to call while it's in that state.
  std::set<int64> creation_times_;

  std::vector<std::string> cookieable_schemes_;

  scoped_refptr<CookieMonsterDelegate> delegate_;

  // Lock for thread-safety
  base::Lock lock_;

  base::Time last_statistic_record_time_;

  bool keep_expired_cookies_;
  bool persist_session_cookies_;

  // Static setting for whether or not file scheme cookies are allows when
  // a new CookieMonster is created, or the accepted schemes on a CookieMonster
  // instance are reset back to defaults.
  static bool default_enable_file_scheme_;

  DISALLOW_COPY_AND_ASSIGN(CookieMonster);
};

class NET_EXPORT CookieMonsterDelegate
    : public base::RefCountedThreadSafe<CookieMonsterDelegate> {
 public:
  // The publicly relevant reasons a cookie might be changed.
  enum ChangeCause {
    // The cookie was changed directly by a consumer's action.
    CHANGE_COOKIE_EXPLICIT,
    // The cookie was automatically removed due to an insert operation that
    // overwrote it.
    CHANGE_COOKIE_OVERWRITE,
    // The cookie was automatically removed as it expired.
    CHANGE_COOKIE_EXPIRED,
    // The cookie was automatically evicted during garbage collection.
    CHANGE_COOKIE_EVICTED,
    // The cookie was overwritten with an already-expired expiration date.
    CHANGE_COOKIE_EXPIRED_OVERWRITE
  };

  // Will be called when a cookie is added or removed. The function is passed
  // the respective |cookie| which was added to or removed from the cookies.
  // If |removed| is true, the cookie was deleted, and |cause| will be set
  // to the reason for its removal. If |removed| is false, the cookie was
  // added, and |cause| will be set to CHANGE_COOKIE_EXPLICIT.
  //
  // As a special case, note that updating a cookie's properties is implemented
  // as a two step process: the cookie to be updated is first removed entirely,
  // generating a notification with cause CHANGE_COOKIE_OVERWRITE.  Afterwards,
  // a new cookie is written with the updated values, generating a notification
  // with cause CHANGE_COOKIE_EXPLICIT.
  virtual void OnCookieChanged(const CanonicalCookie& cookie,
                               bool removed,
                               ChangeCause cause) = 0;
  // Indicates that the cookie store has fully loaded.
  virtual void OnLoaded() = 0;

 protected:
  friend class base::RefCountedThreadSafe<CookieMonsterDelegate>;
  virtual ~CookieMonsterDelegate() {}
};

typedef base::RefCountedThreadSafe<CookieMonster::PersistentCookieStore>
    RefcountedPersistentCookieStore;

class NET_EXPORT CookieMonster::PersistentCookieStore
    : public RefcountedPersistentCookieStore {
 public:
  typedef base::Callback<void(const std::vector<CanonicalCookie*>&)>
      LoadedCallback;

  // Initializes the store and retrieves the existing cookies. This will be
  // called only once at startup. The callback will return all the cookies
  // that are not yet returned to CookieMonster by previous priority loads.
  virtual void Load(const LoadedCallback& loaded_callback) = 0;

  // Does a priority load of all cookies for the domain key (eTLD+1). The
  // callback will return all the cookies that are not yet returned by previous
  // loads, which includes cookies for the requested domain key if they are not
  // already returned, plus all cookies that are chain-loaded and not yet
  // returned to CookieMonster.
  virtual void LoadCookiesForKey(const std::string& key,
                                 const LoadedCallback& loaded_callback) = 0;

  virtual void AddCookie(const CanonicalCookie& cc) = 0;
  virtual void UpdateCookieAccessTime(const CanonicalCookie& cc) = 0;
  virtual void DeleteCookie(const CanonicalCookie& cc) = 0;

  // Instructs the store to not discard session only cookies on shutdown.
  virtual void SetForceKeepSessionState() = 0;

  // Flushes the store and posts |callback| when complete.
  virtual void Flush(const base::Closure& callback) = 0;

 protected:
  PersistentCookieStore() {}
  virtual ~PersistentCookieStore() {}

 private:
  friend class base::RefCountedThreadSafe<PersistentCookieStore>;
  DISALLOW_COPY_AND_ASSIGN(PersistentCookieStore);
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_MONSTER_H_
