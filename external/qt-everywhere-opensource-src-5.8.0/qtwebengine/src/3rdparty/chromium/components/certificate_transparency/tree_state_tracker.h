// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_TREE_STATE_TRACKER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_TREE_STATE_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/sth_observer.h"

namespace net {
class CTLogVerifier;
class X509Certificate;

namespace ct {
struct SignedCertificateTimestamp;
struct SignedTreeHead;
}  // namespace ct

}  // namespace net

namespace certificate_transparency {
class SingleTreeTracker;

// This class receives notifications of new Signed Tree Heads (STHs) and
// verified Signed Certificate Timestamps (SCTs) and delegates them to
// the SingleTreeTracker tracking the CT log they relate to.
// TODO(eranm): Export the inclusion check status of SCTs+Certs so it can
// be used in the DevTools Security panel, for example. See
// https://crbug.com/506227#c22.
class TreeStateTracker : public net::CTVerifier::Observer,
                         public net::ct::STHObserver {
 public:
  // Tracks the state of the logs provided in |ct_logs|. An instance of this
  // class only tracks the logs provided in the constructor. The implementation
  // is based on the assumption that the list of recognized logs does not change
  // during the object's life time.
  // Observed STHs from logs not in this list will be simply ignored.
  explicit TreeStateTracker(
      std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs);
  ~TreeStateTracker() override;

  // net::ct::CTVerifier::Observer implementation.
  // Delegates to the tree tracker corresponding to the log that issued the SCT.
  void OnSCTVerified(net::X509Certificate* cert,
                     const net::ct::SignedCertificateTimestamp* sct) override;

  // net::ct::STHObserver implementation.
  // Delegates to the tree tracker corresponding to the log that issued the STH.
  void NewSTHObserved(const net::ct::SignedTreeHead& sth) override;

 private:
  // Holds the SingleTreeTracker for each log
  std::map<std::string, std::unique_ptr<SingleTreeTracker>> tree_trackers_;

  DISALLOW_COPY_AND_ASSIGN(TreeStateTracker);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_TREE_STATE_TRACKER_H_
