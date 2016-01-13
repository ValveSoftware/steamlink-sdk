// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_model.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/utf16_indexing.h"

namespace views {

namespace internal {

// Edit holds state information to undo/redo editing changes. Editing operations
// are merged when possible, like when characters are typed in sequence. Calling
// Commit() marks an edit as an independent operation that shouldn't be merged.
class Edit {
 public:
  enum Type {
    INSERT_EDIT,
    DELETE_EDIT,
    REPLACE_EDIT,
  };

  virtual ~Edit() {}

  // Revert the change made by this edit in |model|.
  void Undo(TextfieldModel* model) {
    model->ModifyText(new_text_start_, new_text_end(),
                      old_text_, old_text_start_,
                      old_cursor_pos_);
  }

  // Apply the change of this edit to the |model|.
  void Redo(TextfieldModel* model) {
    model->ModifyText(old_text_start_, old_text_end(),
                      new_text_, new_text_start_,
                      new_cursor_pos_);
  }

  // Try to merge the |edit| into this edit and returns true on success. The
  // merged edit will be deleted after redo and should not be reused.
  bool Merge(const Edit* edit) {
    // Don't merge if previous edit is DELETE. This happens when a
    // user deletes characters then hits return. In this case, the
    // delete should be treated as separate edit that can be undone
    // and should not be merged with the replace edit.
    if (type_ != DELETE_EDIT && edit->force_merge()) {
      MergeReplace(edit);
      return true;
    }
    return mergeable() && edit->mergeable() && DoMerge(edit);
  }

  // Commits the edit and marks as un-mergeable.
  void Commit() { merge_type_ = DO_NOT_MERGE; }

 private:
  friend class InsertEdit;
  friend class ReplaceEdit;
  friend class DeleteEdit;

  Edit(Type type,
       MergeType merge_type,
       size_t old_cursor_pos,
       const base::string16& old_text,
       size_t old_text_start,
       bool delete_backward,
       size_t new_cursor_pos,
       const base::string16& new_text,
       size_t new_text_start)
      : type_(type),
        merge_type_(merge_type),
        old_cursor_pos_(old_cursor_pos),
        old_text_(old_text),
        old_text_start_(old_text_start),
        delete_backward_(delete_backward),
        new_cursor_pos_(new_cursor_pos),
        new_text_(new_text),
        new_text_start_(new_text_start) {
  }

  // Each type of edit provides its own specific merge implementation.
  virtual bool DoMerge(const Edit* edit) = 0;

  Type type() const { return type_; }

  // Can this edit be merged?
  bool mergeable() const { return merge_type_ == MERGEABLE; }

  // Should this edit be forcibly merged with the previous edit?
  bool force_merge() const { return merge_type_ == FORCE_MERGE; }

  // Returns the end index of the |old_text_|.
  size_t old_text_end() const { return old_text_start_ + old_text_.length(); }

  // Returns the end index of the |new_text_|.
  size_t new_text_end() const { return new_text_start_ + new_text_.length(); }

  // Merge the replace edit into the current edit. This handles the special case
  // where an omnibox autocomplete string is set after a new character is typed.
  void MergeReplace(const Edit* edit) {
    CHECK_EQ(REPLACE_EDIT, edit->type_);
    CHECK_EQ(0U, edit->old_text_start_);
    CHECK_EQ(0U, edit->new_text_start_);
    base::string16 old_text = edit->old_text_;
    old_text.erase(new_text_start_, new_text_.length());
    old_text.insert(old_text_start_, old_text_);
    // SetText() replaces entire text. Set |old_text_| to the entire
    // replaced text with |this| edit undone.
    old_text_ = old_text;
    old_text_start_ = edit->old_text_start_;
    delete_backward_ = false;

    new_text_ = edit->new_text_;
    new_text_start_ = edit->new_text_start_;
    merge_type_ = DO_NOT_MERGE;
  }

  Type type_;

