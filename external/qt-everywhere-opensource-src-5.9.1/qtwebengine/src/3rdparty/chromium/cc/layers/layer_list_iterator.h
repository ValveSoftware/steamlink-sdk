// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_LAYER_LIST_ITERATOR_H_

#include <stdlib.h>
#include <vector>

#include "cc/base/cc_export.h"

namespace cc {

class LayerImpl;

// Unlike LayerIterator and friends, these iterators are not intended to permit
// traversing the RSLL. Rather, they visit a collection of LayerImpls in
// stacking order. All recursive walks over the LayerImpl tree should be
// switched to use these classes instead as the concept of a LayerImpl tree is
// deprecated.
template <typename LayerType>
class CC_EXPORT LayerListIterator {
 public:
  explicit LayerListIterator(LayerType* root_layer);
  LayerListIterator(const LayerListIterator<LayerType>& other);
  virtual ~LayerListIterator();

  bool operator==(const LayerListIterator<LayerType>& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListIterator<LayerType>& other) const {
    return !(*this == other);
  }

  // We will only support prefix increment.
  virtual LayerListIterator& operator++();
  LayerType* operator->() const { return current_layer_; }
  LayerType* operator*() const { return current_layer_; }

 protected:
  // The implementation of this iterator is currently tied tightly to the layer
  // tree, but it should be straightforward to reimplement in terms of a list
  // when it's ready.
  LayerType* current_layer_;
  std::vector<size_t> list_indices_;
};

template <typename LayerType>
class CC_EXPORT LayerListReverseIterator : public LayerListIterator<LayerType> {
 public:
  explicit LayerListReverseIterator(LayerType* root_layer);
  ~LayerListReverseIterator() override;

  // We will only support prefix increment.
  LayerListIterator<LayerType>& operator++() override;

 private:
  void DescendToRightmostInSubtree();
  LayerType* current_layer() { return this->current_layer_; }
  std::vector<size_t>& list_indices() { return this->list_indices_; }
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_LIST_ITERATOR_H_
