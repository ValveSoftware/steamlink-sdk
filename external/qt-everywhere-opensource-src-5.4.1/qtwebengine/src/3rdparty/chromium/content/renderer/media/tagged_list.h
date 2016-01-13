// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_TAGGED_LIST_H_
#define CONTENT_RENDERER_MEDIA_TAGGED_LIST_H_

#include <algorithm>
#include <list>

#include "base/logging.h"
#include "base/memory/ref_counted.h"

namespace content {

// Implements the pattern of a list of items, where added items are
// tagged, you can tag all items, and you can retrieve the list of
// items that are tagged, which removes the tag.
//
// For thread safety, operations on this object should be under an
// external lock. An internally-locked version could be created, but
// is not needed at the moment as users already lock.
template <class ItemType>
class TaggedList {
 public:
  typedef std::list<scoped_refptr<ItemType> > ItemList;

  TaggedList() {}

  void AddAndTag(ItemType* item) {
    items_.push_back(item);
    tagged_items_.push_back(item);
  }

  void TagAll() {
    tagged_items_ = items_;
  }

  const ItemList& Items() const {
    return items_;
  }

  // Retrieves the list of items with tags, and removes their tags.
  //
  // |dest| should be empty.
  void RetrieveAndClearTags(ItemList* dest) {
    DCHECK(dest->empty());
    dest->swap(tagged_items_);
  }

  // Remove an item that matches a predicate. Will return a reference
  // to it if it is found.
  template <class UnaryPredicate>
  scoped_refptr<ItemType> Remove(UnaryPredicate predicate) {
    tagged_items_.remove_if(predicate);

    typename ItemList::iterator it = std::find_if(
        items_.begin(), items_.end(), predicate);
    if (it != items_.end()) {
      scoped_refptr<ItemType> removed_item = *it;
      items_.erase(it);
      return removed_item;
    }

    return NULL;
  }

  template <class UnaryPredicate>
  bool Contains(UnaryPredicate predicate) const {
    return std::find_if(items_.begin(), items_.end(), predicate) !=
        items_.end();
  }

  void Clear() {
    items_.clear();
    tagged_items_.clear();
  }

  bool IsEmpty() const {
    bool is_empty = items_.empty();
    DCHECK(!is_empty || tagged_items_.empty());
    return is_empty;
  }

 private:
  ItemList items_;
  ItemList tagged_items_;

  DISALLOW_COPY_AND_ASSIGN(TaggedList);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_TAGGED_LIST_H_
