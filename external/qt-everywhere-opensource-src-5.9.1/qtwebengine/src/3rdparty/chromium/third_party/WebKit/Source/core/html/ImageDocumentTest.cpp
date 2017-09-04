// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/ImageDocument.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/frame/Settings.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// An image of size 50x50.
Vector<unsigned char> jpegImage() {
  Vector<unsigned char> jpeg;

  static const unsigned char data[] = {
      0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
      0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
      0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0x43, 0x01, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x32, 0x00, 0x32, 0x03,
      0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
      0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x15, 0x01, 0x01, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x02, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03,
      0x11, 0x00, 0x3f, 0x00, 0x00, 0x94, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xd9};

  jpeg.append(data, sizeof(data));
  return jpeg;
}
}

class WindowToViewportScalingChromeClient : public EmptyChromeClient {
 public:
  WindowToViewportScalingChromeClient()
      : EmptyChromeClient(), m_scaleFactor(1.f) {}

  void setScalingFactor(float s) { m_scaleFactor = s; }
  float windowToViewportScalar(const float s) const override {
    return s * m_scaleFactor;
  }

 private:
  float m_scaleFactor;
};

class ImageDocumentTest : public ::testing::Test {
 protected:
  void TearDown() override { ThreadState::current()->collectAllGarbage(); }

  void createDocumentWithoutLoadingImage(int viewWidth, int viewHeight);
  void createDocument(int viewWidth, int viewHeight);
  void loadImage();

  ImageDocument& document() const;

  int imageWidth() const { return document().imageElement()->width(); }
  int imageHeight() const { return document().imageElement()->height(); }

  void setPageZoom(float);
  void setWindowToViewportScalingFactor(float);

 private:
  Persistent<WindowToViewportScalingChromeClient> m_chromeClient;
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void ImageDocumentTest::createDocumentWithoutLoadingImage(int viewWidth,
                                                          int viewHeight) {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  m_chromeClient = new WindowToViewportScalingChromeClient();
  pageClients.chromeClient = m_chromeClient;
  m_dummyPageHolder =
      DummyPageHolder::create(IntSize(viewWidth, viewHeight), &pageClients);

  LocalFrame& frame = m_dummyPageHolder->frame();
  frame.document()->shutdown();
  DocumentInit init(KURL(), &frame);
  frame.localDOMWindow()->installNewDocument("image/jpeg", init);
}

void ImageDocumentTest::createDocument(int viewWidth, int viewHeight) {
  createDocumentWithoutLoadingImage(viewWidth, viewHeight);
  loadImage();
}

ImageDocument& ImageDocumentTest::document() const {
  Document* document = m_dummyPageHolder->frame().domWindow()->document();
  ImageDocument* imageDocument = static_cast<ImageDocument*>(document);
  return *imageDocument;
}

void ImageDocumentTest::loadImage() {
  DocumentParser* parser = document().implicitOpen(
      ParserSynchronizationPolicy::ForceSynchronousParsing);
  const Vector<unsigned char>& data = jpegImage();
  parser->appendBytes(reinterpret_cast<const char*>(data.data()), data.size());
  parser->finish();
}

void ImageDocumentTest::setPageZoom(float factor) {
  m_dummyPageHolder->frame().setPageZoomFactor(factor);
}

void ImageDocumentTest::setWindowToViewportScalingFactor(float factor) {
  m_chromeClient->setScalingFactor(factor);
}

TEST_F(ImageDocumentTest, ImageLoad) {
  createDocument(50, 50);
  EXPECT_EQ(50, imageWidth());
  EXPECT_EQ(50, imageHeight());
}

TEST_F(ImageDocumentTest, LargeImageScalesDown) {
  createDocument(25, 30);
  EXPECT_EQ(25, imageWidth());
  EXPECT_EQ(25, imageHeight());

  createDocument(35, 20);
  EXPECT_EQ(20, imageWidth());
  EXPECT_EQ(20, imageHeight());
}

TEST_F(ImageDocumentTest, RestoreImageOnClick) {
  createDocument(30, 40);
  document().imageClicked(4, 4);
  EXPECT_EQ(50, imageWidth());
  EXPECT_EQ(50, imageHeight());
}

TEST_F(ImageDocumentTest, InitialZoomDoesNotAffectScreenFit) {
  createDocumentWithoutLoadingImage(20, 10);
  setPageZoom(2.f);
  loadImage();
  EXPECT_EQ(10, imageWidth());
  EXPECT_EQ(10, imageHeight());
  document().imageClicked(4, 4);
  EXPECT_EQ(50, imageWidth());
  EXPECT_EQ(50, imageHeight());
}

TEST_F(ImageDocumentTest, ZoomingDoesNotChangeRelativeSize) {
  createDocument(75, 75);
  setPageZoom(0.5f);
  document().windowSizeChanged();
  EXPECT_EQ(50, imageWidth());
  EXPECT_EQ(50, imageHeight());
  setPageZoom(2.f);
  document().windowSizeChanged();
  EXPECT_EQ(50, imageWidth());
  EXPECT_EQ(50, imageHeight());
}

TEST_F(ImageDocumentTest, ImageScalesDownWithDsf) {
  createDocumentWithoutLoadingImage(20, 30);
  setWindowToViewportScalingFactor(2.f);
  loadImage();
  EXPECT_EQ(10, imageWidth());
  EXPECT_EQ(10, imageHeight());
}

TEST_F(ImageDocumentTest, ImageNotCenteredWithForceZeroLayoutHeight) {
  createDocumentWithoutLoadingImage(80, 70);
  document().page()->settings().setForceZeroLayoutHeight(true);
  loadImage();
  EXPECT_FALSE(document().shouldShrinkToFit());
  EXPECT_EQ(0, document().imageElement()->offsetLeft());
  EXPECT_EQ(0, document().imageElement()->offsetTop());
  EXPECT_EQ(50, imageWidth());
  EXPECT_EQ(50, imageHeight());
}

TEST_F(ImageDocumentTest, ImageCenteredWithoutForceZeroLayoutHeight) {
  createDocumentWithoutLoadingImage(80, 70);
  document().page()->settings().setForceZeroLayoutHeight(false);
  loadImage();
  EXPECT_TRUE(document().shouldShrinkToFit());
  EXPECT_EQ(15, document().imageElement()->offsetLeft());
  EXPECT_EQ(10, document().imageElement()->offsetTop());
  EXPECT_EQ(50, imageWidth());
  EXPECT_EQ(50, imageHeight());
}

}  // namespace blink
