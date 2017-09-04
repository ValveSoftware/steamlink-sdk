// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_CONNECTION_H_
#define BLIMP_NET_BLIMP_CONNECTION_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/connection_error_observer.h"

namespace blimp {

class BlimpMessageProcessor;

// Encapsulates the state and logic used to exchange BlimpMessages over
// a network connection.
class BLIMP_NET_EXPORT BlimpConnection : public ConnectionErrorObserver {
 public:
  ~BlimpConnection() override;

  // Adds |observer| to the connection's error observer list.
  virtual void AddConnectionErrorObserver(ConnectionErrorObserver* observer);

  // Removes |observer| from the connection's error observer list.
  virtual void RemoveConnectionErrorObserver(ConnectionErrorObserver* observer);

  // Sets the processor which will take incoming messages for this connection.
  // Can be set multiple times, but previously set processors are discarded.
  // Caller retains the ownership of |processor|.
  virtual void SetIncomingMessageProcessor(
      BlimpMessageProcessor* processor) = 0;

  // Gets a processor for BrowserSession->BlimpConnection message routing.
  virtual BlimpMessageProcessor* GetOutgoingMessageProcessor() = 0;

 protected:
  class EndConnectionFilter;
  friend class EndConnectionFilter;

  BlimpConnection();

  void AddEndConnectionProcessor(BlimpMessageProcessor* processor);

  // ConnectionErrorObserver implementation.
  void OnConnectionError(int error) override;

  BlimpMessageProcessor* GetEndConnectionProcessor() const;

 private:
  std::unique_ptr<EndConnectionFilter> end_connection_filter_;
  base::ObserverList<ConnectionErrorObserver> error_observers_;
  DISALLOW_COPY_AND_ASSIGN(BlimpConnection);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_CONNECTION_H_
