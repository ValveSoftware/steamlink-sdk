// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ModulesInitializer.h"

#include "bindings/modules/v8/ModuleBindingsInitializer.h"
#include "core/EventTypeNames.h"
#include "core/css/CSSPaintImageGenerator.h"
#include "core/dom/Document.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "modules/EventModulesFactory.h"
#include "modules/EventModulesNames.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/IndexedDBNames.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/compositorworker/CompositorWorkerThread.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"
#include "modules/filesystem/DraggedIsolatedFileSystemImpl.h"
#include "modules/imagebitmap/ImageBitmapRenderingContext.h"
#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "wtf/PtrUtil.h"

namespace blink {

void ModulesInitializer::initialize()
{
    ASSERT(!isInitialized());

    // Strings must be initialized before calling CoreInitializer::init().
    const unsigned modulesStaticStringsCount = EventNames::EventModulesNamesCount
        + EventTargetNames::EventTargetModulesNamesCount
        + IndexedDBNames::IndexedDBNamesCount;
    StringImpl::reserveStaticStringsCapacityForSize(modulesStaticStringsCount);

    EventNames::initModules();
    EventTargetNames::initModules();
    Document::registerEventFactory(EventModulesFactory::create());
    ModuleBindingsInitializer::init();
    IndexedDBNames::init();
    AXObjectCache::init(AXObjectCacheImpl::create);
    DraggedIsolatedFileSystem::init(DraggedIsolatedFileSystemImpl::prepareForDataObject);
    CSSPaintImageGenerator::init(CSSPaintImageGeneratorImpl::create);

    CoreInitializer::initialize();

    // Canvas context types must be registered with the HTMLCanvasElement.
    HTMLCanvasElement::registerRenderingContextFactory(wrapUnique(new CanvasRenderingContext2D::Factory()));
    HTMLCanvasElement::registerRenderingContextFactory(wrapUnique(new WebGLRenderingContext::Factory()));
    HTMLCanvasElement::registerRenderingContextFactory(wrapUnique(new WebGL2RenderingContext::Factory()));
    HTMLCanvasElement::registerRenderingContextFactory(wrapUnique(new ImageBitmapRenderingContext::Factory()));

    // OffscreenCanvas context types must be registered with the OffscreenCanvas.
    OffscreenCanvas::registerRenderingContextFactory(wrapUnique(new OffscreenCanvasRenderingContext2D::Factory()));
    OffscreenCanvas::registerRenderingContextFactory(wrapUnique(new WebGLRenderingContext::Factory()));

    ASSERT(isInitialized());
}

void ModulesInitializer::shutdown()
{
    ASSERT(isInitialized());
    DatabaseManager::terminateDatabaseThread();
    CoreInitializer::shutdown();
    CompositorWorkerThread::clearSharedBackingThread();
}

} // namespace blink
