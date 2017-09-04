// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_SET_H_

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace mojo {
namespace internal {

// TODO(blundell): This class should be rewritten to be structured
// similarly to BindingSet if possible, with PtrSet owning its
// Elements and those Elements calling back into PtrSet on connection
// error.
template <typename Interface, template <typename> class Ptr>
class PtrSet {
 public:
  PtrSet() {}
  ~PtrSet() { CloseAll(); }

  void AddPtr(Ptr<Interface> ptr) {
    auto weak_interface_ptr = new Element(std::move(ptr));
    ptrs_.push_back(weak_interface_ptr->GetWeakPtr());
    ClearNullPtrs();
  }

  template <typename FunctionType>
  void ForAllPtrs(FunctionType function) {
    for (const auto& it : ptrs_) {
      if (it)
        function(it->get());
    }
    ClearNullPtrs();
  }

  void CloseAll() {
    for (const auto& it : ptrs_) {
      if (it)
        it->Close();
    }
    ptrs_.clear();
  }

 private:
  class Element {
   public:
    explicit Element(Ptr<Interface> ptr)
        : ptr_(std::move(ptr)), weak_ptr_factory_(this) {
      ptr_.set_connection_error_handler(base::Bind(&DeleteElement, this));
    }

    ~Element() {}

    void Close() {
      ptr_.reset();

      // Resetting the interface ptr means that it won't call this object back
      // on connection error anymore, so this object must delete itself now.
      DeleteElement(this);
    }

    Interface* get() { return ptr_.get(); }

    base::WeakPtr<Element> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    static void DeleteElement(Element* element) { delete element; }

    Ptr<Interface> ptr_;
    base::WeakPtrFactory<Element> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(Element);
  };

  void ClearNullPtrs() {
    ptrs_.erase(std::remove_if(ptrs_.begin(), ptrs_.end(),
                               [](const base::WeakPtr<Element>& p) {
                                 return p.get() == nullptr;
                               }),
                ptrs_.end());
  }

  std::vector<base::WeakPtr<Element>> ptrs_;
};

}  // namespace internal

template <typename Interface>
using InterfacePtrSet = internal::PtrSet<Interface, InterfacePtr>;

template <typename Interface>
using AssociatedInterfacePtrSet =
    internal::PtrSet<Interface, AssociatedInterfacePtr>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_SET_H_
