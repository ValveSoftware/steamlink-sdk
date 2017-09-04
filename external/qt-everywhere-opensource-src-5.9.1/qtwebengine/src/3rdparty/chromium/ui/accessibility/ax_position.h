// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_POSITION_H_
#define UI_ACCESSIBILITY_AX_POSITION_H_

#include <stdint.h>

#include <memory>

namespace ui {

// Defines the type of position in the accessibility tree.
// A tree position is used when referring to a specific child of a node in the
// accessibility tree.
// A text position is used when referring to a specific character of text inside
// a particular node.
// A null position is used to signify that the provided data is invalid or that
// a boundary has been reached.
enum class AXPositionKind { NullPosition, TreePosition, TextPosition };

// A position in the |AXTree|.
// It could either indicate a non-textual node in the accessibility tree, or a
// text node and a character offset.
// A text node has either a role of static_text, inline_text_box or line_break.
//
// This class template uses static polymorphism in order to allow sub-classes to
// be created from the base class without the base class knowing the type of the
// sub-class in advance.
// The template argument |AXPositionType| should always be set to the type of
// any class that inherits from this template, making this a
// "curiously recursive template".
template <class AXPositionType, class AXNodeType>
class AXPosition {
 public:
  static int INVALID_TREE_ID;
  static int INVALID_ANCHOR_ID;
  static int INVALID_INDEX;
  static int INVALID_OFFSET;

  AXPosition() {}
  virtual ~AXPosition() {}

  static AXPosition<AXPositionType, AXNodeType>* CreateNullPosition() {
    auto new_position = static_cast<AXPosition<AXPositionType, AXNodeType>*>(
        new AXPositionType());
    DCHECK(new_position);
    new_position->Initialize(AXPositionKind::NullPosition, INVALID_TREE_ID,
                             INVALID_ANCHOR_ID, INVALID_INDEX, INVALID_OFFSET,
                             AX_TEXT_AFFINITY_UPSTREAM);
    return new_position;
  }

  static AXPosition<AXPositionType, AXNodeType>*
  CreateTreePosition(int tree_id, int32_t anchor_id, int child_index) {
    auto new_position = static_cast<AXPosition<AXPositionType, AXNodeType>*>(
        new AXPositionType());
    DCHECK(new_position);
    new_position->Initialize(AXPositionKind::TreePosition, tree_id, anchor_id,
                             child_index, INVALID_OFFSET,
                             AX_TEXT_AFFINITY_UPSTREAM);
    return new_position;
  }

  static AXPosition<AXPositionType, AXNodeType>* CreateTextPosition(
      int tree_id,
      int32_t anchor_id,
      int text_offset,
      AXTextAffinity affinity) {
    auto new_position = static_cast<AXPosition<AXPositionType, AXNodeType>*>(
        new AXPositionType());
    DCHECK(new_position);
    new_position->Initialize(AXPositionKind::TextPosition, tree_id, anchor_id,
                             INVALID_INDEX, text_offset, affinity);
    return new_position;
  }

  int tree_id() const { return tree_id_; }
  int32_t anchor_id() const { return anchor_id_; }

  AXNodeType* GetAnchor() const {
    if (tree_id_ == INVALID_TREE_ID || anchor_id_ == INVALID_ANCHOR_ID)
      return nullptr;
    DCHECK_GE(tree_id_, 0);
    DCHECK_GE(anchor_id_, 0);
    return GetNodeInTree(tree_id_, anchor_id_);
  }

  AXPositionKind kind() const { return kind_; }
  int child_index() const { return child_index_; }
  int text_offset() const { return text_offset_; }
  AXTextAffinity affinity() const { return affinity_; }

