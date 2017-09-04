// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/PasswordInputType.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLInputElement.h"
#include "core/testing/DummyPageHolder.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/modules/sensitive_input_visibility/sensitive_input_visibility_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockInterfaceProvider : public blink::InterfaceProvider {
 public:
  MockInterfaceProvider()
      : m_mockSensitiveInputVisibilityService(this),
        m_passwordFieldVisibleCalled(false),
        m_numPasswordFieldsInvisibleCalls(0) {}

  virtual ~MockInterfaceProvider() {}

  void getInterface(const char* name,
                    mojo::ScopedMessagePipeHandle handle) override {
    m_mockSensitiveInputVisibilityService.bindRequest(
        mojo::MakeRequest<mojom::blink::SensitiveInputVisibilityService>(
            std::move(handle)));
  }

  void setPasswordFieldVisibleCalled() { m_passwordFieldVisibleCalled = true; }

  bool passwordFieldVisibleCalled() const {
    return m_passwordFieldVisibleCalled;
  }

  void incrementPasswordFieldsInvisibleCalled() {
    ++m_numPasswordFieldsInvisibleCalls;
  }

  unsigned numPasswordFieldsInvisibleCalls() const {
    return m_numPasswordFieldsInvisibleCalls;
  }

 private:
  class MockSensitiveInputVisibilityService
      : public mojom::blink::SensitiveInputVisibilityService {
   public:
    explicit MockSensitiveInputVisibilityService(
        MockInterfaceProvider* registry)
        : m_registry(registry) {}
    ~MockSensitiveInputVisibilityService() override {}

    void bindRequest(
        mojom::blink::SensitiveInputVisibilityServiceRequest request) {
      m_bindingSet.AddBinding(this, std::move(request));
    }

   private:
    // mojom::SensitiveInputVisibilityService
    void PasswordFieldVisibleInInsecureContext() override {
      m_registry->setPasswordFieldVisibleCalled();
    }

    void AllPasswordFieldsInInsecureContextInvisible() override {
      m_registry->incrementPasswordFieldsInvisibleCalled();
    }

    mojo::BindingSet<SensitiveInputVisibilityService> m_bindingSet;
    MockInterfaceProvider* const m_registry;
  };

  MockSensitiveInputVisibilityService m_mockSensitiveInputVisibilityService;
  bool m_passwordFieldVisibleCalled;
  unsigned m_numPasswordFieldsInvisibleCalls;
};

// Tests that a Mojo message is sent when a visible password field
// appears on the page.
TEST(PasswordInputTypeTest, PasswordVisibilityEvent) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML("<input type='password'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_TRUE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is not sent when a visible password field
// appears in a secure context.
TEST(PasswordInputTypeTest, PasswordVisibilityEventInSecureContext) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().setURL(KURL(KURL(), "https://example.test"));
  pageHolder->document().setSecurityOrigin(
      SecurityOrigin::create(KURL(KURL(), "https://example.test")));
  pageHolder->document().body()->setInnerHTML("<input type='password'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  // No message should have been sent from a secure context.
  blink::testing::runPendingTasks();
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is sent when a previously invisible password field
// becomes visible.
TEST(PasswordInputTypeTest, InvisiblePasswordFieldBecomesVisible) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML(
      "<input type='password' style='display:none;'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  // The message should not be sent for a hidden password field.
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());

  // Now make the input visible.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_TRUE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is sent when a previously non-password field
// becomes a password.
TEST(PasswordInputTypeTest, NonPasswordFieldBecomesPassword) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML("<input type='text'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  // The message should not be sent for a non-password field.
  blink::testing::runPendingTasks();
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());

  // Now make the input a password field.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setType("password");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_TRUE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is *not* sent when a previously invisible password
// field becomes a visible non-password field.
TEST(PasswordInputTypeTest,
     InvisiblePasswordFieldBecomesVisibleNonPasswordField) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML(
      "<input type='password' style='display:none;'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  // The message should not be sent for a hidden password field.
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());

  // Now make the input a visible non-password field.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setType("text");
  input->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is sent when the only visible password
// field becomes invisible.
TEST(PasswordInputTypeTest, VisiblePasswordFieldBecomesInvisible) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML("<input type='password'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_TRUE(interfaceProvider.passwordFieldVisibleCalled());
  EXPECT_EQ(0u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // Now make the input invisible.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setAttribute("style", "display:none;", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(1u, interfaceProvider.numPasswordFieldsInvisibleCalls());
}

// Tests that a Mojo message is sent when all visible password fields
// become invisible.
TEST(PasswordInputTypeTest, AllVisiblePasswordFieldBecomeInvisible) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML(
      "<input type='password'><input type='password'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(0u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // Make the first input invisible. There should be no message because
  // there is still a visible input.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setAttribute("style", "display:none;", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(0u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // When all inputs are invisible, then a message should be sent.
  input = toHTMLInputElement(pageHolder->document().body()->lastChild());
  input->setAttribute("style", "display:none;", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(1u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // If the count of visible inputs goes positive again and then back to
  // zero, a message should be sent again.
  input->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(1u, interfaceProvider.numPasswordFieldsInvisibleCalls());
  input->setAttribute("style", "display:none;", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(2u, interfaceProvider.numPasswordFieldsInvisibleCalls());
}

// Tests that a Mojo message is sent when the containing element of a
// visible password field becomes invisible.
TEST(PasswordInputTypeTest, PasswordFieldContainerBecomesInvisible) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML(
      "<div><input type='password'></div>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(0u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // If the containing div becomes invisible, a message should be sent.
  HTMLElement* div =
      toHTMLDivElement(pageHolder->document().body()->firstChild());
  div->setAttribute("style", "display:none;", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(1u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // If the containing div becomes visible and then invisible again, a message
  // should be sent.
  div->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(1u, interfaceProvider.numPasswordFieldsInvisibleCalls());
  div->setAttribute("style", "display:none;", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(2u, interfaceProvider.numPasswordFieldsInvisibleCalls());
}

// Tests that a Mojo message is sent when all visible password fields
// become non-password fields.
TEST(PasswordInputTypeTest, PasswordFieldsBecomeNonPasswordFields) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML(
      "<input type='password'><input type='password'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(0u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // Make the first input a non-password input. There should be no
  // message because there is still a visible password input.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setType("text");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(0u, interfaceProvider.numPasswordFieldsInvisibleCalls());

  // When all inputs are no longer passwords, then a message should be sent.
  input = toHTMLInputElement(pageHolder->document().body()->lastChild());
  input->setType("text");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_EQ(1u, interfaceProvider.numPasswordFieldsInvisibleCalls());
}

// Tests that only one Mojo message is sent for multiple password
// visibility events within the same task.
TEST(PasswordInputTypeTest, MultipleEventsInSameTask) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML("<input type='password'>");
  pageHolder->document().view()->updateAllLifecyclePhases();
  // Make the password field invisible in the same task.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setType("text");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  // Only a single Mojo message should have been sent, with the latest state of
  // the page (which is that no password fields are visible).
  EXPECT_EQ(1u, interfaceProvider.numPasswordFieldsInvisibleCalls());
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());
}

}  // namespace blink
