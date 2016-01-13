// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_PUBLIC_VIEW_H_
#define UI_V2_PUBLIC_VIEW_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/rect.h"
#include "ui/v2/public/layout.h"
#include "ui/v2/public/painter.h"
#include "ui/v2/public/v2_export.h"

namespace ui {
class Layer;
}

namespace v2 {

class Layout;
class Painter;
class ViewObserver;
class ViewLayerOwner;

class V2_EXPORT View /* : public EventTarget */ {
 public:
  typedef std::vector<View*> Children;

  View();
  virtual ~View();

  // Configuration.

  void set_owned_by_parent(bool owned_by_parent) {
    owned_by_parent_ = owned_by_parent;
  }

  // View takes ownership.
  void SetPainter(Painter* painter);

  // See documentation in layout.h. A layout manager's rules apply to this
  // View's children.
  // View takes ownership.
  void SetLayout(Layout* layout);

  // Observation.

  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);

  // Disposition.

  void SetBounds(const gfx::Rect& bounds);
  gfx::Rect bounds() const { return bounds_; }

  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Tree.

  View* parent() { return parent_; }
  const View* parent() const { return parent_; }
  const Children& children() const { return children_; }

  void AddChild(View* child);
  void RemoveChild(View* child);

  bool Contains(View* child) const;

  void StackChildAtTop(View* child);
  void StackChildAtBottom(View* child);
  void StackChildAbove(View* child, View* other);
  void StackChildBelow(View* child, View* other);

  // Layer.

  inline const ui::Layer* layer() const;
  inline ui::Layer* layer();
  bool HasLayer() const;
  void CreateLayer(ui::LayerType layer_type);
  void DestroyLayer();
  ui::Layer* AcquireLayer();

 private:
  friend class ViewPrivate;

  // Disposition attributes.
  gfx::Rect bounds_;
  bool visible_;

  scoped_ptr<Painter> painter_;
  scoped_ptr<Layout> layout_;

  // Tree.
  bool owned_by_parent_;
  View* parent_;
  Children children_;

  // Layer.
  scoped_ptr<ViewLayerOwner> layer_owner_;

  ObserverList<ViewObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace v2

#endif  // UI_V2_PUBLIC_VIEW_H_
