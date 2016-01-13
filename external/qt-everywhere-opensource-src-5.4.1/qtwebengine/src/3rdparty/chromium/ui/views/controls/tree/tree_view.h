// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TREE_TREE_VIEW_VIEWS_H_
#define UI_VIEWS_CONTROLS_TREE_TREE_VIEW_VIEWS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {

class Textfield;
class TreeViewController;
class PrefixSelector;

// TreeView displays hierarchical data as returned from a TreeModel. The user
// can expand, collapse and edit the items. A Controller may be attached to
// receive notification of selection changes and restrict editing.
//
// Note on implementation. This implementation doesn't scale well. In particular
// it does not store any row information, but instead calculates it as
// necessary. But it's more than adequate for current uses.
class VIEWS_EXPORT TreeView : public ui::TreeModelObserver,
                              public TextfieldController,
                              public FocusChangeListener,
                              public PrefixDelegate {
 public:
  // The tree view's class name.
  static const char kViewClassName[];

  TreeView();
  virtual ~TreeView();

  // Returns new ScrollPane that contains the receiver.
  View* CreateParentIfNecessary();

  // Sets the model. TreeView does not take ownership of the model.
  void SetModel(ui::TreeModel* model);
  ui::TreeModel* model() const { return model_; }

  // Sets whether to automatically expand children when a parent node is
  // expanded. The default is false. If true, when a node in the tree is
  // expanded for the first time, its children are also automatically expanded.
  // If a node is subsequently collapsed and expanded again, the children
  // will not be automatically expanded.
  void set_auto_expand_children(bool auto_expand_children) {
    auto_expand_children_ = auto_expand_children;
  }

  // Sets whether the user can edit the nodes. The default is true. If true,
  // the Controller is queried to determine if a particular node can be edited.
  void SetEditable(bool editable);

  // Edits the specified node. This cancels the current edit and expands all
  // parents of node.
  void StartEditing(ui::TreeModelNode* node);

  // Cancels the current edit. Does nothing if not editing.
  void CancelEdit();

  // Commits the current edit. Does nothing if not editing.
  void CommitEdit();

  // If the user is editing a node, it is returned. If the user is not
  // editing a node, NULL is returned.
  ui::TreeModelNode* GetEditingNode();

  // Selects the specified node. This expands all the parents of node.
  void SetSelectedNode(ui::TreeModelNode* model_node);

  // Returns the selected node, or NULL if nothing is selected.
  ui::TreeModelNode* GetSelectedNode();

  // Marks |model_node| as collapsed. This only effects the UI if node and all
  // its parents are expanded (IsExpanded(model_node) returns true).
  void Collapse(ui::TreeModelNode* model_node);

  // Make sure node and all its parents are expanded.
  void Expand(ui::TreeModelNode* node);

  // Invoked from ExpandAll(). Expands the supplied node and recursively
  // invokes itself with all children.
  void ExpandAll(ui::TreeModelNode* node);

  // Returns true if the specified node is expanded.
  bool IsExpanded(ui::TreeModelNode* model_node);

  // Sets whether the root is shown. If true, the root node of the tree is
  // shown, if false only the children of the root are shown. The default is
  // true.
  void SetRootShown(bool root_visible);

  // Sets the controller, which may be null. TreeView does not take ownership
  // of the controller.
  void SetController(TreeViewController* controller) {
    controller_ = controller;
  }

  // Returns the node for the specified row, or NULL for an invalid row index.
  ui::TreeModelNode* GetNodeForRow(int row);

  // Maps a node to a row, returns -1 if node is not valid.
  int GetRowForNode(ui::TreeModelNode* node);

  views::Textfield* editor() { return editor_; }

  // View overrides:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void ShowContextMenu(const gfx::Point& p,
                               ui::MenuSourceType source_type) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // TreeModelObserver overrides:
  virtual void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count) OVERRIDE;
  virtual void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* parent,
                                int start,
                                int count) OVERRIDE;
  virtual void TreeNodeChanged(ui::TreeModel* model,
                               ui::TreeModelNode* model_node) OVERRIDE;

  // TextfieldController overrides:
  virtual void ContentsChanged(Textfield* sender,
                               const base::string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;

  // FocusChangeListener overrides:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

  // PrefixDelegate overrides:
  virtual int GetRowCount() OVERRIDE;
  virtual int GetSelectedRow() OVERRIDE;
  virtual void SetSelectedRow(int row) OVERRIDE;
  virtual base::string16 GetTextForRow(int row) OVERRIDE;

 protected:
  // View overrides:
  virtual gfx::Point GetKeyboardContextMenuLocation() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

 private:
  friend class TreeViewTest;

  // Selects, expands or collapses nodes in the tree.  Consistent behavior for
  // tap gesture and click events.
  bool OnClickOrTap(const ui::LocatedEvent& event);

  // InternalNode is used to track information about the set of nodes displayed
  // by TreeViewViews.
  class InternalNode : public ui::TreeNode<InternalNode> {
   public:
    InternalNode();
    virtual ~InternalNode();

    // Resets the state from |node|.
    void Reset(ui::TreeModelNode* node);

    // The model node this InternalNode represents.
    ui::TreeModelNode* model_node() { return model_node_; }

    // Whether the node is expanded.
    void set_is_expanded(bool expanded) { is_expanded_ = expanded; }
    bool is_expanded() const { return is_expanded_; }

    // Whether children have been loaded.
    void set_loaded_children(bool value) { loaded_children_ = value; }
    bool loaded_children() const { return loaded_children_; }

    // Width needed to display the string.
    void set_text_width(int width) { text_width_ = width; }
    int text_width() const { return text_width_; }

    // Returns the total number of descendants (including this node).
    int NumExpandedNodes() const;

    // Returns the max width of all descendants (including this node). |indent|
    // is how many pixels each child is indented and |depth| is the depth of
    // this node from its parent.
    int GetMaxWidth(int indent, int depth);

   private:
    // The node from the model.
    ui::TreeModelNode* model_node_;

    // Whether the children have been loaded.
    bool loaded_children_;

    bool is_expanded_;

    int text_width_;

    DISALLOW_COPY_AND_ASSIGN(InternalNode);
  };

  // Used by GetInternalNodeForModelNode.
  enum GetInternalNodeCreateType {
    // If an InternalNode hasn't been created yet, create it.
    CREATE_IF_NOT_LOADED,

    // Don't create an InternalNode if one hasn't been created yet.
    DONT_CREATE_IF_NOT_LOADED,
  };

  // Used by IncrementSelection.
  enum IncrementType {
    // Selects the next node.
    INCREMENT_NEXT,

    // Selects the previous node.
    INCREMENT_PREVIOUS
  };

  // Row of the root node. This varies depending upon whether the root is
  // visible.
  int root_row() const { return root_shown_ ? 0 : -1; }

  // Depth of the root node.
  int root_depth() const { return root_shown_ ? 0 : -1; }

  // Loads the children of the specified node.
  void LoadChildren(InternalNode* node);

  // Configures an InternalNode from a node from the model. This is used
  // when a node changes as well as when loading.
  void ConfigureInternalNode(ui::TreeModelNode* model_node, InternalNode* node);

  // Sets |node|s text_width.
  void UpdateNodeTextWidth(InternalNode* node);

  // Invoked when the set of drawn nodes changes.
  void DrawnNodesChanged();

  // Updates |preferred_size_| from the state of the UI.
  void UpdatePreferredSize();

  // Positions |editor_|.
  void LayoutEditor();

  // Schedules a paint for |node|.
  void SchedulePaintForNode(InternalNode* node);

  // Recursively paints rows from |min_row| to |max_row|. |node| is the node for
  // the row |*row|. |row| is updated as this walks the tree. Depth is the depth
  // of |*row|.
  void PaintRows(gfx::Canvas* canvas,
                 int min_row,
                 int max_row,
                 InternalNode* node,
                 int depth,
                 int* row);

  // Invoked to paint a single node.
  void PaintRow(gfx::Canvas* canvas,
                InternalNode* node,
                int row,
                int depth);

  // Paints the expand control given the specified nodes bounds.
  void PaintExpandControl(gfx::Canvas* canvas,
                          const gfx::Rect& node_bounds,
                          bool expanded);

  // Returns the InternalNode for a model node. |create_type| indicates wheter
  // this should load InternalNode or not.
  InternalNode* GetInternalNodeForModelNode(
      ui::TreeModelNode* model_node,
      GetInternalNodeCreateType create_type);

  // Returns the bounds for a node.
  gfx::Rect GetBoundsForNode(InternalNode* node);

  // Implementation of GetBoundsForNode. Separated out as some callers already
  // know the row/depth.
  gfx::Rect GetBoundsForNodeImpl(InternalNode* node, int row, int depth);

  // Returns the row and depth of a node.
  int GetRowForInternalNode(InternalNode* node, int* depth);

  // Returns the row and depth of the specified node.
  InternalNode* GetNodeByRow(int row, int* depth);

  // Implementation of GetNodeByRow. |curent_row| is updated as we iterate.
  InternalNode* GetNodeByRowImpl(InternalNode* node,
                                 int target_row,
                                 int current_depth,
                                 int* current_row,
                                 int* node_depth);

  // Increments the selection. Invoked in response to up/down arrow.
  void IncrementSelection(IncrementType type);

  // If the current node is expanded, it's collapsed, otherwise selection is
  // moved to the parent.
  void CollapseOrSelectParent();

  // If the selected node is collapsed, it's expanded. Otherwise the first child
  // is seleected.
  void ExpandOrSelectChild();

  // Implementation of Expand(). Returns true if at least one node was expanded
  // that previously wasn't.
  bool ExpandImpl(ui::TreeModelNode* model_node);

  // The model, may be null.
  ui::TreeModel* model_;

  // Default icons for closed/open.
  gfx::ImageSkia closed_icon_;
  gfx::ImageSkia open_icon_;

  // Icons from the model.
  std::vector<gfx::ImageSkia> icons_;

  // The root node.
  InternalNode root_;

  // The selected node, may be NULL.
  InternalNode* selected_node_;

  bool editing_;

  // The editor; lazily created and never destroyed (well, until TreeView is
  // destroyed). Hidden when no longer editing. We do this avoid destruction
  // problems.
  Textfield* editor_;

  // Preferred size of |editor_| with no content.
  gfx::Size empty_editor_size_;

  // If non-NULL we've attached a listener to this focus manager. Used to know
  // when focus is changing to another view so that we can cancel the edit.
  FocusManager* focus_manager_;

  // Whether to automatically expand children when a parent node is expanded.
  bool auto_expand_children_;

  // Whether the user can edit the items.
  bool editable_;

  // The controller.
  TreeViewController* controller_;

  // Whether or not the root is shown in the tree.
  bool root_shown_;

  // Cached preferred size.
  gfx::Size preferred_size_;

  // Font list used to display text.
  gfx::FontList font_list_;

  // Height of each row. Based on font and some padding.
  int row_height_;

  // Offset the text is drawn at. This accounts for the size of the expand
  // control, icon and offsets.
  int text_offset_;

  scoped_ptr<PrefixSelector> selector_;

  DISALLOW_COPY_AND_ASSIGN(TreeView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TREE_TREE_VIEW_VIEWS_H_
