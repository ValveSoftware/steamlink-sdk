// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_CANONICAL_COOKIE_H_
#define NET_COOKIES_CANONICAL_COOKIE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"

class GURL;

namespace net {

class ParsedCookie;

class NET_EXPORT CanonicalCookie {
 public:
  // These constructors do no validation or canonicalization of their inputs;
  // the resulting CanonicalCookies should not be relied on to be canonical
  // unless the caller has done appropriate validation and canonicalization
  // themselves.
  CanonicalCookie();
  CanonicalCookie(const GURL& url,
                  const std::string& name,
                  const std::string& value,
                  const std::string& domain,
                  const std::string& path,
                  const base::Time& creation,
                  const base::Time& expiration,
                  const base::Time& last_access,
                  bool secure,
                  bool httponly,
                  CookiePriority priority);

  // This constructor does canonicalization but not validation.
  // The result of this constructor should not be relied on in contexts
  // in which pre-validation of the ParsedCookie has not been done.
  CanonicalCookie(const GURL& url, const ParsedCookie& pc);

  ~CanonicalCookie();

  // Supports the default copy constructor.

  // Creates a new |CanonicalCookie| from the |cookie_line| and the
  // |creation_time|. Canonicalizes and validates inputs. May return NULL if
  // an attribut value is invalid.
  static CanonicalCookie* Create(const GURL& url,
                                 const std::string& cookie_line,
                                 const base::Time& creation_time,
                                 const CookieOptions& options);

  // Creates a canonical cookie from unparsed attribute values.
  // Canonicalizes and validates inputs.  May return NULL if an attribute
  // value is invalid.
  static CanonicalCookie* Create(const GURL& url,
                                 const std::string& name,
                                 const std::string& value,
                                 const std::string& domain,
                                 const std::string& path,
                                 const base::Time& creation,
                                 const base::Time& expiration,
                                 bool secure,
                                 bool http_only,
                                 CookiePriority priority);

  const std::string& Source() const { return source_; }
  const std::string& Name() const { return name_; }
  const std::string& Value() const { return value_; }
  const std::string& Domain() const { return domain_; }
  const std::string& Path() const { return path_; }
  const base::Time& CreationDate() const { return creation_date_; }
  const base::Time& LastAccessDate() const { return last_access_date_; }
  bool IsPersistent() const { return !expiry_date_.is_null(); }
  const base::Time& ExpiryDate() const { return expiry_date_; }
  bool IsSecure() const { return secure_; }
  bool IsHttpOnly() const { return httponly_; }
  CookiePriority Priority() const { return priority_; }
  bool IsDomainCookie() const {
    return !domain_.empty() && domain_[0] == '.'; }
  bool IsHostCookie() const { return !IsDomainCookie(); }

  bool IsExpired(const base::Time& current) const {
    return !expiry_date_.is_null() && current >= expiry_date_;
  }

  // Are the cookies considered equivalent in the eyes of RFC 2965.
  // The RFC says that name must match (case-sensitive), domain must
  // match (case insensitive), and path must match (case sensitive).
  // For the case insensitive domain compare, we rely on the domain
  // having been canonicalized (in
  // GetCookieDomainWithString->CanonicalizeHost).
  bool IsEquivalent(const CanonicalCookie& ecc) const {
    // It seems like it would make sense to take secure and httponly into
    // account, but the RFC doesn't specify this.
    // NOTE: Keep this logic in-sync with TrimDuplicateCookiesForHost().
    return (name_ == ecc.Name() && domain_ == ecc.Domain()
            && path_ == ecc.Path());
  }

  void SetLastAccessDate(const base::Time& date) {
    last_access_date_ = date;
  }

  // Returns true if the given |url_path| path-matches the cookie-path as
  // described in section 5.1.4 in RFC 6265.
  bool IsOnPath(const std::string& url_path) const;

  // Returns true if the cookie domain matches the given |host| as described in
  // section 5.1.3 of RFC 6265.
  bool IsDomainMatch(const std::string& host) const;

  // Returns true if the cookie should be included for the given request |url|.
  // HTTP only cookies can be filter by using appropriate cookie |options|.
  // PLEASE NOTE that this method does not check whether a cookie is expired or
  // not!
  bool IncludeForRequestURL(const GURL& url,
                            const CookieOptions& options) const;

  std::string DebugString() const;

  // Returns a duplicate of this cookie.
  CanonicalCookie* Duplicate();

  // Returns the cookie source when cookies are set for |url|. This function
  // is public for unit test purposes only.
  static std::string GetCookieSourceFromURL(const GURL& url);
  static std::string CanonPath(const GURL& url, const ParsedCookie& pc);
  static base::Time CanonExpiration(const ParsedCookie& pc,
                                    const base::Time& current,
                                    const base::Time& server_time);

 private:
  // NOTE: When any new members are added below, the implementation of
  // Duplicate() must be updated to copy the new member accordingly.

  // The source member of a canonical cookie is the origin of the URL that tried
  // to set this cookie, minus the port number if any.  This field is not
  // persistent though; its only used in the in-tab cookies dialog to show the
  // user the source URL. This is used for both allowed and blocked cookies.
  // When a CanonicalCookie is constructed from the backing store (common case)
  // this field will be null.  CanonicalCookie consumers should not rely on
  // this field unless they guarantee that the creator of those
  // CanonicalCookies properly initialized the field.
  std::string source_;
  std::string name_;
  std::string value_;
  std::string domain_;
  std::string path_;
  base::Time creation_date_;
  base::Time expiry_date_;
  base::Time last_access_date_;
  bool secure_;
  bool httponly_;
  CookiePriority priority_;
  // NOTE: When any new members are added above this comment, the
  // implementation of Duplicate() must be updated to copy the new member
  // accordingly.
};

typedef std::vector<CanonicalCookie> CookieList;

}  // namespace net

#endif  // NET_COOKIES_CANONICAL_COOKIE_H_