  // True if the edit can be marged.
  MergeType merge_type_;
  // Old cursor position.
  size_t old_cursor_pos_;
  // Deleted text by this edit.
  base::string16 old_text_;
  // The index of |old_text_|.
  size_t old_text_start_;
  // True if the deletion is made backward.
  bool delete_backward_;
  // New cursor position.
  size_t new_cursor_pos_;
  // Added text.
  base::string16 new_text_;
  // The index of |new_text_|
  size_t new_text_start_;

  DISALLOW_COPY_AND_ASSIGN(Edit);
};

class InsertEdit : public Edit {
 public:
  InsertEdit(bool mergeable, const base::string16& new_text, size_t at)
      : Edit(INSERT_EDIT,
             mergeable ? MERGEABLE : DO_NOT_MERGE,
             at  /* old cursor */,
             base::string16(),
             at,
             false  /* N/A */,
             at + new_text.length()  /* new cursor */,
             new_text,
             at) {
  }

  // Edit implementation.
  virtual bool DoMerge(const Edit* edit) OVERRIDE {
    if (edit->type() != INSERT_EDIT || new_text_end() != edit->new_text_start_)
      return false;
    // If continuous edit, merge it.
    // TODO(oshima): gtk splits edits between whitespace. Find out what
    // we want to here and implement if necessary.
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

class ReplaceEdit : public Edit {
 public:
  ReplaceEdit(MergeType merge_type,
              const base::string16& old_text,
              size_t old_cursor_pos,
              size_t old_text_start,
              bool backward,
              size_t new_cursor_pos,
              const base::string16& new_text,
              size_t new_text_start)
      : Edit(REPLACE_EDIT, merge_type,
             old_cursor_pos,
             old_text,
             old_text_start,
             backward,
             new_cursor_pos,
             new_text,
             new_text_start) {
  }

  // Edit implementation.
  virtual bool DoMerge(const Edit* edit) OVERRIDE {
    if (edit->type() == DELETE_EDIT ||
        new_text_end() != edit->old_text_start_ ||
        edit->old_text_start_ != edit->new_text_start_)
      return false;
    old_text_ += edit->old_text_;
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

class DeleteEdit : public Edit {
 public:
  DeleteEdit(bool mergeable,
             const base::string16& text,
             size_t text_start,
             bool backward)
      : Edit(DELETE_EDIT,
             mergeable ? MERGEABLE : DO_NOT_MERGE,
             (backward ? text_start + text.length() : text_start),
             text,
             text_start,
             backward,
             text_start,
             base::string16(),
             text_start) {
  }

  // Edit implementation.
  virtual bool DoMerge(const Edit* edit) OVERRIDE {
    if (edit->type() != DELETE_EDIT)
      return false;

    if (delete_backward_) {
      // backspace can be merged only with backspace at the same position.
      if (!edit->delete_backward_ || old_text_start_ != edit->old_text_end())
        return false;
      old_text_start_ = edit->old_text_start_;
      old_text_ = edit->old_text_ + old_text_;
      new_cursor_pos_ = edit->new_cursor_pos_;
    } else {
      // delete can be merged only with delete at the same position.
      if (edit->delete_backward_ || old_text_start_ != edit->old_text_start_)
        return false;
      old_text_ += edit->old_text_;
    }
    return true;
  }
};

}  // namespace internal

namespace {

// Returns the first segment that is visually emphasized. Usually it's used for
// representing the target clause (on Windows). Returns an invalid range if
// there is no such a range.
gfx::Range GetFirstEmphasizedRange(const ui::CompositionText& composition) {
  for (size_t i = 0; i < composition.underlines.size(); ++i) {
    const ui::CompositionUnderline& underline = composition.underlines[i];
    if (underline.thick)
      return gfx::Range(underline.start_offset, underline.end_offset);
  }
  return gfx::Range::InvalidRange();
}

}  // namespace

using internal::Edit;
using internal::DeleteEdit;
using internal::InsertEdit;
using internal::ReplaceEdit;
using internal::MergeType;
using internal::DO_NOT_MERGE;
using internal::FORCE_MERGE;
using internal::MERGEABLE;

/////////////////////////////////////////////////////////////////
// TextfieldModel: public

TextfieldModel::Delegate::~Delegate() {}

TextfieldModel::TextfieldModel(Delegate* delegate)
    : delegate_(delegate),
      render_text_(gfx::RenderText::CreateInstance()),
      current_edit_(edit_history_.end()) {
}

TextfieldModel::~TextfieldModel() {
  ClearEditHistory();
  ClearComposition();
}

bool TextfieldModel::SetText(const base::string16& new_text) {
  bool changed = false;
  if (HasCompositionText()) {
    ConfirmCompositionText();
    changed = true;
  }
  if (text() != new_text) {
    if (changed)  // No need to remember composition.
      Undo();
    size_t old_cursor = GetCursorPosition();
    // SetText moves the cursor to the end.
    size_t new_cursor = new_text.length();
    SelectAll(false);
    // If there is a composition text, don't merge with previous edit.
    // Otherwise, force merge the edits.
    ExecuteAndRecordReplace(changed ? DO_NOT_MERGE : FORCE_MERGE,
                            old_cursor, new_cursor, new_text, 0U);
    render_text_->SetCursorPosition(new_cursor);
  }
  ClearSelection();
  return changed;
}

void TextfieldModel::Append(const base::string16& new_text) {
  if (HasCompositionText())
    ConfirmCompositionText();
  size_t save = GetCursorPosition();
  MoveCursor(gfx::LINE_BREAK,
             render_text_->GetVisualDirectionOfLogicalEnd(),
             false);
  InsertText(new_text);
  render_text_->SetCursorPosition(save);
  ClearSelection();
}

bool TextfieldModel::Delete() {
  if (HasCompositionText()) {
    // No undo/redo for composition text.
    CancelCompositionText();
    return true;
  }
  if (HasSelection()) {
    DeleteSelection();
    return true;
  }
  if (text().length() > GetCursorPosition()) {
    size_t cursor_position = GetCursorPosition();
    size_t next_grapheme_index = render_text_->IndexOfAdjacentGrapheme(
        cursor_position, gfx::CURSOR_FORWARD);
    ExecuteAndRecordDelete(gfx::Range(cursor_position, next_grapheme_index),
                           true);
    return true;
  }
  return false;
}

bool TextfieldModel::Backspace() {
  if (HasCompositionText()) {
    // No undo/redo for composition text.
    CancelCompositionText();
    return true;
  }
  if (HasSelection()) {
    DeleteSelection();
    return true;
  }
  size_t cursor_position = GetCursorPosition();
  if (cursor_position > 0) {
    // Delete one code point, which may be two UTF-16 words.
    size_t previous_char = gfx::UTF16OffsetToIndex(text(), cursor_position, -1);
    ExecuteAndRecordDelete(gfx::Range(cursor_position, previous_char), true);
    return true;
  }
  return false;
}

size_t TextfieldModel::GetCursorPosition() const {
  return render_text_->cursor_position();
}

void TextfieldModel::MoveCursor(gfx::BreakType break_type,
                                gfx::VisualCursorDirection direction,
                                bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->MoveCursor(break_type, direction, select);
}

bool TextfieldModel::MoveCursorTo(const gfx::SelectionModel& cursor) {
  if (HasCompositionText()) {
    ConfirmCompositionText();
    // ConfirmCompositionText() updates cursor position. Need to reflect it in
    // the SelectionModel parameter of MoveCursorTo().
    gfx::Range range(render_text_->selection().start(), cursor.caret_pos());
    if (!range.is_empty())
      return render_text_->SelectRange(range);
    return render_text_->MoveCursorTo(
        gfx::SelectionModel(cursor.caret_pos(), cursor.caret_affinity()));
  }
  return render_text_->MoveCursorTo(cursor);
}

bool TextfieldModel::MoveCursorTo(const gfx::Point& point, bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
  gfx::SelectionModel cursor = render_text_->FindCursorPosition(point);
  if (select)
    cursor.set_selection_start(render_text_->selection().start());
  return render_text_->MoveCursorTo(cursor);
}

base::string16 TextfieldModel::GetSelectedText() const {
  return text().substr(render_text_->selection().GetMin(),
                       render_text_->selection().length());
}

void TextfieldModel::SelectRange(const gfx::Range& range) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SelectRange(range);
}

void TextfieldModel::SelectSelectionModel(const gfx::SelectionModel& sel) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->MoveCursorTo(sel);
}

void TextfieldModel::SelectAll(bool reversed) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SelectAll(reversed);
}

void TextfieldModel::SelectWord() {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SelectWord();
}

void TextfieldModel::ClearSelection() {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->ClearSelection();
}

bool TextfieldModel::CanUndo() {
  return edit_history_.size() && current_edit_ != edit_history_.end();
}

bool TextfieldModel::CanRedo() {
  if (!edit_history_.size())
    return false;
  // There is no redo iff the current edit is the last element in the history.
  EditHistory::iterator iter = current_edit_;
  return iter == edit_history_.end() || // at the top.
      ++iter != edit_history_.end();
}

bool TextfieldModel::Undo() {
  if (!CanUndo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText())
    CancelCompositionText();

  base::string16 old = text();
  size_t old_cursor = GetCursorPosition();
  (*current_edit_)->Commit();
  (*current_edit_)->Undo(this);

  if (current_edit_ == edit_history_.begin())
    current_edit_ = edit_history_.end();
  else
    current_edit_--;
  return old != text() || old_cursor != GetCursorPosition();
}

bool TextfieldModel::Redo() {
  if (!CanRedo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText())
    CancelCompositionText();

  if (current_edit_ == edit_history_.end())
    current_edit_ = edit_history_.begin();
  else
    current_edit_ ++;
  base::string16 old = text();
  size_t old_cursor = GetCursorPosition();
  (*current_edit_)->Redo(this);
  return old != text() || old_cursor != GetCursorPosition();
}

bool TextfieldModel::Cut() {
  if (!HasCompositionText() && HasSelection() && !render_text_->obscured()) {
    ui::ScopedClipboardWriter(
        ui::Clipboard::GetForCurrentThread(),
        ui::CLIPBOARD_TYPE_COPY_PASTE).WriteText(GetSelectedText());
    // A trick to let undo/redo handle cursor correctly.
    // Undoing CUT moves the cursor to the end of the change rather
    // than beginning, unlike Delete/Backspace.
    // TODO(oshima): Change Delete/Backspace to use DeleteSelection,
    // update DeleteEdit and remove this trick.
    const gfx::Range& selection = render_text_->selection();
    render_text_->SelectRange(gfx::Range(selection.end(), selection.start()));
    DeleteSelection();
    return true;
  }
  return false;
}

bool TextfieldModel::Copy() {
  if (!HasCompositionText() && HasSelection() && !render_text_->obscured()) {
    ui::ScopedClipboardWriter(
        ui::Clipboard::GetForCurrentThread(),
        ui::CLIPBOARD_TYPE_COPY_PASTE).WriteText(GetSelectedText());
    return true;
  }
  return false;
}

bool TextfieldModel::Paste() {
  base::string16 result;
  ui::Clipboard::GetForCurrentThread()->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE,
                                                 &result);
  if (result.empty())
    return false;

