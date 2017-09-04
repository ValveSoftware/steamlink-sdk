// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

template <typename BindingType>
struct BindingSetTraits;

template <typename Interface>
struct BindingSetTraits<Binding<Interface>> {
  using ProxyType = InterfacePtr<Interface>;
  using RequestType = InterfaceRequest<Interface>;

  static RequestType GetProxy(ProxyType* proxy) {
    return mojo::GetProxy(proxy);
  }
};

enum class BindingSetDispatchMode {
  WITHOUT_CONTEXT,
  WITH_CONTEXT,
};

using BindingId = size_t;

// Use this class to manage a set of bindings, which are automatically destroyed
// and removed from the set when the pipe they are bound to is disconnected.
template <typename Interface, typename BindingType = Binding<Interface>>
class BindingSet {
 public:
  using PreDispatchCallback = base::Callback<void(void*)>;
  using Traits = BindingSetTraits<BindingType>;
  using ProxyType = typename Traits::ProxyType;
  using RequestType = typename Traits::RequestType;

  BindingSet() : BindingSet(BindingSetDispatchMode::WITHOUT_CONTEXT) {}

  // Constructs a new BindingSet operating in |dispatch_mode|. If |WITH_CONTEXT|
  // is used, AddBinding() supports a |context| argument, and dispatch_context()
  // may be called during message or error dispatch to identify which specific
  // binding received the message or error.
  explicit BindingSet(BindingSetDispatchMode dispatch_mode)
      : dispatch_mode_(dispatch_mode) {}

  void set_connection_error_handler(const base::Closure& error_handler) {
    error_handler_ = error_handler;
    error_with_reason_handler_.Reset();
  }

  void set_connection_error_with_reason_handler(
      const ConnectionErrorWithReasonCallback& error_handler) {
    error_with_reason_handler_ = error_handler;
    error_handler_.Reset();
  }

  // Sets a callback to be invoked immediately before dispatching any message or
  // error received by any of the bindings in the set. This may only be used
  // if the set was constructed with |BindingSetDispatchMode::WITH_CONTEXT|.
  // |handler| is passed the context associated with the binding which received
  // the message or event about to be dispatched.
  void set_pre_dispatch_handler(const PreDispatchCallback& handler) {
    DCHECK(SupportsContext());
    pre_dispatch_handler_ = handler;
  }

  // Adds a new binding to the set which binds |request| to |impl|. If |context|
  // is non-null, dispatch_context() will reflect this value during the extent
  // of any message or error dispatch targeting this specific binding. Note that
  // |context| may only be non-null if the BindingSet was constructed with
  // |BindingSetDispatchMode::WITH_CONTEXT|.
  BindingId AddBinding(Interface* impl,
                     RequestType request,
                     void* context = nullptr) {
    DCHECK(!context || SupportsContext());
    BindingId id = next_binding_id_++;
    DCHECK_GE(next_binding_id_, 0u);
    std::unique_ptr<Entry> entry =
        base::MakeUnique<Entry>(impl, std::move(request), this, id, context);
    bindings_.insert(std::make_pair(id, std::move(entry)));
    return id;
  }

  // Removes a binding from the set. Note that this is safe to call even if the
  // binding corresponding to |id| has already been removed.
  //
  // Returns |true| if the binding was removed and |false| if it didn't exist.
  bool RemoveBinding(BindingId id) {
    auto it = bindings_.find(id);
    if (it == bindings_.end())
      return false;
    bindings_.erase(it);
    return true;
  }

  // Returns a proxy bound to one end of a pipe whose other end is bound to
  // |this|. If |id_storage| is not null, |*id_storage| will be set to the ID
  // of the added binding.
  ProxyType CreateInterfacePtrAndBind(Interface* impl,
                                      BindingId* id_storage = nullptr) {
    ProxyType proxy;
    BindingId id = AddBinding(impl, Traits::GetProxy(&proxy));
    if (id_storage)
      *id_storage = id;
    return proxy;
  }

  void CloseAllBindings() { bindings_.clear(); }

  bool empty() const { return bindings_.empty(); }

  // Implementations may call this when processing a dispatched message or
  // error. During the extent of message or error dispatch, this will return the
  // context associated with the specific binding which received the message or
  // error. Use AddBinding() to associated a context with a specific binding.
  //
  // Note that this may ONLY be called if the BindingSet was constructed with
  // |BindingSetDispatchMode::WITH_CONTEXT|.
  void* dispatch_context() const {
    DCHECK(SupportsContext());
    return dispatch_context_;
  }

  void FlushForTesting() {
    for (auto& binding : bindings_)
      binding.second->FlushForTesting();
  }

 private:
  friend class Entry;

  class Entry {
   public:
    Entry(Interface* impl,
          RequestType request,
          BindingSet* binding_set,
          BindingId binding_id,
          void* context)
        : binding_(impl, std::move(request)),
          binding_set_(binding_set),
          binding_id_(binding_id),
          context_(context) {
      if (binding_set->SupportsContext())
        binding_.AddFilter(base::MakeUnique<DispatchFilter>(this));
      binding_.set_connection_error_with_reason_handler(
          base::Bind(&Entry::OnConnectionError, base::Unretained(this)));
    }

    void FlushForTesting() { binding_.FlushForTesting(); }

   private:
    class DispatchFilter : public MessageReceiver {
     public:
      explicit DispatchFilter(Entry* entry) : entry_(entry) {}
      ~DispatchFilter() override {}

     private:
      // MessageReceiver:
      bool Accept(Message* message) override {
        entry_->WillDispatch();
        return true;
      }

      Entry* entry_;

      DISALLOW_COPY_AND_ASSIGN(DispatchFilter);
    };

    void WillDispatch() {
      DCHECK(binding_set_->SupportsContext());
      binding_set_->SetDispatchContext(context_);
    }

    void OnConnectionError(uint32_t custom_reason,
                           const std::string& description) {
      if (binding_set_->SupportsContext())
        WillDispatch();
      binding_set_->OnConnectionError(binding_id_, custom_reason, description);
    }

    BindingType binding_;
    BindingSet* const binding_set_;
    const BindingId binding_id_;
    void* const context_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  void SetDispatchContext(void* context) {
    DCHECK(SupportsContext());
    dispatch_context_ = context;
    if (!pre_dispatch_handler_.is_null())
      pre_dispatch_handler_.Run(context);
  }

  bool SupportsContext() const {
    return dispatch_mode_ == BindingSetDispatchMode::WITH_CONTEXT;
  }

  void OnConnectionError(BindingId id,
                         uint32_t custom_reason,
                         const std::string& description) {
    auto it = bindings_.find(id);
    DCHECK(it != bindings_.end());
    bindings_.erase(it);

    if (!error_handler_.is_null())
      error_handler_.Run();
    else if (!error_with_reason_handler_.is_null())
      error_with_reason_handler_.Run(custom_reason, description);
  }

  BindingSetDispatchMode dispatch_mode_;
  base::Closure error_handler_;
  ConnectionErrorWithReasonCallback error_with_reason_handler_;
  PreDispatchCallback pre_dispatch_handler_;
  BindingId next_binding_id_ = 0;
  std::map<BindingId, std::unique_ptr<Entry>> bindings_;
  void* dispatch_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BindingSet);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
