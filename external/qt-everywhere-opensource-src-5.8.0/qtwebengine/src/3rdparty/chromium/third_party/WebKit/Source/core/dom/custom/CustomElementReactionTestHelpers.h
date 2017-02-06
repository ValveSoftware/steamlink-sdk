// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReaction.h"

#include "core/dom/custom/CustomElementReactionQueue.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include <initializer_list>
#include <vector>

namespace blink {

class Element;

class Command : public GarbageCollectedFinalized<Command> {
    WTF_MAKE_NONCOPYABLE(Command);
public:
    Command() = default;
    virtual ~Command() = default;
    DEFINE_INLINE_VIRTUAL_TRACE() { }
    virtual void run(Element*) = 0;
};

class Log : public Command {
    WTF_MAKE_NONCOPYABLE(Log);
public:
    Log(char what, std::vector<char>& where) : m_what(what), m_where(where) { }
    ~Log() override = default;
    void run(Element*) override { m_where.push_back(m_what); }
private:
    char m_what;
    std::vector<char>& m_where;
};

class Recurse : public Command {
    WTF_MAKE_NONCOPYABLE(Recurse);
public:
    Recurse(CustomElementReactionQueue* queue) : m_queue(queue) { }
    ~Recurse() override = default;
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        Command::trace(visitor);
        visitor->trace(m_queue);
    }
    void run(Element* element) override { m_queue->invokeReactions(element); }
private:
    Member<CustomElementReactionQueue> m_queue;
};

class Enqueue : public Command {
    WTF_MAKE_NONCOPYABLE(Enqueue);
public:
    Enqueue(CustomElementReactionQueue* queue, CustomElementReaction* reaction)
        : m_queue(queue)
        , m_reaction(reaction)
    {
    }
    ~Enqueue() override = default;
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        Command::trace(visitor);
        visitor->trace(m_queue);
        visitor->trace(m_reaction);
    }
    void run(Element*) override
    {
        m_queue->add(m_reaction);
    }
private:
    Member<CustomElementReactionQueue> m_queue;
    Member<CustomElementReaction> m_reaction;
};

class TestReaction : public CustomElementReaction {
    WTF_MAKE_NONCOPYABLE(TestReaction);
public:
    TestReaction(std::initializer_list<Command*> commands)
        : CustomElementReaction(nullptr)
    {
        // TODO(dominicc): Simply pass the initializer list when
        // HeapVector supports initializer lists like Vector.
        for (auto& command : commands)
            m_commands.append(command);
    }
    ~TestReaction() override = default;
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        CustomElementReaction::trace(visitor);
        visitor->trace(m_commands);
    }
    void invoke(Element* element) override
    {
        for (auto& command : m_commands)
            command->run(element);
    }

private:
    HeapVector<Member<Command>> m_commands;
};

} // namespace blink
