// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/cookies/cookie_monster.h"

#include <algorithm>
#include <functional>
#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// In steady state, most cookie requests can be satisfied by the in memory
// cookie monster store.  However, if a request comes in during the initial
// cookie load, it must be delayed until that load completes. That is done by
// queueing it on CookieMonster::tasks_pending_ and running it when notification
// of cookie load completion is received via CookieMonster::OnLoaded. This
// callback is passed to the persistent store from CookieMonster::InitStore(),
// which is called on the first operation invoked on the CookieMonster.
//
// On the browser critical paths (e.g. for loading initial web pages in a
// session restore) it may take too long to wait for the full load. If a cookie
// request is for a specific URL, DoCookieTaskForURL is called, which triggers a
// priority load if the key is not loaded yet by calling PersistentCookieStore
// :: LoadCookiesForKey. The request is queued in
// CookieMonster::tasks_pending_for_key_ and executed upon receiving
// notification of key load completion via CookieMonster::OnKeyLoaded(). If
// multiple requests for the same eTLD+1 are received before key load
// completion, only the first request calls
// PersistentCookieStore::LoadCookiesForKey, all subsequent requests are queued
// in CookieMonster::tasks_pending_for_key_ and executed upon receiving
// notification of key load completion triggered by the first request for the
// same eTLD+1.

static const int kMinutesInTenYears = 10 * 365 * 24 * 60;

namespace net {

// See comments at declaration of these variables in cookie_monster.h
// for details.
const size_t CookieMonster::kDomainMaxCookies           = 180;
const size_t CookieMonster::kDomainPurgeCookies         = 30;
const size_t CookieMonster::kMaxCookies                 = 3300;
const size_t CookieMonster::kPurgeCookies               = 300;

const size_t CookieMonster::kDomainCookiesQuotaLow    = 30;
const size_t CookieMonster::kDomainCookiesQuotaMedium = 50;
const size_t CookieMonster::kDomainCookiesQuotaHigh   =
    kDomainMaxCookies - kDomainPurgeCookies
    - kDomainCookiesQuotaLow - kDomainCookiesQuotaMedium;

const int CookieMonster::kSafeFromGlobalPurgeDays       = 30;

namespace {

bool ContainsControlCharacter(const std::string& s) {
  for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
    if ((*i >= 0) && (*i <= 31))
      return true;
  }

  return false;
}

typedef std::vector<CanonicalCookie*> CanonicalCookieVector;

// Default minimum delay after updating a cookie's LastAccessDate before we
// will update it again.
const int kDefaultAccessUpdateThresholdSeconds = 60;

// Comparator to sort cookies from highest creation date to lowest
// creation date.
struct OrderByCreationTimeDesc {
  bool operator()(const CookieMonster::CookieMap::iterator& a,
                  const CookieMonster::CookieMap::iterator& b) const {
    return a->second->CreationDate() > b->second->CreationDate();
  }
};

// Constants for use in VLOG
const int kVlogPerCookieMonster = 1;
const int kVlogPeriodic = 3;
const int kVlogGarbageCollection = 5;
const int kVlogSetCookies = 7;
const int kVlogGetCookies = 9;

// Mozilla sorts on the path length (longest first), and then it
// sorts by creation time (oldest first).
// The RFC says the sort order for the domain attribute is undefined.
bool CookieSorter(CanonicalCookie* cc1, CanonicalCookie* cc2) {
  if (cc1->Path().length() == cc2->Path().length())
    return cc1->CreationDate() < cc2->CreationDate();
  return cc1->Path().length() > cc2->Path().length();
}

bool LRACookieSorter(const CookieMonster::CookieMap::iterator& it1,
                     const CookieMonster::CookieMap::iterator& it2) {
  // Cookies accessed less recently should be deleted first.
  if (it1->second->LastAccessDate() != it2->second->LastAccessDate())
    return it1->second->LastAccessDate() < it2->second->LastAccessDate();

  // In rare cases we might have two cookies with identical last access times.
  // To preserve the stability of the sort, in these cases prefer to delete
  // older cookies over newer ones.  CreationDate() is guaranteed to be unique.
  return it1->second->CreationDate() < it2->second->CreationDate();
}

// Our strategy to find duplicates is:
// (1) Build a map from (cookiename, cookiepath) to
//     {list of cookies with this signature, sorted by creation time}.
// (2) For each list with more than 1 entry, keep the cookie having the
//     most recent creation time, and delete the others.
//
// Two cookies are considered equivalent if they have the same domain,
// name, and path.
struct CookieSignature {
 public:
  CookieSignature(const std::string& name,
                  const std::string& domain,
                  const std::string& path)
      : name(name), domain(domain), path(path) {
  }

  // To be a key for a map this class needs to be assignable, copyable,
  // and have an operator<.  The default assignment operator
  // and copy constructor are exactly what we want.

  bool operator<(const CookieSignature& cs) const {
    // Name compare dominates, then domain, then path.
    int diff = name.compare(cs.name);
    if (diff != 0)
      return diff < 0;

    diff = domain.compare(cs.domain);
    if (diff != 0)
      return diff < 0;

    return path.compare(cs.path) < 0;
  }

