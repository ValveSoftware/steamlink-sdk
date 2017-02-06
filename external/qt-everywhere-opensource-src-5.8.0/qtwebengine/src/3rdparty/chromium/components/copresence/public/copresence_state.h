// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_STATE_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_STATE_H_

#include <map>
#include <string>
#include <vector>

namespace copresence {

class CopresenceObserver;
class Directive;
struct TransmittedToken;
struct ReceivedToken;

// This class tracks the state of the copresence component and can notify
// interested parties when it changes. This is useful for debugging purposes.
class CopresenceState {
 public:
  // Add a listener for future state changes. |observer| is owned by the caller,
  // and must be valid until removed (or the CopresenceState is destroyed).
  virtual void AddObserver(CopresenceObserver* observer) = 0;

  // Remove a state change listener.
  virtual void RemoveObserver(CopresenceObserver* observer) = 0;

  // Accessor for the currently active directives.
  virtual const std::vector<Directive>& active_directives() const = 0;

  // Accessor for recently transmitted tokens.
  virtual const std::map<std::string, TransmittedToken>&
  transmitted_tokens() const = 0;

  // Accessor for recently received tokens.
  virtual const std::map<std::string, ReceivedToken>&
  received_tokens() const = 0;

 protected:
  virtual ~CopresenceState() {}
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_STATE_H_
