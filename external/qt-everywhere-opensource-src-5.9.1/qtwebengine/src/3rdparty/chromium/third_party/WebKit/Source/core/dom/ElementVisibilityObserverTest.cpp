// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementVisibilityObserver.h"

#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLDocument.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ElementVisibilityObserverTest : public ::testing::Test {
 protected:
  void SetUp() override { m_dummyPageHolder = DummyPageHolder::create(); }

  Document& document() { return m_dummyPageHolder->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(ElementVisibilityObserverTest, ObserveElementWithoutDocumentFrame) {
  HTMLElement* element = HTMLDivElement::create(
      *DOMImplementation::create(document())->createHTMLDocument("test"));
  ElementVisibilityObserver* observer =
      new ElementVisibilityObserver(element, nullptr);
  observer->start();
  observer->stop();
  // It should not crash.
}

}  // anonymous namespace

}  // blink namespace
