// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_persister.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"

using net::HashValue;
using net::HashValueTag;
using net::HashValueVector;
using net::TransportSecurityState;

namespace {

base::ListValue* SPKIHashesToListValue(const HashValueVector& hashes) {
  base::ListValue* pins = new base::ListValue;
  for (size_t i = 0; i != hashes.size(); i++)
    pins->Append(new base::StringValue(hashes[i].ToString()));
  return pins;
}

void SPKIHashesFromListValue(const base::ListValue& pins,
                             HashValueVector* hashes) {
  size_t num_pins = pins.GetSize();
  for (size_t i = 0; i < num_pins; ++i) {
    std::string type_and_base64;
    HashValue fingerprint;
    if (pins.GetString(i, &type_and_base64) &&
        fingerprint.FromString(type_and_base64)) {
      hashes->push_back(fingerprint);
    }
  }
}

// This function converts the binary hashes to a base64 string which we can
// include in a JSON file.
std::string HashedDomainToExternalString(const std::string& hashed) {
  std::string out;
  base::Base64Encode(hashed, &out);
  return out;
}

// This inverts |HashedDomainToExternalString|, above. It turns an external
// string (from a JSON file) into an internal (binary) string.
std::string ExternalStringToHashedDomain(const std::string& external) {
  std::string out;
  if (!base::Base64Decode(external, &out) ||
      out.size() != crypto::kSHA256Length) {
    return std::string();
  }

  return out;
}

const char kIncludeSubdomains[] = "include_subdomains";
const char kStsIncludeSubdomains[] = "sts_include_subdomains";
const char kPkpIncludeSubdomains[] = "pkp_include_subdomains";
const char kMode[] = "mode";
const char kExpiry[] = "expiry";
const char kDynamicSPKIHashesExpiry[] = "dynamic_spki_hashes_expiry";
const char kDynamicSPKIHashes[] = "dynamic_spki_hashes";
const char kForceHTTPS[] = "force-https";
const char kStrict[] = "strict";
const char kDefault[] = "default";
const char kPinningOnly[] = "pinning-only";
const char kCreated[] = "created";
const char kStsObserved[] = "sts_observed";
const char kPkpObserved[] = "pkp_observed";

std::string LoadState(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    return "";
  }
  return result;
}

}  // namespace