  std::string name;
  std::string domain;
  std::string path;
};

// For a CookieItVector iterator range [|it_begin|, |it_end|),
// sorts the first |num_sort| + 1 elements by LastAccessDate().
// The + 1 element exists so for any interval of length <= |num_sort| starting
// from |cookies_its_begin|, a LastAccessDate() bound can be found.
void SortLeastRecentlyAccessed(
    CookieMonster::CookieItVector::iterator it_begin,
    CookieMonster::CookieItVector::iterator it_end,
    size_t num_sort) {
  DCHECK_LT(static_cast<int>(num_sort), it_end - it_begin);
  std::partial_sort(it_begin, it_begin + num_sort + 1, it_end, LRACookieSorter);
}

// Predicate to support PartitionCookieByPriority().
struct CookiePriorityEqualsTo
    : std::unary_function<const CookieMonster::CookieMap::iterator, bool> {
  CookiePriorityEqualsTo(CookiePriority priority)
    : priority_(priority) {}

  bool operator()(const CookieMonster::CookieMap::iterator it) const {
    return it->second->Priority() == priority_;
  }

  const CookiePriority priority_;
};

// For a CookieItVector iterator range [|it_begin|, |it_end|),
// moves all cookies with a given |priority| to the beginning of the list.
// Returns: An iterator in [it_begin, it_end) to the first element with
// priority != |priority|, or |it_end| if all have priority == |priority|.
CookieMonster::CookieItVector::iterator PartitionCookieByPriority(
    CookieMonster::CookieItVector::iterator it_begin,
    CookieMonster::CookieItVector::iterator it_end,
    CookiePriority priority) {
  return std::partition(it_begin, it_end, CookiePriorityEqualsTo(priority));
}

bool LowerBoundAccessDateComparator(
  const CookieMonster::CookieMap::iterator it, const Time& access_date) {
  return it->second->LastAccessDate() < access_date;
}

// For a CookieItVector iterator range [|it_begin|, |it_end|)
// from a CookieItVector sorted by LastAccessDate(), returns the
// first iterator with access date >= |access_date|, or cookie_its_end if this
// holds for all.
CookieMonster::CookieItVector::iterator LowerBoundAccessDate(
    const CookieMonster::CookieItVector::iterator its_begin,
    const CookieMonster::CookieItVector::iterator its_end,
    const Time& access_date) {
  return std::lower_bound(its_begin, its_end, access_date,
                          LowerBoundAccessDateComparator);
}

// Mapping between DeletionCause and CookieMonsterDelegate::ChangeCause; the
// mapping also provides a boolean that specifies whether or not an
// OnCookieChanged notification ought to be generated.
typedef struct ChangeCausePair_struct {
  CookieMonsterDelegate::ChangeCause cause;
  bool notify;
} ChangeCausePair;
ChangeCausePair ChangeCauseMapping[] = {
  // DELETE_COOKIE_EXPLICIT
  { CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, true },
  // DELETE_COOKIE_OVERWRITE
  { CookieMonsterDelegate::CHANGE_COOKIE_OVERWRITE, true },
  // DELETE_COOKIE_EXPIRED
  { CookieMonsterDelegate::CHANGE_COOKIE_EXPIRED, true },
  // DELETE_COOKIE_EVICTED
  { CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE
  { CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, false },
  // DELETE_COOKIE_DONT_RECORD
  { CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, false },
  // DELETE_COOKIE_EVICTED_DOMAIN
  { CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EVICTED_GLOBAL
  { CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE
  { CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE
  { CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EXPIRED_OVERWRITE
  { CookieMonsterDelegate::CHANGE_COOKIE_EXPIRED_OVERWRITE, true },
  // DELETE_COOKIE_CONTROL_CHAR
  { CookieMonsterDelegate::CHANGE_COOKIE_EVICTED, true},
  // DELETE_COOKIE_LAST_ENTRY
  { CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT, false }
};

std::string BuildCookieLine(const CanonicalCookieVector& cookies) {
  std::string cookie_line;
  for (CanonicalCookieVector::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (it != cookies.begin())
      cookie_line += "; ";
    // In Mozilla if you set a cookie like AAAA, it will have an empty token
    // and a value of AAAA.  When it sends the cookie back, it will send AAAA,
    // so we need to avoid sending =AAAA for a blank token value.
    if (!(*it)->Name().empty())
      cookie_line += (*it)->Name() + "=";
    cookie_line += (*it)->Value();
  }
  return cookie_line;
}

}  // namespace

CookieMonster::CookieMonster(PersistentCookieStore* store,
                             CookieMonsterDelegate* delegate)
    : initialized_(false),
      loaded_(store == NULL),
      store_(store),
      last_access_threshold_(
          TimeDelta::FromSeconds(kDefaultAccessUpdateThresholdSeconds)),
      delegate_(delegate),
      last_statistic_record_time_(Time::Now()),
      keep_expired_cookies_(false),
      persist_session_cookies_(false) {
  InitializeHistograms();
  SetDefaultCookieableSchemes();
}

CookieMonster::CookieMonster(PersistentCookieStore* store,
                             CookieMonsterDelegate* delegate,
                             int last_access_threshold_milliseconds)
    : initialized_(false),
      loaded_(store == NULL),
      store_(store),
      last_access_threshold_(base::TimeDelta::FromMilliseconds(
          last_access_threshold_milliseconds)),
      delegate_(delegate),
      last_statistic_record_time_(base::Time::Now()),
      keep_expired_cookies_(false),
      persist_session_cookies_(false) {
  InitializeHistograms();
  SetDefaultCookieableSchemes();
}


// Task classes for queueing the coming request.

class CookieMonster::CookieMonsterTask
    : public base::RefCountedThreadSafe<CookieMonsterTask> {
 public:
  // Runs the task and invokes the client callback on the thread that
  // originally constructed the task.
  virtual void Run() = 0;

 protected:
  explicit CookieMonsterTask(CookieMonster* cookie_monster);
  virtual ~CookieMonsterTask();

  // Invokes the callback immediately, if the current thread is the one
  // that originated the task, or queues the callback for execution on the
  // appropriate thread. Maintains a reference to this CookieMonsterTask
  // instance until the callback completes.
  void InvokeCallback(base::Closure callback);

  CookieMonster* cookie_monster() {
    return cookie_monster_;
  }

 private:
  friend class base::RefCountedThreadSafe<CookieMonsterTask>;

  CookieMonster* cookie_monster_;
  scoped_refptr<base::MessageLoopProxy> thread_;

  DISALLOW_COPY_AND_ASSIGN(CookieMonsterTask);
};

CookieMonster::CookieMonsterTask::CookieMonsterTask(
    CookieMonster* cookie_monster)
    : cookie_monster_(cookie_monster),
      thread_(base::MessageLoopProxy::current()) {
}

CookieMonster::CookieMonsterTask::~CookieMonsterTask() {}

// Unfortunately, one cannot re-bind a Callback with parameters into a closure.
// Therefore, the closure passed to InvokeCallback is a clumsy binding of
// Callback::Run on a wrapped Callback instance. Since Callback is not
// reference counted, we bind to an instance that is a member of the
// CookieMonsterTask subclass. Then, we cannot simply post the callback to a
// message loop because the underlying instance may be destroyed (along with the
// CookieMonsterTask instance) in the interim. Therefore, we post a callback
// bound to the CookieMonsterTask, which *is* reference counted (thus preventing
// destruction of the original callback), and which invokes the closure (which
// invokes the original callback with the returned data).
void CookieMonster::CookieMonsterTask::InvokeCallback(base::Closure callback) {
  if (thread_->BelongsToCurrentThread()) {
    callback.Run();
  } else {
    thread_->PostTask(FROM_HERE, base::Bind(
        &CookieMonsterTask::InvokeCallback, this, callback));
  }
}

// Task class for SetCookieWithDetails call.
class CookieMonster::SetCookieWithDetailsTask : public CookieMonsterTask {
 public:
  SetCookieWithDetailsTask(CookieMonster* cookie_monster,
                           const GURL& url,
                           const std::string& name,
                           const std::string& value,
                           const std::string& domain,
                           const std::string& path,
                           const base::Time& expiration_time,
                           bool secure,
                           bool http_only,
                           CookiePriority priority,
                           const SetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        name_(name),
        value_(value),
        domain_(domain),
        path_(path),
        expiration_time_(expiration_time),
        secure_(secure),
        http_only_(http_only),
        priority_(priority),
        callback_(callback) {
  }

  // CookieMonsterTask:
  virtual void Run() OVERRIDE;

 protected:
  virtual ~SetCookieWithDetailsTask() {}

 private:
  GURL url_;
  std::string name_;
  std::string value_;
  std::string domain_;
  std::string path_;
  base::Time expiration_time_;
  bool secure_;
  bool http_only_;
  CookiePriority priority_;
  SetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SetCookieWithDetailsTask);
};

void CookieMonster::SetCookieWithDetailsTask::Run() {
  bool success = this->cookie_monster()->
      SetCookieWithDetails(url_, name_, value_, domain_, path_,
                           expiration_time_, secure_, http_only_, priority_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&SetCookiesCallback::Run,
                                    base::Unretained(&callback_), success));
  }
}

// Task class for GetAllCookies call.
class CookieMonster::GetAllCookiesTask : public CookieMonsterTask {
 public:
  GetAllCookiesTask(CookieMonster* cookie_monster,
                    const GetCookieListCallback& callback)
      : CookieMonsterTask(cookie_monster),
        callback_(callback) {
  }

  // CookieMonsterTask
  virtual void Run() OVERRIDE;

 protected:
  virtual ~GetAllCookiesTask() {}

 private:
  GetCookieListCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetAllCookiesTask);
};

void CookieMonster::GetAllCookiesTask::Run() {
  if (!callback_.is_null()) {
    CookieList cookies = this->cookie_monster()->GetAllCookies();
    this->InvokeCallback(base::Bind(&GetCookieListCallback::Run,
                                    base::Unretained(&callback_), cookies));
    }
}

// Task class for GetAllCookiesForURLWithOptions call.
class CookieMonster::GetAllCookiesForURLWithOptionsTask
    : public CookieMonsterTask {
 public:
  GetAllCookiesForURLWithOptionsTask(
      CookieMonster* cookie_monster,
      const GURL& url,
      const CookieOptions& options,
      const GetCookieListCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        options_(options),
        callback_(callback) {
  }

  // CookieMonsterTask:
  virtual void Run() OVERRIDE;

 protected:
  virtual ~GetAllCookiesForURLWithOptionsTask() {}

 private:
  GURL url_;
  CookieOptions options_;
  GetCookieListCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetAllCookiesForURLWithOptionsTask);
};

void CookieMonster::GetAllCookiesForURLWithOptionsTask::Run() {
  if (!callback_.is_null()) {
    CookieList cookies = this->cookie_monster()->
        GetAllCookiesForURLWithOptions(url_, options_);
    this->InvokeCallback(base::Bind(&GetCookieListCallback::Run,
                                    base::Unretained(&callback_), cookies));
  }
}

template <typename Result> struct CallbackType {
  typedef base::Callback<void(Result)> Type;
};

template <> struct CallbackType<void> {
  typedef base::Closure Type;
};

// Base task class for Delete*Task.
template <typename Result>
class CookieMonster::DeleteTask : public CookieMonsterTask {
 public:
  DeleteTask(CookieMonster* cookie_monster,
             const typename CallbackType<Result>::Type& callback)
      : CookieMonsterTask(cookie_monster),
        callback_(callback) {
  }

  // CookieMonsterTask:
  virtual void Run() OVERRIDE;

 private:
  // Runs the delete task and returns a result.
  virtual Result RunDeleteTask() = 0;
  base::Closure RunDeleteTaskAndBindCallback();
  void FlushDone(const base::Closure& callback);

  typename CallbackType<Result>::Type callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteTask);
};

template <typename Result>
base::Closure CookieMonster::DeleteTask<Result>::
RunDeleteTaskAndBindCallback() {
  Result result = RunDeleteTask();
  if (callback_.is_null())
    return base::Closure();
  return base::Bind(callback_, result);
}

template <>
base::Closure CookieMonster::DeleteTask<void>::RunDeleteTaskAndBindCallback() {
  RunDeleteTask();
  return callback_;
}

template <typename Result>
void CookieMonster::DeleteTask<Result>::Run() {
  this->cookie_monster()->FlushStore(
      base::Bind(&DeleteTask<Result>::FlushDone, this,
                 RunDeleteTaskAndBindCallback()));
}

template <typename Result>
void CookieMonster::DeleteTask<Result>::FlushDone(
    const base::Closure& callback) {
  if (!callback.is_null()) {
    this->InvokeCallback(callback);
  }
}

// Task class for DeleteAll call.
class CookieMonster::DeleteAllTask : public DeleteTask<int> {
 public:
  DeleteAllTask(CookieMonster* cookie_monster,
                const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback) {
  }