  bool IsNullPosition() const {
    return kind_ == AXPositionKind::NullPosition || !GetAnchor();
  }
  bool IsTreePosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TreePosition;
  }
  bool IsTextPosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TextPosition;
  }

  bool AtStartOfAnchor() const {
    if (!GetAnchor())
      return false;

    switch (kind_) {
      case AXPositionKind::NullPosition:
        return false;
      case AXPositionKind::TreePosition:
        return child_index_ == 0;
      case AXPositionKind::TextPosition:
        return text_offset_ == 0;
    }

    return false;
  }

  bool AtEndOfAnchor() const {
    if (!GetAnchor())
      return false;

    switch (kind_) {
      case AXPositionKind::NullPosition:
        return false;
      case AXPositionKind::TreePosition:
        return child_index_ == AnchorChildCount();
      case AXPositionKind::TextPosition:
        return text_offset_ == MaxTextOffset();
    }

    return false;
  }

  AXPosition<AXPositionType, AXNodeType>* CreatePositionAtStartOfAnchor()
      const {
    switch (kind_) {
      case AXPositionKind::NullPosition:
        return CreateNullPosition();
      case AXPositionKind::TreePosition:
        return CreateTreePosition(tree_id_, anchor_id_, 0 /* child_index */);
      case AXPositionKind::TextPosition:
        return CreateTextPosition(tree_id_, anchor_id_, 0 /* text_offset */,
                                  AX_TEXT_AFFINITY_UPSTREAM);
    }
    return CreateNullPosition();
  }

  AXPosition<AXPositionType, AXNodeType>* CreatePositionAtEndOfAnchor() const {
    switch (kind_) {
      case AXPositionKind::NullPosition:
        return CreateNullPosition();
      case AXPositionKind::TreePosition:
        return CreateTreePosition(tree_id_, anchor_id_, AnchorChildCount());
      case AXPositionKind::TextPosition:
        return CreateTextPosition(tree_id_, anchor_id_, MaxTextOffset(),
                                  AX_TEXT_AFFINITY_UPSTREAM);
    }
    return CreateNullPosition();
  }

  AXPosition<AXPositionType, AXNodeType>* CreateChildPositionAt(
      int child_index) const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (child_index < 0 || child_index >= AnchorChildCount())
      return CreateNullPosition();

    int tree_id = INVALID_TREE_ID;
    int32_t child_id = INVALID_ANCHOR_ID;
    AnchorChild(child_index, &tree_id, &child_id);
    DCHECK_NE(tree_id, INVALID_TREE_ID);
    DCHECK_NE(child_id, INVALID_ANCHOR_ID);
    switch (kind_) {
      case AXPositionKind::NullPosition:
        NOTREACHED();
        return CreateNullPosition();
      case AXPositionKind::TreePosition:
        return CreateTreePosition(tree_id, child_id, 0 /* child_index */);
      case AXPositionKind::TextPosition:
        return CreateTextPosition(tree_id, child_id, 0 /* text_offset */,
                                  AX_TEXT_AFFINITY_UPSTREAM);
    }

    return CreateNullPosition();
  }

  AXPosition<AXPositionType, AXNodeType>* CreateParentPosition() const {
    if (IsNullPosition())
      return CreateNullPosition();

    int tree_id = INVALID_TREE_ID;
    int32_t parent_id = INVALID_ANCHOR_ID;
    AnchorParent(&tree_id, &parent_id);
    if (tree_id == INVALID_TREE_ID || parent_id == INVALID_ANCHOR_ID)
      return CreateNullPosition();

    DCHECK_GE(tree_id, 0);
    DCHECK_GE(parent_id, 0);
    switch (kind_) {
      case AXPositionKind::NullPosition:
        NOTREACHED();
        return CreateNullPosition();
      case AXPositionKind::TreePosition:
        return CreateTreePosition(tree_id, parent_id, 0 /* child_index */);
      case AXPositionKind::TextPosition:
        return CreateTextPosition(tree_id, parent_id, 0 /* text_offset */,
                                  AX_TEXT_AFFINITY_UPSTREAM);
    }

    return CreateNullPosition();
  }

  // The following methods work across anchors.

  // TODO(nektar): Not yet implemented for tree positions.
  AXPosition<AXPositionType, AXNodeType>* CreateNextCharacterPosition() const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (text_offset_ + 1 < MaxTextOffset()) {
      return CreateTextPosition(tree_id_, anchor_id_, text_offset_ + 1,
                                AX_TEXT_AFFINITY_UPSTREAM);
    }

    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> next_leaf(
        CreateNextAnchorPosition());
    while (next_leaf && !next_leaf->IsNullPosition() &&
           next_leaf->AnchorChildCount()) {
      next_leaf.reset(next_leaf->CreateNextAnchorPosition());
    }

    DCHECK(next_leaf);
    return next_leaf.release();
  }

  // TODO(nektar): Not yet implemented for tree positions.
  AXPosition<AXPositionType, AXNodeType>* CreatePreviousCharacterPosition()
      const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (text_offset_ > 0) {
      return CreateTextPosition(tree_id_, anchor_id_, text_offset_ - 1,
                                AX_TEXT_AFFINITY_UPSTREAM);
    }

    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> previous_leaf(
        CreatePreviousAnchorPosition());
    while (previous_leaf && !previous_leaf->IsNullPosition() &&
           previous_leaf->AnchorChildCount()) {
      previous_leaf.reset(previous_leaf->CreatePreviousAnchorPosition());
    }

    DCHECK(previous_leaf);
    previous_leaf.reset(previous_leaf->CreatePositionAtEndOfAnchor());
    if (!previous_leaf->AtStartOfAnchor())
      --previous_leaf->text_offset_;
    return previous_leaf.release();
  }

  // TODO(nektar): Add word, line and paragraph navigation methods.

 protected:
  virtual void Initialize(AXPositionKind kind,
                          int tree_id,
                          int32_t anchor_id,
                          int child_index,
                          int text_offset,
                          AXTextAffinity affinity) {
    kind_ = kind;
    tree_id_ = tree_id;
    anchor_id_ = anchor_id;
    child_index_ = child_index;
    text_offset_ = text_offset;
    affinity_ = affinity;

    if (!GetAnchor() ||
        (child_index_ != INVALID_INDEX &&
         (child_index_ < 0 || child_index_ > AnchorChildCount())) ||
        (text_offset_ != INVALID_OFFSET &&
         (text_offset_ < 0 || text_offset_ > MaxTextOffset()))) {
      // reset to the null position.
      kind_ = AXPositionKind::NullPosition;
      tree_id_ = INVALID_TREE_ID;
      anchor_id_ = INVALID_ANCHOR_ID;
      child_index_ = INVALID_INDEX;
      text_offset_ = INVALID_OFFSET;
      affinity_ = AX_TEXT_AFFINITY_UPSTREAM;
    }
  }

  // Uses depth-first pre-order traversal.
  virtual AXPosition<AXPositionType, AXNodeType>* CreateNextAnchorPosition()
      const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (AnchorChildCount())
      return CreateChildPositionAt(0);

    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> current_position(
        CreateTreePosition(tree_id_, anchor_id_, child_index_));
    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> parent_position(
        CreateParentPosition());
    while (parent_position && !parent_position->IsNullPosition()) {
      // Get the next sibling if it exists, otherwise move up to the parent's
      // next sibling.
      int index_in_parent = current_position->AnchorIndexInParent();
      if (index_in_parent < parent_position->AnchorChildCount() - 1) {
        AXPosition<AXPositionType, AXNodeType>* next_sibling =
            parent_position->CreateChildPositionAt(index_in_parent + 1);
        DCHECK(next_sibling && !next_sibling->IsNullPosition());
        return next_sibling;
      }

      current_position = std::move(parent_position);
      parent_position.reset(current_position->CreateParentPosition());
    }

    return CreateNullPosition();
  }

  // Uses depth-first pre-order traversal.
  virtual AXPosition<AXPositionType, AXNodeType>* CreatePreviousAnchorPosition()
      const {
    if (IsNullPosition())
      return CreateNullPosition();

    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> parent_position(
        CreateParentPosition());
    if (!parent_position || parent_position->IsNullPosition())
      return CreateNullPosition();

    // Get the previous sibling's deepest first child if a previous sibling
    // exists, otherwise move up to the parent.
    int index_in_parent = AnchorIndexInParent();
    if (index_in_parent <= 0)
      return parent_position.release();

    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> leaf(
        parent_position->CreateChildPositionAt(index_in_parent - 1));
    while (leaf && !leaf->IsNullPosition() && leaf->AnchorChildCount())
      leaf.reset(leaf->CreateChildPositionAt(0));

    return leaf.release();
  }

  // Abstract methods.
  virtual void AnchorChild(int child_index,
                           int* tree_id,
                           int32_t* child_id) const = 0;
  virtual int AnchorChildCount() const = 0;
  virtual int AnchorIndexInParent() const = 0;
  virtual void AnchorParent(int* tree_id, int32_t* parent_id) const = 0;
  virtual AXNodeType* GetNodeInTree(int tree_id, int32_t node_id) const = 0;
  // Returns the length of the text that is present inside the anchor node,
  // including any text found on descendant nodes.
  virtual int MaxTextOffset() const = 0;

 private:
  AXPositionKind kind_;
  int tree_id_;
  int32_t anchor_id_;

  // For text positions, |child_index_| is initially set to |-1| and only
  // computed on demand. The same with tree positions and |text_offset_|.
  int child_index_;
  int text_offset_;

  // TODO(nektar): Get rid of affinity and make Blink handle affinity
  // internally since inline text objects don't span lines.
  ui::AXTextAffinity affinity_;
};

template <class AXPositionType, class AXNodeType>
int AXPosition<AXPositionType, AXNodeType>::INVALID_TREE_ID = -1;
template <class AXPositionType, class AXNodeType>
int32_t AXPosition<AXPositionType, AXNodeType>::INVALID_ANCHOR_ID = -1;
template <class AXPositionType, class AXNodeType>
int AXPosition<AXPositionType, AXNodeType>::INVALID_INDEX = -1;
template <class AXPositionType, class AXNodeType>
int AXPosition<AXPositionType, AXNodeType>::INVALID_OFFSET = -1;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_POSITION_H_
