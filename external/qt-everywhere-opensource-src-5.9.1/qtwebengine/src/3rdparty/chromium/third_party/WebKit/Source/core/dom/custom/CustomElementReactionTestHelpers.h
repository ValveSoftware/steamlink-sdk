// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReaction.h"

#include "core/dom/custom/CustomElementReactionQueue.h"
#include "core/dom/custom/CustomElementReactionStack.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include <initializer_list>
#include <memory>
#include <vector>

namespace blink {

class Element;

class Command : public GarbageCollectedFinalized<Command> {
  WTF_MAKE_NONCOPYABLE(Command);

 public:
  Command() = default;
  virtual ~Command() = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {}
  virtual void run(Element*) = 0;
};

class Call : public Command {
  WTF_MAKE_NONCOPYABLE(Call);

 public:
  using Callback = WTF::Function<void(Element*)>;
  Call(std::unique_ptr<Callback> callback) : m_callback(std::move(callback)) {}
  ~Call() override = default;
  void run(Element* element) override { m_callback->operator()(element); }

 private:
  std::unique_ptr<Callback> m_callback;
};

class Unreached : public Command {
  WTF_MAKE_NONCOPYABLE(Unreached);

 public:
  Unreached(const char* message) : m_message(message) {}
  ~Unreached() override = default;
  void run(Element*) override { EXPECT_TRUE(false) << m_message; }

 private:
  const char* m_message;
};

class Log : public Command {
  WTF_MAKE_NONCOPYABLE(Log);

 public:
  Log(char what, std::vector<char>& where) : m_what(what), m_where(where) {}
  ~Log() override = default;
  void run(Element*) override { m_where.push_back(m_what); }

 private:
  char m_what;
  std::vector<char>& m_where;
};

class Recurse : public Command {
  WTF_MAKE_NONCOPYABLE(Recurse);

 public:
  Recurse(CustomElementReactionQueue* queue) : m_queue(queue) {}
  ~Recurse() override = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {
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
      : m_queue(queue), m_reaction(reaction) {}
  ~Enqueue() override = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {
    Command::trace(visitor);
    visitor->trace(m_queue);
    visitor->trace(m_reaction);
  }
  void run(Element*) override { m_queue->add(m_reaction); }

 private:
  Member<CustomElementReactionQueue> m_queue;
  Member<CustomElementReaction> m_reaction;
};

class TestReaction : public CustomElementReaction {
  WTF_MAKE_NONCOPYABLE(TestReaction);

 public:
  TestReaction(std::initializer_list<Command*> commands)
      : CustomElementReaction(nullptr) {
    // TODO(dominicc): Simply pass the initializer list when
    // HeapVector supports initializer lists like Vector.
    for (auto& command : commands)
      m_commands.append(command);
  }
  ~TestReaction() override = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {
    CustomElementReaction::trace(visitor);
    visitor->trace(m_commands);
  }
  void invoke(Element* element) override {
    for (auto& command : m_commands)
      command->run(element);
  }

 private:
  HeapVector<Member<Command>> m_commands;
};

class ResetCustomElementReactionStackForTest final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ResetCustomElementReactionStackForTest);

 public:
  ResetCustomElementReactionStackForTest()
      : m_stack(new CustomElementReactionStack),
        m_oldStack(
            CustomElementReactionStackTestSupport::setCurrentForTest(m_stack)) {
  }

  ~ResetCustomElementReactionStackForTest() {
    CustomElementReactionStackTestSupport::setCurrentForTest(m_oldStack);
  }

  CustomElementReactionStack& stack() { return *m_stack; }

 private:
  Member<CustomElementReactionStack> m_stack;
  Member<CustomElementReactionStack> m_oldStack;
};

}  // namespace blink
