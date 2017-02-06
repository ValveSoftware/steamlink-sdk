// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_LIST_MODEL_H_
#define UI_BASE_MODELS_LIST_MODEL_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/base/models/list_model_observer.h"

namespace ui {

// A list model that manages a list of ItemType pointers. Items added to the
// model are owned by the model. An item can be taken out of the model by
// RemoveAt.
template <class ItemType>
class ListModel {
 public:
  ListModel() {}
  ~ListModel() {}

  // Adds |item| at the |index| into |items_|. Takes ownership of |item|.
  void AddAt(size_t index, ItemType* item) {
    DCHECK_LE(index, item_count());
    items_.insert(items_.begin() + index, item);
    NotifyItemsAdded(index, 1);
  }

  // Convenience function to append an item to the model.
  void Add(ItemType* item) {
    AddAt(item_count(), item);
  }

  // Removes the item at |index| from |items_| without deleting it.
  // Returns a scoped pointer containing the removed item.
  std::unique_ptr<ItemType> RemoveAt(size_t index) {
    DCHECK_LT(index, item_count());
    ItemType* item = items_[index];
    items_.weak_erase(items_.begin() + index);
    NotifyItemsRemoved(index, 1);
    return base::WrapUnique<ItemType>(item);
  }

  // Removes all items from the model. This does NOT delete the items.
  void RemoveAll() {
    size_t count = item_count();
    items_.weak_clear();
    NotifyItemsRemoved(0, count);
  }

  // Removes the item at |index| from |items_| and deletes it.
  void DeleteAt(size_t index) {
    std::unique_ptr<ItemType> item = RemoveAt(index);
    // |item| will be deleted on destruction.
  }

  // Removes and deletes all items from the model.
  void DeleteAll() {
    ScopedVector<ItemType> to_be_deleted(std::move(items_));
    NotifyItemsRemoved(0, to_be_deleted.size());
  }

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(size_t index, size_t target_index) {
    DCHECK_LT(index, item_count());
    DCHECK_LT(target_index, item_count());

    if (index == target_index)
      return;

    ItemType* item = items_[index];
    items_.weak_erase(items_.begin() + index);
    items_.insert(items_.begin() + target_index, item);
    NotifyItemMoved(index, target_index);
  }

  void AddObserver(ListModelObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(ListModelObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  void NotifyItemsAdded(size_t start, size_t count) {
    FOR_EACH_OBSERVER(ListModelObserver,
                      observers_,
                      ListItemsAdded(start, count));
  }

  void NotifyItemsRemoved(size_t start, size_t count) {
    FOR_EACH_OBSERVER(ListModelObserver,
                      observers_,
                      ListItemsRemoved(start, count));
  }

  void NotifyItemMoved(size_t index, size_t target_index) {
    FOR_EACH_OBSERVER(ListModelObserver,
                      observers_,
                      ListItemMoved(index, target_index));
  }

  void NotifyItemsChanged(size_t start, size_t count) {
    FOR_EACH_OBSERVER(ListModelObserver,
                      observers_,
                      ListItemsChanged(start, count));
  }

  size_t item_count() const { return items_.size(); }

  const ItemType* GetItemAt(size_t index) const {
    DCHECK_LT(index, item_count());
    return items_[index];
  }
  ItemType* GetItemAt(size_t index) {
    return const_cast<ItemType*>(
        const_cast<const ListModel<ItemType>*>(this)->GetItemAt(index));
  }

  // Iteration interface.
  typename ScopedVector<ItemType>::iterator begin() { return items_.begin(); }
  typename ScopedVector<ItemType>::const_iterator begin() const {
    return items_.begin();
  }
  typename ScopedVector<ItemType>::iterator end() { return items_.end(); }
  typename ScopedVector<ItemType>::const_iterator end() const {
    return items_.end();
  }

 private:
  ScopedVector<ItemType> items_;
  base::ObserverList<ListModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ListModel<ItemType>);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_LIST_MODEL_H_