  // DeleteTask:
  virtual int RunDeleteTask() OVERRIDE;

 protected:
  virtual ~DeleteAllTask() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteAllTask);
};

int CookieMonster::DeleteAllTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteAll(true);
}

// Task class for DeleteAllCreatedBetween call.
class CookieMonster::DeleteAllCreatedBetweenTask : public DeleteTask<int> {
 public:
  DeleteAllCreatedBetweenTask(CookieMonster* cookie_monster,
                              const Time& delete_begin,
                              const Time& delete_end,
                              const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback),
        delete_begin_(delete_begin),
        delete_end_(delete_end) {
  }

  // DeleteTask:
  virtual int RunDeleteTask() OVERRIDE;

 protected:
  virtual ~DeleteAllCreatedBetweenTask() {}

 private:
  Time delete_begin_;
  Time delete_end_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllCreatedBetweenTask);
};

int CookieMonster::DeleteAllCreatedBetweenTask::RunDeleteTask() {
  return this->cookie_monster()->
      DeleteAllCreatedBetween(delete_begin_, delete_end_);
}

// Task class for DeleteAllForHost call.
class CookieMonster::DeleteAllForHostTask : public DeleteTask<int> {
 public:
  DeleteAllForHostTask(CookieMonster* cookie_monster,
                       const GURL& url,
                       const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback),
        url_(url) {
  }

  // DeleteTask:
  virtual int RunDeleteTask() OVERRIDE;

 protected:
  virtual ~DeleteAllForHostTask() {}

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllForHostTask);
};

int CookieMonster::DeleteAllForHostTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteAllForHost(url_);
}

// Task class for DeleteAllCreatedBetweenForHost call.
class CookieMonster::DeleteAllCreatedBetweenForHostTask
    : public DeleteTask<int> {
 public:
  DeleteAllCreatedBetweenForHostTask(
      CookieMonster* cookie_monster,
      Time delete_begin,
      Time delete_end,
      const GURL& url,
      const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback),
        delete_begin_(delete_begin),
        delete_end_(delete_end),
        url_(url) {
  }

  // DeleteTask:
  virtual int RunDeleteTask() OVERRIDE;

 protected:
  virtual ~DeleteAllCreatedBetweenForHostTask() {}

 private:
  Time delete_begin_;
  Time delete_end_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllCreatedBetweenForHostTask);
};

int CookieMonster::DeleteAllCreatedBetweenForHostTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteAllCreatedBetweenForHost(
      delete_begin_, delete_end_, url_);
}

// Task class for DeleteCanonicalCookie call.
class CookieMonster::DeleteCanonicalCookieTask : public DeleteTask<bool> {
 public:
  DeleteCanonicalCookieTask(CookieMonster* cookie_monster,
                            const CanonicalCookie& cookie,
                            const DeleteCookieCallback& callback)
      : DeleteTask<bool>(cookie_monster, callback),
        cookie_(cookie) {
  }

  // DeleteTask:
  virtual bool RunDeleteTask() OVERRIDE;

 protected:
  virtual ~DeleteCanonicalCookieTask() {}

 private:
  CanonicalCookie cookie_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCanonicalCookieTask);
};

bool CookieMonster::DeleteCanonicalCookieTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteCanonicalCookie(cookie_);
}

// Task class for SetCookieWithOptions call.
class CookieMonster::SetCookieWithOptionsTask : public CookieMonsterTask {
 public:
  SetCookieWithOptionsTask(CookieMonster* cookie_monster,
                           const GURL& url,
                           const std::string& cookie_line,
                           const CookieOptions& options,
                           const SetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        cookie_line_(cookie_line),
        options_(options),
        callback_(callback) {
  }

  // CookieMonsterTask:
  virtual void Run() OVERRIDE;

 protected:
  virtual ~SetCookieWithOptionsTask() {}

 private:
  GURL url_;
  std::string cookie_line_;
  CookieOptions options_;
  SetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SetCookieWithOptionsTask);
};

void CookieMonster::SetCookieWithOptionsTask::Run() {
  bool result = this->cookie_monster()->
      SetCookieWithOptions(url_, cookie_line_, options_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&SetCookiesCallback::Run,
                                    base::Unretained(&callback_), result));
  }
}

// Task class for GetCookiesWithOptions call.
class CookieMonster::GetCookiesWithOptionsTask : public CookieMonsterTask {
 public:
  GetCookiesWithOptionsTask(CookieMonster* cookie_monster,
                            const GURL& url,
                            const CookieOptions& options,
                            const GetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        options_(options),
        callback_(callback) {
  }

  // CookieMonsterTask:
  virtual void Run() OVERRIDE;

 protected:
  virtual ~GetCookiesWithOptionsTask() {}

 private:
  GURL url_;
  CookieOptions options_;
  GetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetCookiesWithOptionsTask);
};

void CookieMonster::GetCookiesWithOptionsTask::Run() {
  std::string cookie = this->cookie_monster()->
      GetCookiesWithOptions(url_, options_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&GetCookiesCallback::Run,
                                    base::Unretained(&callback_), cookie));
  }
}

// Task class for DeleteCookie call.
class CookieMonster::DeleteCookieTask : public DeleteTask<void> {
 public:
  DeleteCookieTask(CookieMonster* cookie_monster,
                   const GURL& url,
                   const std::string& cookie_name,
                   const base::Closure& callback)
      : DeleteTask<void>(cookie_monster, callback),
        url_(url),
        cookie_name_(cookie_name) {
  }

  // DeleteTask:
  virtual void RunDeleteTask() OVERRIDE;

 protected:
  virtual ~DeleteCookieTask() {}

 private:
  GURL url_;
  std::string cookie_name_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCookieTask);
};

void CookieMonster::DeleteCookieTask::RunDeleteTask() {
  this->cookie_monster()->DeleteCookie(url_, cookie_name_);
}

// Task class for DeleteSessionCookies call.
class CookieMonster::DeleteSessionCookiesTask : public DeleteTask<int> {
 public:
  DeleteSessionCookiesTask(CookieMonster* cookie_monster,
                           const DeleteCallback& callback)
      : DeleteTask<int>(cookie_monster, callback) {
  }

  // DeleteTask:
  virtual int RunDeleteTask() OVERRIDE;

 protected:
  virtual ~DeleteSessionCookiesTask() {}

 private:

  DISALLOW_COPY_AND_ASSIGN(DeleteSessionCookiesTask);
};

int CookieMonster::DeleteSessionCookiesTask::RunDeleteTask() {
  return this->cookie_monster()->DeleteSessionCookies();
}

// Task class for HasCookiesForETLDP1Task call.
class CookieMonster::HasCookiesForETLDP1Task : public CookieMonsterTask {
 public:
  HasCookiesForETLDP1Task(
      CookieMonster* cookie_monster,
      const std::string& etldp1,
      const HasCookiesForETLDP1Callback& callback)
      : CookieMonsterTask(cookie_monster),
        etldp1_(etldp1),
        callback_(callback) {
  }

  // CookieMonsterTask:
  virtual void Run() OVERRIDE;

 protected:
  virtual ~HasCookiesForETLDP1Task() {}

 private:
  std::string etldp1_;
  HasCookiesForETLDP1Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(HasCookiesForETLDP1Task);
};

void CookieMonster::HasCookiesForETLDP1Task::Run() {
  bool result = this->cookie_monster()->HasCookiesForETLDP1(etldp1_);
  if (!callback_.is_null()) {
    this->InvokeCallback(
        base::Bind(&HasCookiesForETLDP1Callback::Run,
                   base::Unretained(&callback_), result));
  }
}

// Asynchronous CookieMonster API

