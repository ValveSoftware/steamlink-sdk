// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DragAndDropCommand_h
#define DragAndDropCommand_h

#include "core/editing/commands/CompositeEditCommand.h"

namespace blink {

// |DragAndDropCommand| is a dummy command. It doesn't do anything by itself,
// but will act as a catcher for the following |DeleteByDrag| and
// |InsertFromDrop| commands, and combine them into a single undo entry.
// In the future when necessary, this mechanism can be generalized into a common
// command wrapper to achieve undo group.
class DragAndDropCommand final : public CompositeEditCommand {
 public:
  static DragAndDropCommand* create(Document& document) {
    return new DragAndDropCommand(document);
  }

  bool isCommandGroupWrapper() const override;
  bool isDragAndDropCommand() const override;

 private:
  explicit DragAndDropCommand(Document&);

  void doApply(EditingState*) override;
  InputEvent::InputType inputType() const override;
};

}  // namespace blink

#endif  // DragAndDropCommand_h
