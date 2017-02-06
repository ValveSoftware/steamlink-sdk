// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_OBSERVER_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_OBSERVER_H_

namespace copresence {

class Directive;
struct TransmittedToken;
struct ReceivedToken;

// Implement this interface to be notified of changes
// to the state of the copresence component.
// TODO(ckehoe): Create a LoggingObserver based on this that replaces
// equivalent logging elsewhere. Then we know what state is being recorded.
class CopresenceObserver {
 public:
  // Called when the list of active directives has changed.
  virtual void DirectivesUpdated() = 0;

  // Called when a token is transmitted.
  virtual void TokenTransmitted(const TransmittedToken& token) = 0;

  // Called when a token is received.
  virtual void TokenReceived(const ReceivedToken& token) = 0;

 protected:
  virtual ~CopresenceObserver() {}
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_OBSERVER_H_
