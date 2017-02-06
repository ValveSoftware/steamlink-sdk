// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {

// Use this class to manage a set of bindings, which are automatically destroyed
// and removed from the set when the pipe they are bound to is disconnected.
template <typename Interface>
class BindingSet {
 public:
  BindingSet() {}
  ~BindingSet() { CloseAllBindings(); }

  void set_connection_error_handler(const base::Closure& error_handler) {
    error_handler_ = error_handler;
  }

  void AddBinding(Interface* impl, InterfaceRequest<Interface> request) {
    auto binding = new Element(impl, std::move(request));
    binding->set_connection_error_handler(
        base::Bind(&BindingSet::OnConnectionError, base::Unretained(this)));
    bindings_.push_back(binding->GetWeakPtr());
  }

  // Returns an InterfacePtr bound to one end of a pipe whose other end is
  // bound to |this|.
  InterfacePtr<Interface> CreateInterfacePtrAndBind(Interface* impl) {
    InterfacePtr<Interface> interface_ptr;
    AddBinding(impl, GetProxy(&interface_ptr));
    return interface_ptr;
  }

  void CloseAllBindings() {
    for (const auto& it : bindings_) {
      if (it) {
        it->Close();
        delete it.get();
      }
    }
    bindings_.clear();
  }

  bool empty() const { return bindings_.empty(); }

 private:
  class Element {
   public:
    Element(Interface* impl, InterfaceRequest<Interface> request)
        : binding_(impl, std::move(request)), weak_ptr_factory_(this) {
      binding_.set_connection_error_handler(
          base::Bind(&Element::OnConnectionError, base::Unretained(this)));
    }

    ~Element() {}

    void set_connection_error_handler(const base::Closure& error_handler) {
      error_handler_ = error_handler;
    }

    base::WeakPtr<Element> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

    void Close() { binding_.Close(); }

    void OnConnectionError() {
      base::Closure error_handler = error_handler_;
      delete this;
      if (!error_handler.is_null())
        error_handler.Run();
    }

   private:
    Binding<Interface> binding_;
    base::Closure error_handler_;
    base::WeakPtrFactory<Element> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(Element);
  };

  void OnConnectionError() {
    // Clear any deleted bindings.
    bindings_.erase(std::remove_if(bindings_.begin(), bindings_.end(),
                                   [](const base::WeakPtr<Element>& p) {
                                     return p.get() == nullptr;
                                   }),
                    bindings_.end());

    if (!error_handler_.is_null())
      error_handler_.Run();
  }

  base::Closure error_handler_;
  std::vector<base::WeakPtr<Element>> bindings_;

  DISALLOW_COPY_AND_ASSIGN(BindingSet);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
