// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "content/browser/compositor/image_transport_factory.h"

namespace cc {
class ContextProvider;
}

namespace content {

// An ImageTransportFactory that disables transport.
class NoTransportImageTransportFactory : public ImageTransportFactory {
 public:
  explicit NoTransportImageTransportFactory(
      scoped_ptr<ui::ContextFactory> context_factory);
  virtual ~NoTransportImageTransportFactory();

  // ImageTransportFactory implementation.
  virtual ui::ContextFactory* GetContextFactory() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetSharedSurfaceHandle() OVERRIDE;
  virtual GLHelper* GetGLHelper() OVERRIDE;
  virtual void AddObserver(ImageTransportFactoryObserver* observer) OVERRIDE;
  virtual void RemoveObserver(ImageTransportFactoryObserver* observer) OVERRIDE;
#if defined(OS_MACOSX)
  virtual void OnSurfaceDisplayed(int surface_id) OVERRIDE {}
#endif

 private:
  scoped_ptr<ui::ContextFactory> context_factory_;
  scoped_refptr<cc::ContextProvider> context_provider_;
  scoped_ptr<GLHelper> gl_helper_;
  ObserverList<ImageTransportFactoryObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NoTransportImageTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
