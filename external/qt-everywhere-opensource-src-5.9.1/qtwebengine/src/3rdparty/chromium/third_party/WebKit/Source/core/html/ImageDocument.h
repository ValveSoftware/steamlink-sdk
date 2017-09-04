/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageDocument_h
#define ImageDocument_h

#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLImageElement.h"
#include "wtf/RefPtr.h"

namespace blink {

class ImageResource;

class CORE_EXPORT ImageDocument final : public HTMLDocument {
 public:
  static ImageDocument* create(
      const DocumentInit& initializer = DocumentInit()) {
    return new ImageDocument(initializer);
  }

  ImageResource* cachedImage();
  HTMLImageElement* imageElement() const { return m_imageElement.get(); }

  void windowSizeChanged();
  void imageUpdated();
  void imageClicked(int x, int y);
  void imageLoaded();
  void updateImageStyle();
  bool shouldShrinkToFit() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit ImageDocument(const DocumentInit&);

  DocumentParser* createParser() override;

  void createDocumentStructure();

  // Calculates how large the div needs to be to properly center the image.
  int calculateDivWidth();

  // These methods are for m_shrinkToFitMode == Desktop.
  void resizeImageToFit();
  void restoreImageSize();
  bool imageFitsInWindow() const;
  // Calculates the image size multiplier that's needed to fit the image to
  // the window, taking into account page zoom and device scale.
  float scale() const;

  Member<HTMLDivElement> m_divElement;
  Member<HTMLImageElement> m_imageElement;

  // Whether enough of the image has been loaded to determine its size
  bool m_imageSizeIsKnown;

  // Whether the image is shrunk to fit or not
  bool m_didShrinkImage;

  // Whether the image should be shrunk or not
  bool m_shouldShrinkImage;

  // Whether the image has finished loading or not
  bool m_imageIsLoaded;

  // Size of the checkerboard background tiles
  int m_styleCheckerSize;

  // Desktop: State of the mouse cursor in the image style
  enum MouseCursorMode { Default, ZoomIn, ZoomOut };
  MouseCursorMode m_styleMouseCursorMode;

  enum ShrinkToFitMode { Viewport, Desktop };
  ShrinkToFitMode m_shrinkToFitMode;
};

DEFINE_DOCUMENT_TYPE_CASTS(ImageDocument);

}  // namespace blink

#endif  // ImageDocument_h