void CookieMonster::SetCookieWithDetailsAsync(
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    const Time& expiration_time,
    bool secure,
    bool http_only,
    CookiePriority priority,
    const SetCookiesCallback& callback) {
  scoped_refptr<SetCookieWithDetailsTask> task =
      new SetCookieWithDetailsTask(this, url, name, value, domain, path,
                                   expiration_time, secure, http_only, priority,
                                   callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetAllCookiesAsync(const GetCookieListCallback& callback) {
  scoped_refptr<GetAllCookiesTask> task =
      new GetAllCookiesTask(this, callback);

  DoCookieTask(task);
}


void CookieMonster::GetAllCookiesForURLWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const GetCookieListCallback& callback) {
  scoped_refptr<GetAllCookiesForURLWithOptionsTask> task =
      new GetAllCookiesForURLWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetAllCookiesForURLAsync(
    const GURL& url, const GetCookieListCallback& callback) {
  CookieOptions options;
  options.set_include_httponly();
  scoped_refptr<GetAllCookiesForURLWithOptionsTask> task =
      new GetAllCookiesForURLWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::HasCookiesForETLDP1Async(
    const std::string& etldp1,
    const HasCookiesForETLDP1Callback& callback) {
  scoped_refptr<HasCookiesForETLDP1Task> task =
      new HasCookiesForETLDP1Task(this, etldp1, callback);

  DoCookieTaskForURL(task, GURL("http://" + etldp1));
}

void CookieMonster::DeleteAllAsync(const DeleteCallback& callback) {
  scoped_refptr<DeleteAllTask> task =
      new DeleteAllTask(this, callback);

  DoCookieTask(task);
}

void CookieMonster::DeleteAllCreatedBetweenAsync(
    const Time& delete_begin, const Time& delete_end,
    const DeleteCallback& callback) {
  scoped_refptr<DeleteAllCreatedBetweenTask> task =
      new DeleteAllCreatedBetweenTask(this, delete_begin, delete_end,
                                      callback);

  DoCookieTask(task);
}

void CookieMonster::DeleteAllCreatedBetweenForHostAsync(
    const Time delete_begin,
    const Time delete_end,
    const GURL& url,
    const DeleteCallback& callback) {
  scoped_refptr<DeleteAllCreatedBetweenForHostTask> task =
      new DeleteAllCreatedBetweenForHostTask(
          this, delete_begin, delete_end, url, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteAllForHostAsync(
    const GURL& url, const DeleteCallback& callback) {
  scoped_refptr<DeleteAllForHostTask> task =
      new DeleteAllForHostTask(this, url, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    const DeleteCookieCallback& callback) {
  scoped_refptr<DeleteCanonicalCookieTask> task =
      new DeleteCanonicalCookieTask(this, cookie, callback);

  DoCookieTask(task);
}

void CookieMonster::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const CookieOptions& options,
    const SetCookiesCallback& callback) {
  scoped_refptr<SetCookieWithOptionsTask> task =
      new SetCookieWithOptionsTask(this, url, cookie_line, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetCookiesWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const GetCookiesCallback& callback) {
  scoped_refptr<GetCookiesWithOptionsTask> task =
      new GetCookiesWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteCookieAsync(const GURL& url,
                                      const std::string& cookie_name,
                                      const base::Closure& callback) {
  scoped_refptr<DeleteCookieTask> task =
      new DeleteCookieTask(this, url, cookie_name, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteSessionCookiesAsync(
    const CookieStore::DeleteCallback& callback) {
  scoped_refptr<DeleteSessionCookiesTask> task =
      new DeleteSessionCookiesTask(this, callback);

  DoCookieTask(task);
}

void CookieMonster::DoCookieTask(
    const scoped_refptr<CookieMonsterTask>& task_item) {
  {
    base::AutoLock autolock(lock_);
    InitIfNecessary();
    if (!loaded_) {
      tasks_pending_.push(task_item);
      return;
    }
  }

  task_item->Run();
}

void CookieMonster::DoCookieTaskForURL(
    const scoped_refptr<CookieMonsterTask>& task_item,
    const GURL& url) {
  {
    base::AutoLock autolock(lock_);
    InitIfNecessary();
    // If cookies for the requested domain key (eTLD+1) have been loaded from DB
    // then run the task, otherwise load from DB.
    if (!loaded_) {
      // Checks if the domain key has been loaded.
      std::string key(cookie_util::GetEffectiveDomain(url.scheme(),
                                                       url.host()));
      if (keys_loaded_.find(key) == keys_loaded_.end()) {
        std::map<std::string, std::deque<scoped_refptr<CookieMonsterTask> > >
          ::iterator it = tasks_pending_for_key_.find(key);
        if (it == tasks_pending_for_key_.end()) {
          store_->LoadCookiesForKey(key,
            base::Bind(&CookieMonster::OnKeyLoaded, this, key));
          it = tasks_pending_for_key_.insert(std::make_pair(key,
            std::deque<scoped_refptr<CookieMonsterTask> >())).first;
        }
        it->second.push_back(task_item);
        return;
      }
    }
  }
  task_item->Run();
}

bool CookieMonster::SetCookieWithDetails(const GURL& url,
                                         const std::string& name,
                                         const std::string& value,
                                         const std::string& domain,
                                         const std::string& path,
                                         const base::Time& expiration_time,
                                         bool secure,
                                         bool http_only,
                                         CookiePriority priority) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return false;

  Time creation_time = CurrentTime();
  last_time_seen_ = creation_time;

  scoped_ptr<CanonicalCookie> cc;
  cc.reset(CanonicalCookie::Create(url, name, value, domain, path,
                                   creation_time, expiration_time,
                                   secure, http_only, priority));

  if (!cc.get())
    return false;

  CookieOptions options;
  options.set_include_httponly();
  return SetCanonicalCookie(&cc, creation_time, options);
}

bool CookieMonster::InitializeFrom(const CookieList& list) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  for (net::CookieList::const_iterator iter = list.begin();
           iter != list.end(); ++iter) {
    scoped_ptr<CanonicalCookie> cookie(new CanonicalCookie(*iter));
    net::CookieOptions options;
    options.set_include_httponly();
    if (!SetCanonicalCookie(&cookie, cookie->CreationDate(), options))
      return false;
  }
  return true;
}

CookieList CookieMonster::GetAllCookies() {
  base::AutoLock autolock(lock_);

  // This function is being called to scrape the cookie list for management UI
  // or similar.  We shouldn't show expired cookies in this list since it will
  // just be confusing to users, and this function is called rarely enough (and
  // is already slow enough) that it's OK to take the time to garbage collect
  // the expired cookies now.
  //
  // Note that this does not prune cookies to be below our limits (if we've
  // exceeded them) the way that calling GarbageCollect() would.
  GarbageCollectExpired(Time::Now(),
                        CookieMapItPair(cookies_.begin(), cookies_.end()),
                        NULL);

  // Copy the CanonicalCookie pointers from the map so that we can use the same
  // sorter as elsewhere, then copy the result out.
  std::vector<CanonicalCookie*> cookie_ptrs;
  cookie_ptrs.reserve(cookies_.size());
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end(); ++it)
    cookie_ptrs.push_back(it->second);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookie_list;
  cookie_list.reserve(cookie_ptrs.size());
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); ++it)
    cookie_list.push_back(**it);

  return cookie_list;
}

CookieList CookieMonster::GetAllCookiesForURLWithOptions(
    const GURL& url,
    const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  std::vector<CanonicalCookie*> cookie_ptrs;
  FindCookiesForHostAndDomain(url, options, false, &cookie_ptrs);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookies;
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); it++)
    cookies.push_back(**it);

  return cookies;
}

CookieList CookieMonster::GetAllCookiesForURL(const GURL& url) {
  CookieOptions options;
  options.set_include_httponly();

  return GetAllCookiesForURLWithOptions(url, options);
}

int CookieMonster::DeleteAll(bool sync_to_store) {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    InternalDeleteCookie(curit, sync_to_store,
                         sync_to_store ? DELETE_COOKIE_EXPLICIT :
                             DELETE_COOKIE_DONT_RECORD /* Destruction. */);
    ++num_deleted;
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedBetween(const Time& delete_begin,
                                           const Time& delete_end) {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (cc->CreationDate() >= delete_begin &&
        (delete_end.is_null() || cc->CreationDate() < delete_end)) {
      InternalDeleteCookie(curit,
                           true,  /*sync_to_store*/
                           DELETE_COOKIE_EXPLICIT);
      ++num_deleted;
    }
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedBetweenForHost(const Time delete_begin,
                                                  const Time delete_end,
                                                  const GURL& url) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return 0;

  const std::string host(url.host());

  // We store host cookies in the store by their canonical host name;
  // domain cookies are stored with a leading ".".  So this is a pretty
  // simple lookup and per-cookie delete.
  int num_deleted = 0;
  for (CookieMapItPair its = cookies_.equal_range(GetKey(host));
       its.first != its.second;) {
    CookieMap::iterator curit = its.first;
    ++its.first;

    const CanonicalCookie* const cc = curit->second;

    // Delete only on a match as a host cookie.
    if (cc->IsHostCookie() && cc->IsDomainMatch(host) &&
        cc->CreationDate() >= delete_begin &&
        // The assumption that null |delete_end| is equivalent to
        // Time::Max() is confusing.
        (delete_end.is_null() || cc->CreationDate() < delete_end)) {
      num_deleted++;

      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
  return num_deleted;
}

int CookieMonster::DeleteAllForHost(const GURL& url) {
  return DeleteAllCreatedBetweenForHost(Time(), Time::Max(), url);
}


bool CookieMonster::DeleteCanonicalCookie(const CanonicalCookie& cookie) {
  base::AutoLock autolock(lock_);

  for (CookieMapItPair its = cookies_.equal_range(GetKey(cookie.Domain()));
       its.first != its.second; ++its.first) {
    // The creation date acts as our unique index...
    if (its.first->second->CreationDate() == cookie.CreationDate()) {
      InternalDeleteCookie(its.first, true, DELETE_COOKIE_EXPLICIT);
      return true;
    }
  }
  return false;
}

void CookieMonster::SetCookieableSchemes(const char* schemes[],
                                         size_t num_schemes) {
  base::AutoLock autolock(lock_);

  // Cookieable Schemes must be set before first use of function.
  DCHECK(!initialized_);

  cookieable_schemes_.clear();
  cookieable_schemes_.insert(cookieable_schemes_.end(),
                             schemes, schemes + num_schemes);
}

void CookieMonster::SetEnableFileScheme(bool accept) {
  // This assumes "file" is always at the end of the array. See the comment
  // above kDefaultCookieableSchemes.
  int num_schemes = accept ? kDefaultCookieableSchemesCount :
      kDefaultCookieableSchemesCount - 1;
  SetCookieableSchemes(kDefaultCookieableSchemes, num_schemes);
}

void CookieMonster::SetKeepExpiredCookies() {
  keep_expired_cookies_ = true;
}

void CookieMonster::FlushStore(const base::Closure& callback) {
  base::AutoLock autolock(lock_);
  if (initialized_ && store_.get())
    store_->Flush(callback);
  else if (!callback.is_null())
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

bool CookieMonster::SetCookieWithOptions(const GURL& url,
                                         const std::string& cookie_line,
                                         const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  return SetCookieWithCreationTimeAndOptions(url, cookie_line, Time(), options);
}

std::string CookieMonster::GetCookiesWithOptions(const GURL& url,
                                                 const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return std::string();

  TimeTicks start_time(TimeTicks::Now());

  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::sort(cookies.begin(), cookies.end(), CookieSorter);

  std::string cookie_line = BuildCookieLine(cookies);

  histogram_time_get_->AddTime(TimeTicks::Now() - start_time);

  VLOG(kVlogGetCookies) << "GetCookies() result: " << cookie_line;

  return cookie_line;
}

void CookieMonster::DeleteCookie(const GURL& url,
                                 const std::string& cookie_name) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return;

  CookieOptions options;
  options.set_include_httponly();
  // Get the cookies for this host and its domain(s).
  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::set<CanonicalCookie*> matching_cookies;

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if ((*it)->Name() != cookie_name)
      continue;
    if (url.path().find((*it)->Path()))
      continue;
    matching_cookies.insert(*it);
  }

  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    if (matching_cookies.find(curit->second) != matching_cookies.end()) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
}

int CookieMonster::DeleteSessionCookies() {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (!cc->IsPersistent()) {
      InternalDeleteCookie(curit,
                           true,  /*sync_to_store*/
                           DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    }
  }

  return num_deleted;
}

bool CookieMonster::HasCookiesForETLDP1(const std::string& etldp1) {
  base::AutoLock autolock(lock_);

  const std::string key(GetKey(etldp1));

  CookieMapItPair its = cookies_.equal_range(key);
  return its.first != its.second;
}

CookieMonster* CookieMonster::GetCookieMonster() {
  return this;
}

// This function must be called before the CookieMonster is used.
void CookieMonster::SetPersistSessionCookies(bool persist_session_cookies) {
  DCHECK(!initialized_);
  persist_session_cookies_ = persist_session_cookies;
}

void CookieMonster::SetForceKeepSessionState() {
  if (store_.get()) {
    store_->SetForceKeepSessionState();
  }
}

CookieMonster::~CookieMonster() {
  DeleteAll(false);
}

bool CookieMonster::SetCookieWithCreationTime(const GURL& url,
                                              const std::string& cookie_line,
                                              const base::Time& creation_time) {
  DCHECK(!store_.get()) << "This method is only to be used by unit-tests.";
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  InitIfNecessary();
  return SetCookieWithCreationTimeAndOptions(url, cookie_line, creation_time,
                                             CookieOptions());
}

void CookieMonster::InitStore() {
  DCHECK(store_.get()) << "Store must exist to initialize";

  // We bind in the current time so that we can report the wall-clock time for
  // loading cookies.
  store_->Load(base::Bind(&CookieMonster::OnLoaded, this, TimeTicks::Now()));
}

void CookieMonster::ReportLoaded() {
  if (delegate_.get())
    delegate_->OnLoaded();
}

void CookieMonster::OnLoaded(TimeTicks beginning_time,
                             const std::vector<CanonicalCookie*>& cookies) {
  StoreLoadedCookies(cookies);
  histogram_time_blocked_on_load_->AddTime(TimeTicks::Now() - beginning_time);

  // Invoke the task queue of cookie request.
  InvokeQueue();

  ReportLoaded();
}

void CookieMonster::OnKeyLoaded(const std::string& key,
                                const std::vector<CanonicalCookie*>& cookies) {
  // This function does its own separate locking.
  StoreLoadedCookies(cookies);

  std::deque<scoped_refptr<CookieMonsterTask> > tasks_pending_for_key;

  // We need to do this repeatedly until no more tasks were added to the queue
  // during the period where we release the lock.
  while (true) {
    {
      base::AutoLock autolock(lock_);
      std::map<std::string, std::deque<scoped_refptr<CookieMonsterTask> > >
        ::iterator it = tasks_pending_for_key_.find(key);
      if (it == tasks_pending_for_key_.end()) {
        keys_loaded_.insert(key);
        return;
      }
      if (it->second.empty()) {
        keys_loaded_.insert(key);
        tasks_pending_for_key_.erase(it);
        return;
      }
      it->second.swap(tasks_pending_for_key);
    }

    while (!tasks_pending_for_key.empty()) {
      scoped_refptr<CookieMonsterTask> task = tasks_pending_for_key.front();
      task->Run();
      tasks_pending_for_key.pop_front();
    }
  }
}

void CookieMonster::StoreLoadedCookies(
    const std::vector<CanonicalCookie*>& cookies) {
  // Initialize the store and sync in any saved persistent cookies.  We don't
  // care if it's expired, insert it so it can be garbage collected, removed,
  // and sync'd.
  base::AutoLock autolock(lock_);

  CookieItVector cookies_with_control_chars;

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    int64 cookie_creation_time = (*it)->CreationDate().ToInternalValue();

    if (creation_times_.insert(cookie_creation_time).second) {
      CookieMap::iterator inserted =
          InternalInsertCookie(GetKey((*it)->Domain()), *it, false);
      const Time cookie_access_time((*it)->LastAccessDate());
      if (earliest_access_time_.is_null() ||
          cookie_access_time < earliest_access_time_)
        earliest_access_time_ = cookie_access_time;

      if (ContainsControlCharacter((*it)->Name()) ||
          ContainsControlCharacter((*it)->Value())) {
          cookies_with_control_chars.push_back(inserted);
      }
    } else {
      LOG(ERROR) << base::StringPrintf("Found cookies with duplicate creation "
                                       "times in backing store: "
                                       "{name='%s', domain='%s', path='%s'}",
                                       (*it)->Name().c_str(),
                                       (*it)->Domain().c_str(),
                                       (*it)->Path().c_str());
      // We've been given ownership of the cookie and are throwing it
      // away; reclaim the space.
      delete (*it);
    }
  }

  // Any cookies that contain control characters that we have loaded from the
  // persistent store should be deleted. See http://crbug.com/238041.
  for (CookieItVector::iterator it = cookies_with_control_chars.begin();
       it != cookies_with_control_chars.end();) {
    CookieItVector::iterator curit = it;
    ++it;

    InternalDeleteCookie(*curit, true, DELETE_COOKIE_CONTROL_CHAR);
  }

  // After importing cookies from the PersistentCookieStore, verify that
  // none of our other constraints are violated.
  // In particular, the backing store might have given us duplicate cookies.

  // This method could be called multiple times due to priority loading, thus
  // cookies loaded in previous runs will be validated again, but this is OK
  // since they are expected to be much fewer than total DB.
  EnsureCookiesMapIsValid();
}

void CookieMonster::InvokeQueue() {
  while (true) {
    scoped_refptr<CookieMonsterTask> request_task;
    {
      base::AutoLock autolock(lock_);
      if (tasks_pending_.empty()) {
        loaded_ = true;
        creation_times_.clear();
        keys_loaded_.clear();
        break;
      }
      request_task = tasks_pending_.front();
      tasks_pending_.pop();
    }
    request_task->Run();
  }
}

void CookieMonster::EnsureCookiesMapIsValid() {
  lock_.AssertAcquired();

  int num_duplicates_trimmed = 0;

  // Iterate through all the of the cookies, grouped by host.
  CookieMap::iterator prev_range_end = cookies_.begin();
  while (prev_range_end != cookies_.end()) {
    CookieMap::iterator cur_range_begin = prev_range_end;
    const std::string key = cur_range_begin->first;  // Keep a copy.
    CookieMap::iterator cur_range_end = cookies_.upper_bound(key);
    prev_range_end = cur_range_end;

    // Ensure no equivalent cookies for this host.
    num_duplicates_trimmed +=
        TrimDuplicateCookiesForKey(key, cur_range_begin, cur_range_end);
  }

  // Record how many duplicates were found in the database.
  // See InitializeHistograms() for details.
  histogram_cookie_deletion_cause_->Add(num_duplicates_trimmed);
}

int CookieMonster::TrimDuplicateCookiesForKey(
    const std::string& key,
    CookieMap::iterator begin,
    CookieMap::iterator end) {
  lock_.AssertAcquired();

  // Set of cookies ordered by creation time.
  typedef std::set<CookieMap::iterator, OrderByCreationTimeDesc> CookieSet;

  // Helper map we populate to find the duplicates.
  typedef std::map<CookieSignature, CookieSet> EquivalenceMap;
  EquivalenceMap equivalent_cookies;

  // The number of duplicate cookies that have been found.
  int num_duplicates = 0;

  // Iterate through all of the cookies in our range, and insert them into
  // the equivalence map.
  for (CookieMap::iterator it = begin; it != end; ++it) {
    DCHECK_EQ(key, it->first);
    CanonicalCookie* cookie = it->second;

    CookieSignature signature(cookie->Name(), cookie->Domain(),
                              cookie->Path());
    CookieSet& set = equivalent_cookies[signature];

    // We found a duplicate!
    if (!set.empty())
      num_duplicates++;

    // We save the iterator into |cookies_| rather than the actual cookie
    // pointer, since we may need to delete it later.
    bool insert_success = set.insert(it).second;
    DCHECK(insert_success) <<
        "Duplicate creation times found in duplicate cookie name scan.";
  }

  // If there were no duplicates, we are done!
  if (num_duplicates == 0)
    return 0;

  // Make sure we find everything below that we did above.
  int num_duplicates_found = 0;

  // Otherwise, delete all the duplicate cookies, both from our in-memory store
  // and from the backing store.
  for (EquivalenceMap::iterator it = equivalent_cookies.begin();
       it != equivalent_cookies.end();
       ++it) {
    const CookieSignature& signature = it->first;
    CookieSet& dupes = it->second;

    if (dupes.size() <= 1)
      continue;  // This cookiename/path has no duplicates.
    num_duplicates_found += dupes.size() - 1;

    // Since |dups| is sorted by creation time (descending), the first cookie
    // is the most recent one, so we will keep it. The rest are duplicates.
    dupes.erase(dupes.begin());

    LOG(ERROR) << base::StringPrintf(
        "Found %d duplicate cookies for host='%s', "
        "with {name='%s', domain='%s', path='%s'}",
        static_cast<int>(dupes.size()),
        key.c_str(),
        signature.name.c_str(),
        signature.domain.c_str(),
        signature.path.c_str());

    // Remove all the cookies identified by |dupes|. It is valid to delete our
    // list of iterators one at a time, since |cookies_| is a multimap (they
    // don't invalidate existing iterators following deletion).
    for (CookieSet::iterator dupes_it = dupes.begin();
         dupes_it != dupes.end();
         ++dupes_it) {
      InternalDeleteCookie(*dupes_it, true,
                           DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE);
    }
  }
  DCHECK_EQ(num_duplicates, num_duplicates_found);

  return num_duplicates;
}

// Note: file must be the last scheme.
const char* CookieMonster::kDefaultCookieableSchemes[] =
    { "http", "https", "ws", "wss", "file" };
const int CookieMonster::kDefaultCookieableSchemesCount =
    arraysize(kDefaultCookieableSchemes);

void CookieMonster::SetDefaultCookieableSchemes() {
  // Always disable file scheme unless SetEnableFileScheme(true) is called.
  SetCookieableSchemes(kDefaultCookieableSchemes,
                       kDefaultCookieableSchemesCount - 1);
}

void CookieMonster::FindCookiesForHostAndDomain(
    const GURL& url,
    const CookieOptions& options,
    bool update_access_time,
    std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();

  const Time current_time(CurrentTime());

  // Probe to save statistics relatively frequently.  We do it here rather
  // than in the set path as many websites won't set cookies, and we
  // want to collect statistics whenever the browser's being used.
  RecordPeriodicStats(current_time);

  // Can just dispatch to FindCookiesForKey
  const std::string key(GetKey(url.host()));
  FindCookiesForKey(key, url, options, current_time,
                    update_access_time, cookies);
}

void CookieMonster::FindCookiesForKey(const std::string& key,
                                      const GURL& url,
                                      const CookieOptions& options,
                                      const Time& current,
                                      bool update_access_time,
                                      std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();

  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    // If the cookie is expired, delete it.
    if (cc->IsExpired(current) && !keep_expired_cookies_) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      continue;
    }

    // Filter out cookies that should not be included for a request to the
    // given |url|. HTTP only cookies are filtered depending on the passed
    // cookie |options|.
    if (!cc->IncludeForRequestURL(url, options))
      continue;

    // Add this cookie to the set of matching cookies. Update the access
    // time if we've been requested to do so.
    if (update_access_time) {
      InternalUpdateCookieAccessTime(cc, current);
    }
    cookies->push_back(cc);
  }
}

bool CookieMonster::DeleteAnyEquivalentCookie(const std::string& key,
                                              const CanonicalCookie& ecc,
                                              bool skip_httponly,
                                              bool already_expired) {
  lock_.AssertAcquired();

  bool found_equivalent_cookie = false;
  bool skipped_httponly = false;
  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    if (ecc.IsEquivalent(*cc)) {
      // We should never have more than one equivalent cookie, since they should
      // overwrite each other.
      CHECK(!found_equivalent_cookie) <<
          "Duplicate equivalent cookies found, cookie store is corrupted.";
      if (skip_httponly && cc->IsHttpOnly()) {
        skipped_httponly = true;
      } else {
        InternalDeleteCookie(curit, true, already_expired ?
            DELETE_COOKIE_EXPIRED_OVERWRITE : DELETE_COOKIE_OVERWRITE);
      }
      found_equivalent_cookie = true;
    }
  }
  return skipped_httponly;
}

