// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/DragAndDropCommand.h"

namespace blink {

DragAndDropCommand::DragAndDropCommand(Document& document)
    : CompositeEditCommand(document) {}

bool DragAndDropCommand::isCommandGroupWrapper() const {
  return true;
}

bool DragAndDropCommand::isDragAndDropCommand() const {
  return true;
}

void DragAndDropCommand::doApply(EditingState*) {
  // Do nothing. Should only register undo entry after combined with other
  // commands.
}

InputEvent::InputType DragAndDropCommand::inputType() const {
  return InputEvent::InputType::None;
}

}  // namespace blink
