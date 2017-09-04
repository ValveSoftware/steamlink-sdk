// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/ct_policy_manager.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_matcher/url_matcher.h"
#include "net/base/host_port_pair.h"

namespace certificate_transparency {

class CTPolicyManager::CTDelegate
    : public net::TransportSecurityState::RequireCTDelegate {
 public:
  explicit CTDelegate(
      scoped_refptr<base::SequencedTaskRunner> network_task_runner);
  ~CTDelegate() override = default;

  // Called on the prefs task runner. Updates the CTDelegate to require CT
  // for |required_hosts|, and exclude |excluded_hosts| from CT policies.
  void UpdateFromPrefs(const base::ListValue* required_hosts,
                       const base::ListValue* excluded_hosts);

  // RequireCTDelegate implementation
  // Called on the network task runner.
  CTRequirementLevel IsCTRequiredForHost(const std::string& hostname) override;

 private:
  struct Filter {
    bool ct_required = false;
    bool match_subdomains = false;
    size_t host_length = 0;
  };

  // Called on the |network_task_runner_|, updates the |url_matcher_| to
  // require CT for |required_hosts| and exclude |excluded_hosts|, both
  // of which are Lists of Strings which are URLBlacklist filters.
  void Update(base::ListValue* required_hosts, base::ListValue* excluded_hosts);

  // Parses the filters from |host_patterns|, adding them as filters to
  // |filters_| (with |ct_required| indicating whether or not CT is required
  // for that host), and updating |*conditions| with the corresponding
  // URLMatcher::Conditions to match the host.
  void AddFilters(bool ct_required,
                  base::ListValue* host_patterns,
                  url_matcher::URLMatcherConditionSet::Vector* conditions);

  // Returns true if |lhs| has greater precedence than |rhs|.
  bool FilterTakesPrecedence(const Filter& lhs, const Filter& rhs) const;

  scoped_refptr<base::SequencedTaskRunner> network_task_runner_;
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
  url_matcher::URLMatcherConditionSet::ID next_id_;
  std::map<url_matcher::URLMatcherConditionSet::ID, Filter> filters_;

  DISALLOW_COPY_AND_ASSIGN(CTDelegate);
};

CTPolicyManager::CTDelegate::CTDelegate(
    scoped_refptr<base::SequencedTaskRunner> network_task_runner)
    : network_task_runner_(std::move(network_task_runner)),
      url_matcher_(new url_matcher::URLMatcher),
      next_id_(0) {}

void CTPolicyManager::CTDelegate::UpdateFromPrefs(
    const base::ListValue* required_hosts,
    const base::ListValue* excluded_hosts) {
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CTDelegate::Update, base::Unretained(this),
                 base::Owned(required_hosts->CreateDeepCopy().release()),
                 base::Owned(excluded_hosts->CreateDeepCopy().release())));
}

net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel
CTPolicyManager::CTDelegate::IsCTRequiredForHost(const std::string& hostname) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());

  // Scheme and port are ignored by the policy, so it's OK to construct a
  // new GURL here. However, |hostname| is in network form, not URL form,
  // so it's necessary to wrap IPv6 addresses in brackets.
  std::set<url_matcher::URLMatcherConditionSet::ID> matching_ids =
      url_matcher_->MatchURL(
          GURL("https://" + net::HostPortPair(hostname, 443).HostForURL()));
  if (matching_ids.empty())
    return CTRequirementLevel::DEFAULT;

  // Determine the overall policy by determining the most specific policy.
  std::map<url_matcher::URLMatcherConditionSet::ID, Filter>::const_iterator it =
      filters_.begin();
  const Filter* active_filter = nullptr;
  for (const auto& match : matching_ids) {
    // Because both |filters_| and |matching_ids| are sorted on the ID,
    // treat both as forward-only iterators.
    while (it != filters_.end() && it->first < match)
      ++it;
    if (it == filters_.end()) {
      NOTREACHED();
      break;
    }

    if (!active_filter || FilterTakesPrecedence(it->second, *active_filter))
      active_filter = &it->second;
  }
  CHECK(active_filter);

  return active_filter->ct_required ? CTRequirementLevel::REQUIRED
                                    : CTRequirementLevel::NOT_REQUIRED;
}

void CTPolicyManager::CTDelegate::Update(base::ListValue* required_hosts,
                                         base::ListValue* excluded_hosts) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());

  url_matcher_.reset(new url_matcher::URLMatcher);
  filters_.clear();
  next_id_ = 0;

  url_matcher::URLMatcherConditionSet::Vector all_conditions;
  AddFilters(true, required_hosts, &all_conditions);
  AddFilters(false, excluded_hosts, &all_conditions);

  url_matcher_->AddConditionSets(all_conditions);
}