namespace net {

TransportSecurityPersister::TransportSecurityPersister(
    TransportSecurityState* state,
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* background_runner,
    bool readonly)
    : transport_security_state_(state),
      writer_(profile_path.AppendASCII("TransportSecurity"), background_runner),
      foreground_runner_(base::MessageLoop::current()->message_loop_proxy()),
      background_runner_(background_runner),
      readonly_(readonly),
      weak_ptr_factory_(this) {
  transport_security_state_->SetDelegate(this);

  base::PostTaskAndReplyWithResult(
      background_runner_,
      FROM_HERE,
      base::Bind(&::LoadState, writer_.path()),
      base::Bind(&TransportSecurityPersister::CompleteLoad,
                 weak_ptr_factory_.GetWeakPtr()));
}

TransportSecurityPersister::~TransportSecurityPersister() {
  DCHECK(foreground_runner_->RunsTasksOnCurrentThread());

  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();

  transport_security_state_->SetDelegate(NULL);
}

void TransportSecurityPersister::StateIsDirty(
    TransportSecurityState* state) {
  DCHECK(foreground_runner_->RunsTasksOnCurrentThread());
  DCHECK_EQ(transport_security_state_, state);

  if (!readonly_)
    writer_.ScheduleWrite(this);
}

bool TransportSecurityPersister::SerializeData(std::string* output) {
  DCHECK(foreground_runner_->RunsTasksOnCurrentThread());

  base::DictionaryValue toplevel;
  base::Time now = base::Time::Now();
  TransportSecurityState::Iterator state(*transport_security_state_);
  for (; state.HasNext(); state.Advance()) {
    const std::string& hostname = state.hostname();
    const TransportSecurityState::DomainState& domain_state =
        state.domain_state();

    base::DictionaryValue* serialized = new base::DictionaryValue;
    serialized->SetBoolean(kStsIncludeSubdomains,
                           domain_state.sts.include_subdomains);
    serialized->SetBoolean(kPkpIncludeSubdomains,
                           domain_state.pkp.include_subdomains);
    serialized->SetDouble(kStsObserved,
                          domain_state.sts.last_observed.ToDoubleT());
    serialized->SetDouble(kPkpObserved,
                          domain_state.pkp.last_observed.ToDoubleT());
    serialized->SetDouble(kExpiry, domain_state.sts.expiry.ToDoubleT());
    serialized->SetDouble(kDynamicSPKIHashesExpiry,
                          domain_state.pkp.expiry.ToDoubleT());

    switch (domain_state.sts.upgrade_mode) {
      case TransportSecurityState::DomainState::MODE_FORCE_HTTPS:
        serialized->SetString(kMode, kForceHTTPS);
        break;
      case TransportSecurityState::DomainState::MODE_DEFAULT:
        serialized->SetString(kMode, kDefault);
        break;
      default:
        NOTREACHED() << "DomainState with unknown mode";
        delete serialized;
        continue;
    }

    if (now < domain_state.pkp.expiry) {
      serialized->Set(kDynamicSPKIHashes,
                      SPKIHashesToListValue(domain_state.pkp.spki_hashes));
    }

    toplevel.Set(HashedDomainToExternalString(hostname), serialized);
  }

  base::JSONWriter::WriteWithOptions(&toplevel,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     output);
  return true;
}

bool TransportSecurityPersister::LoadEntries(const std::string& serialized,
                                             bool* dirty) {
  DCHECK(foreground_runner_->RunsTasksOnCurrentThread());

  transport_security_state_->ClearDynamicData();
  return Deserialize(serialized, dirty, transport_security_state_);
}

// static
bool TransportSecurityPersister::Deserialize(const std::string& serialized,
                                             bool* dirty,
                                             TransportSecurityState* state) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(serialized));
  base::DictionaryValue* dict_value = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict_value))
    return false;

  const base::Time current_time(base::Time::Now());
  bool dirtied = false;

  for (base::DictionaryValue::Iterator i(*dict_value);
       !i.IsAtEnd(); i.Advance()) {
    const base::DictionaryValue* parsed = NULL;
    if (!i.value().GetAsDictionary(&parsed)) {
      LOG(WARNING) << "Could not parse entry " << i.key() << "; skipping entry";
      continue;
    }

    std::string mode_string;
    double expiry;
    double dynamic_spki_hashes_expiry = 0.0;
    TransportSecurityState::DomainState domain_state;

    // kIncludeSubdomains is a legacy synonym for kStsIncludeSubdomains and
    // kPkpIncludeSubdomains. Parse at least one of these properties,
    // preferably the new ones.
    bool include_subdomains = false;
    bool parsed_include_subdomains = parsed->GetBoolean(kIncludeSubdomains,
                                                        &include_subdomains);
    domain_state.sts.include_subdomains = include_subdomains;
    domain_state.pkp.include_subdomains = include_subdomains;
    if (parsed->GetBoolean(kStsIncludeSubdomains, &include_subdomains)) {
      domain_state.sts.include_subdomains = include_subdomains;
      parsed_include_subdomains = true;
    }
    if (parsed->GetBoolean(kPkpIncludeSubdomains, &include_subdomains)) {
      domain_state.pkp.include_subdomains = include_subdomains;
      parsed_include_subdomains = true;
    }

    if (!parsed_include_subdomains ||
        !parsed->GetString(kMode, &mode_string) ||
        !parsed->GetDouble(kExpiry, &expiry)) {
      LOG(WARNING) << "Could not parse some elements of entry " << i.key()
                   << "; skipping entry";
      continue;
    }

    // Don't fail if this key is not present.
    parsed->GetDouble(kDynamicSPKIHashesExpiry,
                      &dynamic_spki_hashes_expiry);

    const base::ListValue* pins_list = NULL;
    if (parsed->GetList(kDynamicSPKIHashes, &pins_list)) {
      SPKIHashesFromListValue(*pins_list, &domain_state.pkp.spki_hashes);
    }

    if (mode_string == kForceHTTPS || mode_string == kStrict) {
      domain_state.sts.upgrade_mode =
          TransportSecurityState::DomainState::MODE_FORCE_HTTPS;
    } else if (mode_string == kDefault || mode_string == kPinningOnly) {
      domain_state.sts.upgrade_mode =
          TransportSecurityState::DomainState::MODE_DEFAULT;
    } else {
      LOG(WARNING) << "Unknown TransportSecurityState mode string "
                   << mode_string << " found for entry " << i.key()
                   << "; skipping entry";
      continue;
    }

    domain_state.sts.expiry = base::Time::FromDoubleT(expiry);
    domain_state.pkp.expiry =
        base::Time::FromDoubleT(dynamic_spki_hashes_expiry);

    double sts_observed;
    double pkp_observed;
    if (parsed->GetDouble(kStsObserved, &sts_observed)) {
      domain_state.sts.last_observed = base::Time::FromDoubleT(sts_observed);
    } else if (parsed->GetDouble(kCreated, &sts_observed)) {
      // kCreated is a legacy synonym for both kStsObserved and kPkpObserved.
      domain_state.sts.last_observed = base::Time::FromDoubleT(sts_observed);
    } else {
      // We're migrating an old entry with no observation date. Make sure we
      // write the new date back in a reasonable time frame.
      dirtied = true;
      domain_state.sts.last_observed = base::Time::Now();
    }
    if (parsed->GetDouble(kPkpObserved, &pkp_observed)) {
      domain_state.pkp.last_observed = base::Time::FromDoubleT(pkp_observed);
    } else if (parsed->GetDouble(kCreated, &pkp_observed)) {
      domain_state.pkp.last_observed = base::Time::FromDoubleT(pkp_observed);
    } else {
      dirtied = true;
      domain_state.pkp.last_observed = base::Time::Now();
    }

    if (domain_state.sts.expiry <= current_time &&
        domain_state.pkp.expiry <= current_time) {
      // Make sure we dirty the state if we drop an entry.
      dirtied = true;
      continue;
    }

    std::string hashed = ExternalStringToHashedDomain(i.key());
    if (hashed.empty()) {
      dirtied = true;
      continue;
    }

    state->AddOrUpdateEnabledHosts(hashed, domain_state);
  }

  *dirty = dirtied;
  return true;
}

void TransportSecurityPersister::CompleteLoad(const std::string& state) {
  DCHECK(foreground_runner_->RunsTasksOnCurrentThread());

  if (state.empty())
    return;

  bool dirty = false;
  if (!LoadEntries(state, &dirty)) {
    LOG(ERROR) << "Failed to deserialize state: " << state;
    return;
  }
  if (dirty)
    StateIsDirty(transport_security_state_);
}

}  // namespace net