CookieMonster::CookieMap::iterator CookieMonster::InternalInsertCookie(
    const std::string& key,
    CanonicalCookie* cc,
    bool sync_to_store) {
  lock_.AssertAcquired();

  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store)
    store_->AddCookie(*cc);
  CookieMap::iterator inserted =
      cookies_.insert(CookieMap::value_type(key, cc));
  if (delegate_.get()) {
    delegate_->OnCookieChanged(
        *cc, false, CookieMonsterDelegate::CHANGE_COOKIE_EXPLICIT);
  }

  return inserted;
}

bool CookieMonster::SetCookieWithCreationTimeAndOptions(
    const GURL& url,
    const std::string& cookie_line,
    const Time& creation_time_or_null,
    const CookieOptions& options) {
  lock_.AssertAcquired();

  VLOG(kVlogSetCookies) << "SetCookie() line: " << cookie_line;

  Time creation_time = creation_time_or_null;
  if (creation_time.is_null()) {
    creation_time = CurrentTime();
    last_time_seen_ = creation_time;
  }

  scoped_ptr<CanonicalCookie> cc(
      CanonicalCookie::Create(url, cookie_line, creation_time, options));

  if (!cc.get()) {
    VLOG(kVlogSetCookies) << "WARNING: Failed to allocate CanonicalCookie";
    return false;
  }
  return SetCanonicalCookie(&cc, creation_time, options);
}

