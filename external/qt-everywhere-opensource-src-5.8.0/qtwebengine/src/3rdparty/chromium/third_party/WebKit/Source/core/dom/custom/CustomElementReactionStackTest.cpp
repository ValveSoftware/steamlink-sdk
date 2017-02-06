// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReactionStack.h"

#include "core/dom/custom/CustomElementReaction.h"
#include "core/dom/custom/CustomElementReactionTestHelpers.h"
#include "core/dom/custom/CustomElementTestHelpers.h"
#include "core/html/HTMLDocument.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/AtomicString.h"
#include <initializer_list>
#include <vector>

namespace blink {

TEST(CustomElementReactionStackTest, one)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'a'}))
        << "popping the reaction stack should run reactions";
}

TEST(CustomElementReactionStackTest, multipleElements)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'a', 'b'}))
        << "reactions should run in the order the elements queued";
}

TEST(CustomElementReactionStackTest, popTopEmpty)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->push();
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>())
        << "popping the empty top-of-stack should not run any reactions";
}

TEST(CustomElementReactionStackTest, popTop)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->push();
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'b'}))
        << "popping the top-of-stack should only run top-of-stack reactions";
}

TEST(CustomElementReactionStackTest, requeueingDoesNotReorderElements)
{
    std::vector<char> log;

    Element* element = CreateElement("a");

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueueToCurrentQueue(element, new TestReaction({new Log('a', log)}));
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('z', log)}));
    stack->enqueueToCurrentQueue(element, new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'a', 'b', 'z'}))
        << "reactions should run together in the order elements were queued";
}

TEST(CustomElementReactionStackTest, oneReactionQueuePerElement)
{
    std::vector<char> log;

    Element* element = CreateElement("a");

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueueToCurrentQueue(element, new TestReaction({new Log('a', log)}));
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('z', log)}));
    stack->push();
    stack->enqueueToCurrentQueue(CreateElement("a"), new TestReaction({new Log('y', log)}));
    stack->enqueueToCurrentQueue(element, new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'y', 'a', 'b'}))
        << "reactions should run together in the order elements were queued";

    log.clear();
    stack->popInvokingReactions();
    EXPECT_EQ(log, std::vector<char>({'z'})) << "reactions should be run once";
}

class EnqueueToStack : public Command {
    WTF_MAKE_NONCOPYABLE(EnqueueToStack);
public:
    EnqueueToStack(CustomElementReactionStack* stack, Element* element, CustomElementReaction* reaction)
        : m_stack(stack)
        , m_element(element)
        , m_reaction(reaction)
    {
    }
    ~EnqueueToStack() override = default;
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        Command::trace(visitor);
        visitor->trace(m_stack);
        visitor->trace(m_element);
        visitor->trace(m_reaction);
    }
    void run(Element*) override
    {
        m_stack->enqueueToCurrentQueue(m_element, m_reaction);
    }
private:
    Member<CustomElementReactionStack> m_stack;
    Member<Element> m_element;
    Member<CustomElementReaction> m_reaction;
};

TEST(CustomElementReactionStackTest, enqueueFromReaction)
{
    std::vector<char> log;

    Element* element = CreateElement("a");

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueueToCurrentQueue(element, new TestReaction({
        new EnqueueToStack(stack, element,
            new TestReaction({ new Log('a', log) }) )
    }));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({ 'a' })) << "enqueued reaction from another reaction should run in the same invoke";
}

} // namespace blink
