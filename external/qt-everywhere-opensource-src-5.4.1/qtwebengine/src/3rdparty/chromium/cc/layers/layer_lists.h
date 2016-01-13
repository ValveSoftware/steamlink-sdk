// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_LISTS_H_
#define CC_LAYERS_LAYER_LISTS_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"

namespace cc {
class Layer;
class LayerImpl;

typedef std::vector<scoped_refptr<Layer> > LayerList;

typedef ScopedPtrVector<LayerImpl> OwnedLayerImplList;
typedef std::vector<LayerImpl*> LayerImplList;

class CC_EXPORT RenderSurfaceLayerList {
 public:
  RenderSurfaceLayerList();
  ~RenderSurfaceLayerList();

  Layer* at(size_t i) const;
  void pop_back();
  void push_back(const scoped_refptr<Layer>& layer);
  Layer* back();
  size_t size() const;
  bool empty() const { return size() == 0u; }
  scoped_refptr<Layer>& operator[](size_t i);
  const scoped_refptr<Layer>& operator[](size_t i) const;
  LayerList::iterator begin();
  LayerList::iterator end();
  LayerList::const_iterator begin() const;
  LayerList::const_iterator end() const;
  void clear();
  LayerList& AsLayerList() { return list_; }
  const LayerList& AsLayerList() const { return list_; }

 private:
  LayerList list_;

  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceLayerList);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_LISTS_H_