bool CookieMonster::SetCanonicalCookie(scoped_ptr<CanonicalCookie>* cc,
                                       const Time& creation_time,
                                       const CookieOptions& options) {
  const std::string key(GetKey((*cc)->Domain()));
  bool already_expired = (*cc)->IsExpired(creation_time);
  if (DeleteAnyEquivalentCookie(key, **cc, options.exclude_httponly(),
                                already_expired)) {
    VLOG(kVlogSetCookies) << "SetCookie() not clobbering httponly cookie";
    return false;
  }

  VLOG(kVlogSetCookies) << "SetCookie() key: " << key << " cc: "
                        << (*cc)->DebugString();

  // Realize that we might be setting an expired cookie, and the only point
  // was to delete the cookie which we've already done.
  if (!already_expired || keep_expired_cookies_) {
    // See InitializeHistograms() for details.
    if ((*cc)->IsPersistent()) {
      histogram_expiration_duration_minutes_->Add(
          ((*cc)->ExpiryDate() - creation_time).InMinutes());
    }

    InternalInsertCookie(key, cc->release(), true);
  } else {
    VLOG(kVlogSetCookies) << "SetCookie() not storing already expired cookie.";
  }

  // We assume that hopefully setting a cookie will be less common than
  // querying a cookie.  Since setting a cookie can put us over our limits,
  // make sure that we garbage collect...  We can also make the assumption that
  // if a cookie was set, in the common case it will be used soon after,
  // and we will purge the expired cookies in GetCookies().
  GarbageCollect(creation_time, key);

  return true;
}