  InsertTextInternal(result, false);
  return true;
}

bool TextfieldModel::HasSelection() const {
  return !render_text_->selection().is_empty();
}

void TextfieldModel::DeleteSelection() {
  DCHECK(!HasCompositionText());
  DCHECK(HasSelection());
  ExecuteAndRecordDelete(render_text_->selection(), false);
}

void TextfieldModel::DeleteSelectionAndInsertTextAt(
    const base::string16& new_text,
    size_t position) {
  if (HasCompositionText())
    CancelCompositionText();
  ExecuteAndRecordReplace(DO_NOT_MERGE,
                          GetCursorPosition(),
                          position + new_text.length(),
                          new_text,
                          position);
}

base::string16 TextfieldModel::GetTextFromRange(const gfx::Range& range) const {
  if (range.IsValid() && range.GetMin() < text().length())
    return text().substr(range.GetMin(), range.length());
  return base::string16();
}

void TextfieldModel::GetTextRange(gfx::Range* range) const {
  *range = gfx::Range(0, text().length());
}

void TextfieldModel::SetCompositionText(
    const ui::CompositionText& composition) {
  if (HasCompositionText())
    CancelCompositionText();
  else if (HasSelection())
    DeleteSelection();

  if (composition.text.empty())
    return;

  size_t cursor = GetCursorPosition();
  base::string16 new_text = text();
  render_text_->SetText(new_text.insert(cursor, composition.text));
  gfx::Range range(cursor, cursor + composition.text.length());
  render_text_->SetCompositionRange(range);
  gfx::Range emphasized_range = GetFirstEmphasizedRange(composition);
  if (emphasized_range.IsValid()) {
    // This is a workaround due to the lack of support in RenderText to draw
    // a thick underline. In a composition returned from an IME, the segment
    // emphasized by a thick underline usually represents the target clause.
    // Because the target clause is more important than the actual selection
    // range (or caret position) in the composition here we use a selection-like
    // marker instead to show this range.
    // TODO(yukawa, msw): Support thick underlines and remove this workaround.
    render_text_->SelectRange(gfx::Range(
        cursor + emphasized_range.GetMin(),
        cursor + emphasized_range.GetMax()));
  } else if (!composition.selection.is_empty()) {
    render_text_->SelectRange(gfx::Range(
        cursor + composition.selection.GetMin(),
        cursor + composition.selection.GetMax()));
  } else {
    render_text_->SetCursorPosition(cursor + composition.selection.end());
  }
}

void TextfieldModel::ConfirmCompositionText() {
  DCHECK(HasCompositionText());
  gfx::Range range = render_text_->GetCompositionRange();
  base::string16 composition = text().substr(range.start(), range.length());
  // TODO(oshima): current behavior on ChromeOS is a bit weird and not
  // sure exactly how this should work. Find out and fix if necessary.
  AddOrMergeEditHistory(new InsertEdit(false, composition, range.start()));
  render_text_->SetCursorPosition(range.end());
  ClearComposition();
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldModel::CancelCompositionText() {
  DCHECK(HasCompositionText());
  gfx::Range range = render_text_->GetCompositionRange();
  ClearComposition();
  base::string16 new_text = text();
  render_text_->SetText(new_text.erase(range.start(), range.length()));
  render_text_->SetCursorPosition(range.start());
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldModel::ClearComposition() {
  render_text_->SetCompositionRange(gfx::Range::InvalidRange());
}

void TextfieldModel::GetCompositionTextRange(gfx::Range* range) const {
  *range = gfx::Range(render_text_->GetCompositionRange());
}

bool TextfieldModel::HasCompositionText() const {
  return !render_text_->GetCompositionRange().is_empty();
}

void TextfieldModel::ClearEditHistory() {
  STLDeleteElements(&edit_history_);
  current_edit_ = edit_history_.end();
}

/////////////////////////////////////////////////////////////////
// TextfieldModel: private

void TextfieldModel::InsertTextInternal(const base::string16& new_text,
                                        bool mergeable) {
  if (HasCompositionText()) {
    CancelCompositionText();
    ExecuteAndRecordInsert(new_text, mergeable);
  } else if (HasSelection()) {
    ExecuteAndRecordReplaceSelection(mergeable ? MERGEABLE : DO_NOT_MERGE,
                                     new_text);
  } else {
    ExecuteAndRecordInsert(new_text, mergeable);
  }
}

void TextfieldModel::ReplaceTextInternal(const base::string16& new_text,
                                         bool mergeable) {
  if (HasCompositionText()) {
    CancelCompositionText();
  } else if (!HasSelection()) {
    size_t cursor = GetCursorPosition();
    const gfx::SelectionModel& model = render_text_->selection_model();
    // When there is no selection, the default is to replace the next grapheme
    // with |new_text|. So, need to find the index of next grapheme first.
    size_t next =
        render_text_->IndexOfAdjacentGrapheme(cursor, gfx::CURSOR_FORWARD);
    if (next == model.caret_pos())
      render_text_->MoveCursorTo(model);
    else
      render_text_->SelectRange(gfx::Range(next, model.caret_pos()));
  }
  // Edit history is recorded in InsertText.
  InsertTextInternal(new_text, mergeable);
}

void TextfieldModel::ClearRedoHistory() {
  if (edit_history_.begin() == edit_history_.end())
    return;
  if (current_edit_ == edit_history_.end()) {
    ClearEditHistory();
    return;
  }
  EditHistory::iterator delete_start = current_edit_;
  delete_start++;
  STLDeleteContainerPointers(delete_start, edit_history_.end());
  edit_history_.erase(delete_start, edit_history_.end());
}

void TextfieldModel::ExecuteAndRecordDelete(gfx::Range range, bool mergeable) {
  size_t old_text_start = range.GetMin();
  const base::string16 old_text = text().substr(old_text_start, range.length());
  bool backward = range.is_reversed();
  Edit* edit = new DeleteEdit(mergeable, old_text, old_text_start, backward);
  bool delete_edit = AddOrMergeEditHistory(edit);
  edit->Redo(this);
  if (delete_edit)
    delete edit;
}

void TextfieldModel::ExecuteAndRecordReplaceSelection(
    MergeType merge_type,
    const base::string16& new_text) {
  size_t new_text_start = render_text_->selection().GetMin();
  size_t new_cursor_pos = new_text_start + new_text.length();
  ExecuteAndRecordReplace(merge_type,
                          GetCursorPosition(),
                          new_cursor_pos,
                          new_text,
                          new_text_start);
}

void TextfieldModel::ExecuteAndRecordReplace(MergeType merge_type,
                                             size_t old_cursor_pos,
                                             size_t new_cursor_pos,
                                             const base::string16& new_text,
                                             size_t new_text_start) {
  size_t old_text_start = render_text_->selection().GetMin();
  bool backward = render_text_->selection().is_reversed();
  Edit* edit = new ReplaceEdit(merge_type,
                               GetSelectedText(),
                               old_cursor_pos,
                               old_text_start,
                               backward,
                               new_cursor_pos,
                               new_text,
                               new_text_start);
  bool delete_edit = AddOrMergeEditHistory(edit);
  edit->Redo(this);
  if (delete_edit)
    delete edit;
}

void TextfieldModel::ExecuteAndRecordInsert(const base::string16& new_text,
                                            bool mergeable) {
  Edit* edit = new InsertEdit(mergeable, new_text, GetCursorPosition());
  bool delete_edit = AddOrMergeEditHistory(edit);
  edit->Redo(this);
  if (delete_edit)
    delete edit;
}

bool TextfieldModel::AddOrMergeEditHistory(Edit* edit) {
  ClearRedoHistory();

  if (current_edit_ != edit_history_.end() && (*current_edit_)->Merge(edit)) {
    // If a current edit exists and has been merged with a new edit, don't add
    // to the history, and return true to delete |edit| after redo.
    return true;
  }
  edit_history_.push_back(edit);
  if (current_edit_ == edit_history_.end()) {
    // If there is no redoable edit, this is the 1st edit because RedoHistory
    // has been already deleted.
    DCHECK_EQ(1u, edit_history_.size());
    current_edit_ = edit_history_.begin();
  } else {
    current_edit_++;
  }
  return false;
}

void TextfieldModel::ModifyText(size_t delete_from,
                                size_t delete_to,
                                const base::string16& new_text,
                                size_t new_text_insert_at,
                                size_t new_cursor_pos) {
  DCHECK_LE(delete_from, delete_to);
  base::string16 old_text = text();
  ClearComposition();
  if (delete_from != delete_to)
    render_text_->SetText(old_text.erase(delete_from, delete_to - delete_from));
  if (!new_text.empty())
    render_text_->SetText(old_text.insert(new_text_insert_at, new_text));
  render_text_->SetCursorPosition(new_cursor_pos);
  // TODO(oshima): Select text that was just undone, like Mac (but not GTK).
}

}  // namespace views
