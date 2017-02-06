// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingTestBase.h"
#include "core/editing/Editor.h"
#include "core/editing/commands/EditorCommandNames.h"
#include "core/frame/LocalFrame.h"
#include "public/platform/WebEditingCommandType.h"
#include "wtf/StringExtras.h"

namespace blink {

namespace {

struct CommandNameEntry {
    const char* name;
    WebEditingCommandType type;
};

const CommandNameEntry kCommandNameEntries[] = {
#define V(name) { #name, WebEditingCommandType::name },
    FOR_EACH_BLINK_EDITING_COMMAND_NAME(V)
#undef V
};
// Test all commands except WebEditingCommandType::Invalid.
static_assert(arraysize(kCommandNameEntries) + 1 == static_cast<size_t>(WebEditingCommandType::NumberOfCommandTypes), "must test all valid WebEditingCommandType");

} // anonymous namespace

class EditingCommandTest : public EditingTestBase {
};

TEST_F(EditingCommandTest, EditorCommandOrder)
{
    for (size_t i = 1; i < arraysize(kCommandNameEntries); ++i)
        EXPECT_GT(0, strcasecmp(kCommandNameEntries[i - 1].name, kCommandNameEntries[i].name)) << "EDITOR_COMMAND_MAP must be case-folding ordered. Incorrect index:" << i;
}

TEST_F(EditingCommandTest, CreateCommandFromString)
{
    Editor& dummyEditor = document().frame()->editor();
    for (const auto& entry : kCommandNameEntries) {
        Editor::Command command = dummyEditor.createCommand(entry.name);
        EXPECT_EQ(static_cast<int>(entry.type), command.idForHistogram()) << entry.name;
    }
}

TEST_F(EditingCommandTest, CreateCommandFromStringCaseFolding)
{
    Editor& dummyEditor = document().frame()->editor();
    for (const auto& entry : kCommandNameEntries) {
        Editor::Command command = dummyEditor.createCommand(String(entry.name).lower());
        EXPECT_EQ(static_cast<int>(entry.type), command.idForHistogram()) << entry.name;
        command = dummyEditor.createCommand(String(entry.name).upper());
        EXPECT_EQ(static_cast<int>(entry.type), command.idForHistogram()) << entry.name;
    }
}

TEST_F(EditingCommandTest, CreateCommandFromInvalidString)
{
    const String kInvalidCommandName[] = {
        "",
        "iNvAlId",
        "12345",
    };
    Editor& dummyEditor = document().frame()->editor();
    for (const auto& commandName : kInvalidCommandName) {
        Editor::Command command = dummyEditor.createCommand(commandName);
        EXPECT_EQ(0, command.idForHistogram());
    }
}

} // namespace blink
