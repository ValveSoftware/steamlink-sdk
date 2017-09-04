/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IndentOutdentCommand_h
#define IndentOutdentCommand_h

#include "core/editing/commands/ApplyBlockElementCommand.h"

namespace blink {

class CORE_EXPORT IndentOutdentCommand final : public ApplyBlockElementCommand {
 public:
  enum EIndentType { Indent, Outdent };
  static IndentOutdentCommand* create(Document& document, EIndentType type) {
    return new IndentOutdentCommand(document, type);
  }

  bool preservesTypingStyle() const override { return true; }

 private:
  IndentOutdentCommand(Document&, EIndentType);

  InputEvent::InputType inputType() const override;

  void outdentRegion(const VisiblePosition&,
                     const VisiblePosition&,
                     EditingState*);
  void outdentParagraph(EditingState*);
  bool tryIndentingAsListItem(const Position&, const Position&, EditingState*);
  void indentIntoBlockquote(const Position&,
                            const Position&,
                            HTMLElement*&,
                            EditingState*);

  void formatSelection(const VisiblePosition& startOfSelection,
                       const VisiblePosition& endOfSelection,
                       EditingState*) override;
  void formatRange(const Position& start,
                   const Position& end,
                   const Position& endOfSelection,
                   HTMLElement*& blockquoteForNextIndent,
                   EditingState*) override;

  EIndentType m_typeOfAction;
};

}  // namespace blink

#endif  // IndentOutdentCommand_h
