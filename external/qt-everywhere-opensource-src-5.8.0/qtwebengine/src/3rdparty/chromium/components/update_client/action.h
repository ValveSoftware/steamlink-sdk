// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_H_

#include <stddef.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

namespace update_client {

class Configurator;
struct CrxUpdateItem;
struct UpdateContext;

// Any update can be broken down as a sequence of discrete steps, such as
// checking for updates, downloading patches, updating, and waiting between
// successive updates. An action is the smallest unit of work executed by
// the update engine.
//
// Defines an abstract interface for a unit of work, executed by the
// update engine as part of an update.
class Action {
 public:
  enum class ErrorCategory {
    kErrorNone = 0,
    kNetworkError,
    kUnpackError,
    kInstallError,
    kServiceError,  // Runtime errors which occur in the service itself.
  };

  enum class ServiceError {
    ERROR_WAIT = 1,
  };

  using Callback = base::Callback<void(int error)>;
  virtual ~Action() {}

  // Runs the code encapsulated by the action. When an action completes, it can
  // chain up and transfer the execution flow to another action or it can
  // invoke the |callback| when this function has completed and there is nothing
  // else to do.
  virtual void Run(UpdateContext* update_context, Callback callback) = 0;
};

// Provides a reusable implementation of common functions needed by actions.
class ActionImpl {
 protected:
  ActionImpl();
  ~ActionImpl();

  void Run(UpdateContext* update_context, Action::Callback callback);

  // Changes the current state of the |item| to the new state |to|.
  void ChangeItemState(CrxUpdateItem* item, CrxUpdateItem::State to);

  // Changes the state of all items in |update_context_|. Returns the count
  // of items affected by the call.
  size_t ChangeAllItemsState(CrxUpdateItem::State from,
                             CrxUpdateItem::State to);

  // Returns the item associated with the component |id| or nullptr in case
  // of errors.
  CrxUpdateItem* FindUpdateItemById(const std::string& id) const;

  void NotifyObservers(UpdateClient::Observer::Events event,
                       const std::string& id);

  // Updates the CRX at the front of the CRX queue in this update context.
  void UpdateCrx();

  // Completes updating the CRX at the front of the queue, and initiates
  // the update for the next CRX in the queue, if the queue is not empty.
  void UpdateCrxComplete(CrxUpdateItem* item);

  // Called when the updates for all CRXs have finished and the execution
  // flow must return back to the update engine.
  void UpdateComplete(int error);

  base::ThreadChecker thread_checker_;

  UpdateContext* update_context_;  // Not owned by this class.
  Action::Callback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActionImpl);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_H_