void CTPolicyManager::CTDelegate::AddFilters(
    bool ct_required,
    base::ListValue* hosts,
    url_matcher::URLMatcherConditionSet::Vector* conditions) {
  for (size_t i = 0; i < hosts->GetSize(); ++i) {
    std::string pattern;
    if (!hosts->GetString(i, &pattern))
      continue;

    Filter filter;
    filter.ct_required = ct_required;

    // Parse the pattern just to the hostname, ignoring all other portions of
    // the URL.
    url::Parsed parsed;
    std::string ignored_scheme = url_formatter::SegmentURL(pattern, &parsed);
    if (!parsed.host.is_nonempty())
      continue;  // If there is no host to match, can't apply the filter.

    std::string lc_host = base::ToLowerASCII(
        base::StringPiece(pattern).substr(parsed.host.begin, parsed.host.len));
    if (lc_host == "*") {
      // Wildcard hosts are not allowed and ignored.
      continue;
    } else if (lc_host[0] == '.') {
      // A leading dot means exact match and to not match subdomains.
      lc_host.erase(0, 1);
      filter.match_subdomains = false;
    } else {
      // Canonicalize the host to make sure it's an actual hostname, not an
      // IP address or a BROKEN canonical host, as matching subdomains is
      // not desirable for those.
      url::RawCanonOutputT<char> output;
      url::CanonHostInfo host_info;
      url::CanonicalizeHostVerbose(pattern.c_str(), parsed.host, &output,
                                   &host_info);
      // TODO(rsleevi): Use canonicalized form?
      if (host_info.family == url::CanonHostInfo::NEUTRAL) {
        // Match subdomains (implicit by the omission of '.'). Add in a
        // leading dot to make sure matches only happen at the domain
        // component boundary.
        lc_host.insert(lc_host.begin(), '.');
        filter.match_subdomains = true;
      } else {
        filter.match_subdomains = false;
      }
    }
    filter.host_length = lc_host.size();

    // Create a condition for the URLMatcher that matches the hostname (and/or
    // subdomains).
    url_matcher::URLMatcherConditionFactory* condition_factory =
        url_matcher_->condition_factory();
    std::set<url_matcher::URLMatcherCondition> condition_set;
    condition_set.insert(
        filter.match_subdomains
            ? condition_factory->CreateHostSuffixCondition(lc_host)
            : condition_factory->CreateHostEqualsCondition(lc_host));
    conditions->push_back(
        new url_matcher::URLMatcherConditionSet(next_id_, condition_set));
    filters_[next_id_] = filter;
    ++next_id_;
  }
}

bool CTPolicyManager::CTDelegate::FilterTakesPrecedence(
    const Filter& lhs,
    const Filter& rhs) const {
  if (lhs.match_subdomains != rhs.match_subdomains)
    return !lhs.match_subdomains;  // Prefer the more explicit policy.

  if (lhs.host_length != rhs.host_length)
    return lhs.host_length > rhs.host_length;  // Prefer the longer host match.

  if (lhs.ct_required != rhs.ct_required)
    return lhs.ct_required;  // Prefer the policy that requires CT.

  return false;
}

// static
void CTPolicyManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kCTRequiredHosts);
  registry->RegisterListPref(prefs::kCTExcludedHosts);
}

CTPolicyManager::CTPolicyManager(
    PrefService* pref_service,
    scoped_refptr<base::SequencedTaskRunner> network_task_runner)
    : delegate_(new CTDelegate(std::move(network_task_runner))),
      weak_factory_(this) {
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kCTRequiredHosts,
      base::Bind(&CTPolicyManager::ScheduleUpdate, base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kCTExcludedHosts,
      base::Bind(&CTPolicyManager::ScheduleUpdate, base::Unretained(this)));

  ScheduleUpdate();
}

CTPolicyManager::~CTPolicyManager() {}

void CTPolicyManager::Shutdown() {
  // Cancel any pending updates, since the preferences are going away.
  weak_factory_.InvalidateWeakPtrs();
  pref_change_registrar_.RemoveAll();
}

net::TransportSecurityState::RequireCTDelegate* CTPolicyManager::GetDelegate() {
  return delegate_.get();
}

void CTPolicyManager::ScheduleUpdate() {
  // Cancel any pending updates, and schedule a new update. If this method
  // is called again, this pending update will be cancelled because the weak
  // pointer is invalidated, and the new update will take precedence.
  weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&CTPolicyManager::Update, weak_factory_.GetWeakPtr()));
}

void CTPolicyManager::Update() {
  delegate_->UpdateFromPrefs(
      pref_change_registrar_.prefs()->GetList(prefs::kCTRequiredHosts),
      pref_change_registrar_.prefs()->GetList(prefs::kCTExcludedHosts));
}

}  // namespace certificate_transparency