void CookieMonster::InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                                   const Time& current) {
  lock_.AssertAcquired();

  // Based off the Mozilla code.  When a cookie has been accessed recently,
  // don't bother updating its access time again.  This reduces the number of
  // updates we do during pageload, which in turn reduces the chance our storage
  // backend will hit its batch thresholds and be forced to update.
  if ((current - cc->LastAccessDate()) < last_access_threshold_)
    return;

  // See InitializeHistograms() for details.
  histogram_between_access_interval_minutes_->Add(
      (current - cc->LastAccessDate()).InMinutes());

  cc->SetLastAccessDate(current);
  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get())
    store_->UpdateCookieAccessTime(*cc);
}

// InternalDeleteCookies must not invalidate iterators other than the one being
// deleted.
void CookieMonster::InternalDeleteCookie(CookieMap::iterator it,
                                         bool sync_to_store,
                                         DeletionCause deletion_cause) {
  lock_.AssertAcquired();

  // Ideally, this would be asserted up where we define ChangeCauseMapping,
  // but DeletionCause's visibility (or lack thereof) forces us to make
  // this check here.
  COMPILE_ASSERT(arraysize(ChangeCauseMapping) == DELETE_COOKIE_LAST_ENTRY + 1,
                 ChangeCauseMapping_size_not_eq_DeletionCause_enum_size);

  // See InitializeHistograms() for details.
  if (deletion_cause != DELETE_COOKIE_DONT_RECORD)
    histogram_cookie_deletion_cause_->Add(deletion_cause);

  CanonicalCookie* cc = it->second;
  VLOG(kVlogSetCookies) << "InternalDeleteCookie() cc: " << cc->DebugString();

  if ((cc->IsPersistent() || persist_session_cookies_) && store_.get() &&
      sync_to_store)
    store_->DeleteCookie(*cc);
  if (delegate_.get()) {
    ChangeCausePair mapping = ChangeCauseMapping[deletion_cause];

    if (mapping.notify)
      delegate_->OnCookieChanged(*cc, true, mapping.cause);
  }
  cookies_.erase(it);
  delete cc;
}

// Domain expiry behavior is unchanged by key/expiry scheme (the
// meaning of the key is different, but that's not visible to this routine).
int CookieMonster::GarbageCollect(const Time& current,
                                  const std::string& key) {
  lock_.AssertAcquired();

  int num_deleted = 0;
  Time safe_date(
      Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays));

  // Collect garbage for this key, minding cookie priorities.
  if (cookies_.count(key) > kDomainMaxCookies) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() key: " << key;

    CookieItVector cookie_its;
    num_deleted += GarbageCollectExpired(
        current, cookies_.equal_range(key), &cookie_its);
    if (cookie_its.size() > kDomainMaxCookies) {
      VLOG(kVlogGarbageCollection) << "Deep Garbage Collect domain.";
      size_t purge_goal =
          cookie_its.size() - (kDomainMaxCookies - kDomainPurgeCookies);
      DCHECK(purge_goal > kDomainPurgeCookies);

      // Boundary iterators into |cookie_its| for different priorities.
      CookieItVector::iterator it_bdd[4];
      // Intialize |it_bdd| while sorting |cookie_its| by priorities.
      // Schematic: [MLLHMHHLMM] => [LLL|MMMM|HHH], with 4 boundaries.
      it_bdd[0] = cookie_its.begin();
      it_bdd[3] = cookie_its.end();
      it_bdd[1] = PartitionCookieByPriority(it_bdd[0], it_bdd[3],
                                            COOKIE_PRIORITY_LOW);
      it_bdd[2] = PartitionCookieByPriority(it_bdd[1], it_bdd[3],
                                            COOKIE_PRIORITY_MEDIUM);
      size_t quota[3] = {
        kDomainCookiesQuotaLow,
        kDomainCookiesQuotaMedium,
        kDomainCookiesQuotaHigh
      };

      // Purge domain cookies in 3 rounds.
      // Round 1: consider low-priority cookies only: evict least-recently
      //   accessed, while protecting quota[0] of these from deletion.
      // Round 2: consider {low, medium}-priority cookies, evict least-recently
      //   accessed, while protecting quota[0] + quota[1].
      // Round 3: consider all cookies, evict least-recently accessed.
      size_t accumulated_quota = 0;
      CookieItVector::iterator it_purge_begin = it_bdd[0];
      for (int i = 0; i < 3 && purge_goal > 0; ++i) {
        accumulated_quota += quota[i];

        size_t num_considered = it_bdd[i + 1] - it_purge_begin;
        if (num_considered <= accumulated_quota)
          continue;

        // Number of cookies that will be purged in this round.
        size_t round_goal =
            std::min(purge_goal, num_considered - accumulated_quota);
        purge_goal -= round_goal;

        SortLeastRecentlyAccessed(it_purge_begin, it_bdd[i + 1], round_goal);
        // Cookies accessed on or after |safe_date| would have been safe from
        // global purge, and we want to keep track of this.
        CookieItVector::iterator it_purge_end = it_purge_begin + round_goal;
        CookieItVector::iterator it_purge_middle =
            LowerBoundAccessDate(it_purge_begin, it_purge_end, safe_date);
        // Delete cookies accessed before |safe_date|.
        num_deleted += GarbageCollectDeleteRange(
            current,
            DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE,
            it_purge_begin,
            it_purge_middle);
        // Delete cookies accessed on or after |safe_date|.
        num_deleted += GarbageCollectDeleteRange(
            current,
            DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE,
            it_purge_middle,
            it_purge_end);
        it_purge_begin = it_purge_end;
      }
      DCHECK_EQ(0U, purge_goal);
    }
  }

  // Collect garbage for everything. With firefox style we want to preserve
  // cookies accessed in kSafeFromGlobalPurgeDays, otherwise evict.
  if (cookies_.size() > kMaxCookies &&
      earliest_access_time_ < safe_date) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() everything";
    CookieItVector cookie_its;
    num_deleted += GarbageCollectExpired(
        current, CookieMapItPair(cookies_.begin(), cookies_.end()),
        &cookie_its);
    if (cookie_its.size() > kMaxCookies) {
      VLOG(kVlogGarbageCollection) << "Deep Garbage Collect everything.";
      size_t purge_goal = cookie_its.size() - (kMaxCookies - kPurgeCookies);
      DCHECK(purge_goal > kPurgeCookies);
      // Sorts up to *and including* |cookie_its[purge_goal]|, so
      // |earliest_access_time| will be properly assigned even if
      // |global_purge_it| == |cookie_its.begin() + purge_goal|.
      SortLeastRecentlyAccessed(cookie_its.begin(), cookie_its.end(),
                                purge_goal);
      // Find boundary to cookies older than safe_date.
      CookieItVector::iterator global_purge_it =
          LowerBoundAccessDate(cookie_its.begin(),
                               cookie_its.begin() + purge_goal,
                               safe_date);
      // Only delete the old cookies.
      num_deleted += GarbageCollectDeleteRange(
          current,
          DELETE_COOKIE_EVICTED_GLOBAL,
          cookie_its.begin(),
          global_purge_it);
      // Set access day to the oldest cookie that wasn't deleted.
      earliest_access_time_ = (*global_purge_it)->second->LastAccessDate();
    }
  }

  return num_deleted;
}

int CookieMonster::GarbageCollectExpired(
    const Time& current,
    const CookieMapItPair& itpair,
    CookieItVector* cookie_its) {
  if (keep_expired_cookies_)
    return 0;

  lock_.AssertAcquired();

  int num_deleted = 0;
  for (CookieMap::iterator it = itpair.first, end = itpair.second; it != end;) {
    CookieMap::iterator curit = it;
    ++it;

    if (curit->second->IsExpired(current)) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    } else if (cookie_its) {
      cookie_its->push_back(curit);
    }
  }

  return num_deleted;
}

int CookieMonster::GarbageCollectDeleteRange(
    const Time& current,
    DeletionCause cause,
    CookieItVector::iterator it_begin,
    CookieItVector::iterator it_end) {
  for (CookieItVector::iterator it = it_begin; it != it_end; it++) {
    histogram_evicted_last_access_minutes_->Add(
        (current - (*it)->second->LastAccessDate()).InMinutes());
    InternalDeleteCookie((*it), true, cause);
  }
  return it_end - it_begin;
}

