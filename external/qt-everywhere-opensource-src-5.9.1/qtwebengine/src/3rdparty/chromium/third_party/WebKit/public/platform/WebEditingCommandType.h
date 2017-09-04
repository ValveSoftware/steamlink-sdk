// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebEditingCommandType_h
#define WebEditingCommandType_h

namespace blink {

// Enum values are used in user metrics, do not modify. If you add new commands,
// you should assign new values and update MappedEditingCommands
// in tools/metrics/histograms/histograms.xml, and add the new command to
// EditorCommand.cpp.
enum class WebEditingCommandType {
  Invalid = 0,
  AlignJustified = 1,
  AlignLeft = 2,
  AlignRight = 3,
  BackColor = 4,
  BackwardDelete = 5,
  Bold = 6,
  Copy = 7,
  CreateLink = 8,
  Cut = 9,
  DefaultParagraphSeparator = 10,
  Delete = 11,
  DeleteBackward = 12,
  DeleteBackwardByDecomposingPreviousCharacter = 13,
  DeleteForward = 14,
  DeleteToBeginningOfLine = 15,
  DeleteToBeginningOfParagraph = 16,
  DeleteToEndOfLine = 17,
  DeleteToEndOfParagraph = 18,
  DeleteToMark = 19,
  DeleteWordBackward = 20,
  DeleteWordForward = 21,
  FindString = 22,
  FontName = 23,
  FontSize = 24,
  FontSizeDelta = 25,
  ForeColor = 26,
  FormatBlock = 27,
  ForwardDelete = 28,
  HiliteColor = 29,
  IgnoreSpelling = 30,
  Indent = 31,
  InsertBacktab = 32,
  InsertHTML = 33,
  InsertHorizontalRule = 34,
  InsertImage = 35,
  InsertLineBreak = 36,
  InsertNewline = 37,
  InsertNewlineInQuotedContent = 38,
  InsertOrderedList = 39,
  InsertParagraph = 40,
  InsertTab = 41,
  InsertText = 42,
  InsertUnorderedList = 43,
  Italic = 44,
  JustifyCenter = 45,
  JustifyFull = 46,
  JustifyLeft = 47,
  JustifyNone = 48,
  JustifyRight = 49,
  MakeTextWritingDirectionLeftToRight = 50,
  MakeTextWritingDirectionNatural = 51,
  MakeTextWritingDirectionRightToLeft = 52,
  MoveBackward = 53,
  MoveBackwardAndModifySelection = 54,
  MoveDown = 55,
  MoveDownAndModifySelection = 56,
  MoveForward = 57,
  MoveForwardAndModifySelection = 58,
  MoveLeft = 59,
  MoveLeftAndModifySelection = 60,
  MovePageDown = 61,
  MovePageDownAndModifySelection = 62,
  MovePageUp = 63,
  MovePageUpAndModifySelection = 64,
  MoveParagraphBackward = 65,
  MoveParagraphBackwardAndModifySelection = 66,
  MoveParagraphForward = 67,
  MoveParagraphForwardAndModifySelection = 68,
  MoveRight = 69,
  MoveRightAndModifySelection = 70,
  MoveToBeginningOfDocument = 71,
  MoveToBeginningOfDocumentAndModifySelection = 72,
  MoveToBeginningOfLine = 73,
  MoveToBeginningOfLineAndModifySelection = 74,
  MoveToBeginningOfParagraph = 75,
  MoveToBeginningOfParagraphAndModifySelection = 76,
  MoveToBeginningOfSentence = 77,
  MoveToBeginningOfSentenceAndModifySelection = 78,
  MoveToEndOfDocument = 79,
  MoveToEndOfDocumentAndModifySelection = 80,
  MoveToEndOfLine = 81,
  MoveToEndOfLineAndModifySelection = 82,
  MoveToEndOfParagraph = 83,
  MoveToEndOfParagraphAndModifySelection = 84,
  MoveToEndOfSentence = 85,
  MoveToEndOfSentenceAndModifySelection = 86,
  MoveToLeftEndOfLine = 87,
  MoveToLeftEndOfLineAndModifySelection = 88,
  MoveToRightEndOfLine = 89,
  MoveToRightEndOfLineAndModifySelection = 90,
  MoveUp = 91,
  MoveUpAndModifySelection = 92,
  MoveWordBackward = 93,
  MoveWordBackwardAndModifySelection = 94,
  MoveWordForward = 95,
  MoveWordForwardAndModifySelection = 96,
  MoveWordLeft = 97,
  MoveWordLeftAndModifySelection = 98,
  MoveWordRight = 99,
  MoveWordRightAndModifySelection = 100,
  Outdent = 101,
  OverWrite = 102,
  Paste = 103,
  PasteAndMatchStyle = 104,
  PasteGlobalSelection = 105,
  Print = 106,
  Redo = 107,
  RemoveFormat = 108,
  ScrollPageBackward = 109,
  ScrollPageForward = 110,
  ScrollLineUp = 111,
  ScrollLineDown = 112,
  ScrollToBeginningOfDocument = 113,
  ScrollToEndOfDocument = 114,
  SelectAll = 115,
  SelectLine = 116,
  SelectParagraph = 117,
  SelectSentence = 118,
  SelectToMark = 119,
  SelectWord = 120,
  SetMark = 121,
  Strikethrough = 122,
  StyleWithCSS = 123,
  Subscript = 124,
  Superscript = 125,
  SwapWithMark = 126,
  ToggleBold = 127,
  ToggleItalic = 128,
  ToggleUnderline = 129,
  Transpose = 130,
  Underline = 131,
  Undo = 132,
  Unlink = 133,
  Unscript = 134,
  Unselect = 135,
  UseCSS = 136,
  Yank = 137,
  YankAndSelect = 138,
  AlignCenter = 139,

  // Add new commands immediately above this line.
  NumberOfCommandTypes,

  // These unsupported commands are listed here since they appear in the
  // Microsoft documentation used as the starting point for our DOM
  // executeCommand support.
  //
  // 2D-Position (not supported)
  // AbsolutePosition (not supported)
  // BlockDirLTR (not supported)
  // BlockDirRTL (not supported)
  // BrowseMode (not supported)
  // ClearAuthenticationCache (not supported)
  // CreateBookmark (not supported)
  // DirLTR (not supported)
  // DirRTL (not supported)
  // EditMode (not supported)
  // InlineDirLTR (not supported)
  // InlineDirRTL (not supported)
  // InsertButton (not supported)
  // InsertFieldSet (not supported)
  // InsertIFrame (not supported)
  // InsertInputButton (not supported)
  // InsertInputCheckbox (not supported)
  // InsertInputFileUpload (not supported)
  // InsertInputHidden (not supported)
  // InsertInputImage (not supported)
  // InsertInputPassword (not supported)
  // InsertInputRadio (not supported)
  // InsertInputReset (not supported)
  // InsertInputSubmit (not supported)
  // InsertInputText (not supported)
  // InsertMarquee (not supported)
  // InsertSelectDropDown (not supported)
  // InsertSelectListBox (not supported)
  // InsertTextArea (not supported)
  // LiveResize (not supported)
  // MultipleSelection (not supported)
  // Open (not supported)
  // PlayImage (not supported)
  // Refresh (not supported)
  // RemoveParaFormat (not supported)
  // SaveAs (not supported)
  // SizeToControl (not supported)
  // SizeToControlHeight (not supported)
  // SizeToControlWidth (not supported)
  // Stop (not supported)
  // StopImage (not supported)
  // Unbookmark (not supported)
};

}  // namespace blink

#endif  // WebEditingCommandType_h