// A wrapper around registry_controlled_domains::GetDomainAndRegistry
// to make clear we're creating a key for our local map.  Here and
// in FindCookiesForHostAndDomain() are the only two places where
// we need to conditionalize based on key type.
//
// Note that this key algorithm explicitly ignores the scheme.  This is
// because when we're entering cookies into the map from the backing store,
// we in general won't have the scheme at that point.
// In practical terms, this means that file cookies will be stored
// in the map either by an empty string or by UNC name (and will be
// limited by kMaxCookiesPerHost), and extension cookies will be stored
// based on the single extension id, as the extension id won't have the
// form of a DNS host and hence GetKey() will return it unchanged.
//
// Arguably the right thing to do here is to make the key
// algorithm dependent on the scheme, and make sure that the scheme is
// available everywhere the key must be obtained (specfically at backing
// store load time).  This would require either changing the backing store
// database schema to include the scheme (far more trouble than it's worth), or
// separating out file cookies into their own CookieMonster instance and
// thus restricting each scheme to a single cookie monster (which might
// be worth it, but is still too much trouble to solve what is currently a
// non-problem).
std::string CookieMonster::GetKey(const std::string& domain) const {
  std::string effective_domain(
      registry_controlled_domains::GetDomainAndRegistry(
          domain, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  if (effective_domain.empty())
    effective_domain = domain;

  if (!effective_domain.empty() && effective_domain[0] == '.')
    return effective_domain.substr(1);
  return effective_domain;
}

bool CookieMonster::IsCookieableScheme(const std::string& scheme) {
  base::AutoLock autolock(lock_);

  return std::find(cookieable_schemes_.begin(), cookieable_schemes_.end(),
                   scheme) != cookieable_schemes_.end();
}

bool CookieMonster::HasCookieableScheme(const GURL& url) {
  lock_.AssertAcquired();

  // Make sure the request is on a cookie-able url scheme.
  for (size_t i = 0; i < cookieable_schemes_.size(); ++i) {
    // We matched a scheme.
    if (url.SchemeIs(cookieable_schemes_[i].c_str())) {
      // We've matched a supported scheme.
      return true;
    }
  }

  // The scheme didn't match any in our whitelist.
  VLOG(kVlogPerCookieMonster) << "WARNING: Unsupported cookie scheme: "
                              << url.scheme();
  return false;
}

// Test to see if stats should be recorded, and record them if so.
// The goal here is to get sampling for the average browser-hour of
// activity.  We won't take samples when the web isn't being surfed,
// and when the web is being surfed, we'll take samples about every
// kRecordStatisticsIntervalSeconds.
// last_statistic_record_time_ is initialized to Now() rather than null
// in the constructor so that we won't take statistics right after
// startup, to avoid bias from browsers that are started but not used.
void CookieMonster::RecordPeriodicStats(const base::Time& current_time) {
  const base::TimeDelta kRecordStatisticsIntervalTime(
      base::TimeDelta::FromSeconds(kRecordStatisticsIntervalSeconds));

  // If we've taken statistics recently, return.
  if (current_time - last_statistic_record_time_ <=
      kRecordStatisticsIntervalTime) {
    return;
  }

  // See InitializeHistograms() for details.
  histogram_count_->Add(cookies_.size());

  // More detailed statistics on cookie counts at different granularities.
  TimeTicks beginning_of_time(TimeTicks::Now());

  for (CookieMap::const_iterator it_key = cookies_.begin();
       it_key != cookies_.end(); ) {
    const std::string& key(it_key->first);

    int key_count = 0;
    typedef std::map<std::string, unsigned int> DomainMap;
    DomainMap domain_map;
    CookieMapItPair its_cookies = cookies_.equal_range(key);
    while (its_cookies.first != its_cookies.second) {
      key_count++;
      const std::string& cookie_domain(its_cookies.first->second->Domain());
      domain_map[cookie_domain]++;

      its_cookies.first++;
    }
    histogram_etldp1_count_->Add(key_count);
    histogram_domain_per_etldp1_count_->Add(domain_map.size());
    for (DomainMap::const_iterator domain_map_it = domain_map.begin();
         domain_map_it != domain_map.end(); domain_map_it++)
      histogram_domain_count_->Add(domain_map_it->second);

    it_key = its_cookies.second;
  }

  VLOG(kVlogPeriodic)
      << "Time for recording cookie stats (us): "
      << (TimeTicks::Now() - beginning_of_time).InMicroseconds();

  last_statistic_record_time_ = current_time;
}

// Initialize all histogram counter variables used in this class.
//
// Normal histogram usage involves using the macros defined in
// histogram.h, which automatically takes care of declaring these
// variables (as statics), initializing them, and accumulating into
// them, all from a single entry point.  Unfortunately, that solution
// doesn't work for the CookieMonster, as it's vulnerable to races between
// separate threads executing the same functions and hence initializing the
// same static variables.  There isn't a race danger in the histogram
// accumulation calls; they are written to be resilient to simultaneous
// calls from multiple threads.
//
// The solution taken here is to have per-CookieMonster instance
// variables that are constructed during CookieMonster construction.
// Note that these variables refer to the same underlying histogram,
// so we still race (but safely) with other CookieMonster instances
// for accumulation.
//
// To do this we've expanded out the individual histogram macros calls,
// with declarations of the variables in the class decl, initialization here
// (done from the class constructor) and direct calls to the accumulation
// methods where needed.  The specific histogram macro calls on which the
// initialization is based are included in comments below.
void CookieMonster::InitializeHistograms() {
  // From UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_expiration_duration_minutes_ = base::Histogram::FactoryGet(
      "Cookie.ExpirationDurationMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_between_access_interval_minutes_ = base::Histogram::FactoryGet(
      "Cookie.BetweenAccessIntervalMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_evicted_last_access_minutes_ = base::Histogram::FactoryGet(
      "Cookie.EvictedLastAccessMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_count_ = base::Histogram::FactoryGet(
      "Cookie.Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_domain_count_ = base::Histogram::FactoryGet(
      "Cookie.DomainCount", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_etldp1_count_ = base::Histogram::FactoryGet(
      "Cookie.Etldp1Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_domain_per_etldp1_count_ = base::Histogram::FactoryGet(
      "Cookie.DomainPerEtldp1Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_COUNTS_10000 & UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_number_duplicate_db_cookies_ = base::Histogram::FactoryGet(
      "Net.NumDuplicateCookiesInDb", 1, 10000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_ENUMERATION
  histogram_cookie_deletion_cause_ = base::LinearHistogram::FactoryGet(
      "Cookie.DeletionCause", 1,
      DELETE_COOKIE_LAST_ENTRY - 1, DELETE_COOKIE_LAST_ENTRY,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_{CUSTOM_,}TIMES
  histogram_time_get_ = base::Histogram::FactoryTimeGet("Cookie.TimeGet",
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
      50, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_time_blocked_on_load_ = base::Histogram::FactoryTimeGet(
      "Cookie.TimeBlockedOnLoad",
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
      50, base::Histogram::kUmaTargetedHistogramFlag);
}


// The system resolution is not high enough, so we can have multiple
// set cookies that result in the same system time.  When this happens, we
// increment by one Time unit.  Let's hope computers don't get too fast.
Time CookieMonster::CurrentTime() {
  return std::max(Time::Now(),
      Time::FromInternalValue(last_time_seen_.ToInternalValue() + 1));
}

bool CookieMonster::CopyCookiesForKeyToOtherCookieMonster(
    std::string key,
    CookieMonster* other) {
  ScopedVector<CanonicalCookie> duplicated_cookies;

  {
    base::AutoLock autolock(lock_);
    DCHECK(other);
    if (!loaded_)
      return false;

    for (CookieMapItPair its = cookies_.equal_range(key);
         its.first != its.second;
         ++its.first) {
      CookieMap::iterator curit = its.first;
      CanonicalCookie* cc = curit->second;

      duplicated_cookies.push_back(cc->Duplicate());
    }
  }

  {
    base::AutoLock autolock(other->lock_);
    if (!other->loaded_)
      return false;

    // There must not exist any entries for the key to be copied in |other|.
    CookieMapItPair its = other->cookies_.equal_range(key);
    if (its.first != its.second)
      return false;

    // Store the copied cookies in |other|.
    for (ScopedVector<CanonicalCookie>::const_iterator it =
             duplicated_cookies.begin();
         it != duplicated_cookies.end();
         ++it) {
      other->InternalInsertCookie(key, *it, true);
    }

    // Since the cookies are owned by |other| now, weak clear must be used.
    duplicated_cookies.weak_clear();
  }

  return true;
}

bool CookieMonster::loaded() {
  base::AutoLock autolock(lock_);
  return loaded_;
}

}  // namespace net
