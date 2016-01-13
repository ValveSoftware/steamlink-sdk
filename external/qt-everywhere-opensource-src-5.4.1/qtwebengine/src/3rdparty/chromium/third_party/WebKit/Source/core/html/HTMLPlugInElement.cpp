/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/html/HTMLPlugInElement.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/npruntime_impl.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/PluginDocument.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/EventHandler.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderWidget.h"
#include "platform/Logging.h"
#include "platform/MIMETypeFromURL.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/Widget.h"
#include "platform/plugins/PluginData.h"

namespace WebCore {

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document& doc, bool createdByParser, PreferPlugInsForImagesOption preferPlugInsForImagesOption)
    : HTMLFrameOwnerElement(tagName, doc)
    , m_isDelayingLoadEvent(false)
    , m_NPObject(0)
    , m_isCapturingMouseEvents(false)
    // m_needsWidgetUpdate(!createdByParser) allows HTMLObjectElement to delay
    // widget updates until after all children are parsed. For HTMLEmbedElement
    // this delay is unnecessary, but it is simpler to make both classes share
    // the same codepath in this class.
    , m_needsWidgetUpdate(!createdByParser)
    , m_shouldPreferPlugInsForImages(preferPlugInsForImagesOption == ShouldPreferPlugInsForImages)
    , m_displayState(Playing)
{
    setHasCustomStyleCallbacks();
}

HTMLPlugInElement::~HTMLPlugInElement()
{
    ASSERT(!m_pluginWrapper); // cleared in detach()
    ASSERT(!m_isDelayingLoadEvent);

    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }
}

void HTMLPlugInElement::trace(Visitor* visitor)
{
    visitor->trace(m_imageLoader);
    HTMLFrameOwnerElement::trace(visitor);
}

bool HTMLPlugInElement::canProcessDrag() const
{
    return pluginWidget() && pluginWidget()->isPluginView() && toPluginView(pluginWidget())->canProcessDrag();
}

bool HTMLPlugInElement::willRespondToMouseClickEvents()
{
    if (isDisabledFormControl())
        return false;
    RenderObject* r = renderer();
    return r && (r->isEmbeddedObject() || r->isWidget());
}

void HTMLPlugInElement::removeAllEventListeners()
{
    HTMLFrameOwnerElement::removeAllEventListeners();
    if (RenderWidget* renderer = existingRenderWidget()) {
        if (Widget* widget = renderer->widget())
            widget->eventListenersRemoved();
    }
}

void HTMLPlugInElement::didMoveToNewDocument(Document& oldDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();
    HTMLFrameOwnerElement::didMoveToNewDocument(oldDocument);
}

void HTMLPlugInElement::attach(const AttachContext& context)
{
    HTMLFrameOwnerElement::attach(context);

    if (!renderer() || useFallbackContent())
        return;

    if (isImageType()) {
        if (!m_imageLoader)
            m_imageLoader = HTMLImageLoader::create(this);
        m_imageLoader->updateFromElement();
    } else if (needsWidgetUpdate()
        && renderEmbeddedObject()
        && !renderEmbeddedObject()->showsUnavailablePluginIndicator()
        && !wouldLoadAsNetscapePlugin(m_url, m_serviceType)
        && !m_isDelayingLoadEvent) {
        m_isDelayingLoadEvent = true;
        document().incrementLoadEventDelayCount();
        document().loadPluginsSoon();
    }
}

void HTMLPlugInElement::updateWidget()
{
    RefPtrWillBeRawPtr<HTMLPlugInElement> protector(this);
    updateWidgetInternal();
    if (m_isDelayingLoadEvent) {
        m_isDelayingLoadEvent = false;
        document().decrementLoadEventDelayCount();
    }
}

void HTMLPlugInElement::requestPluginCreationWithoutRendererIfPossible()
{
    if (m_serviceType.isEmpty())
        return;

    if (!document().frame()
        || !document().frame()->loader().client()->canCreatePluginWithoutRenderer(m_serviceType))
        return;

    if (renderer() && renderer()->isWidget())
        return;

    createPluginWithoutRenderer();
}

void HTMLPlugInElement::createPluginWithoutRenderer()
{
    ASSERT(document().frame()->loader().client()->canCreatePluginWithoutRenderer(m_serviceType));

    KURL url;
    Vector<String> paramNames;
    Vector<String> paramValues;

    paramNames.append("type");
    paramValues.append(m_serviceType);

    bool useFallback = false;
    loadPlugin(url, m_serviceType, paramNames, paramValues, useFallback, false);
}

bool HTMLPlugInElement::shouldAccelerate() const
{
    if (Widget* widget = ownedWidget())
        return widget->isPluginView() && toPluginView(widget)->platformLayer();
    return false;
}

void HTMLPlugInElement::detach(const AttachContext& context)
{
    // Update the widget the next time we attach (detaching destroys the plugin).
    // FIXME: None of this "needsWidgetUpdate" related code looks right.
    if (renderer() && !useFallbackContent())
        setNeedsWidgetUpdate(true);
    if (m_isDelayingLoadEvent) {
        m_isDelayingLoadEvent = false;
        document().decrementLoadEventDelayCount();
    }

    // Only try to persist a plugin widget we actually own.
    Widget* plugin = ownedWidget();
    if (plugin && plugin->pluginShouldPersist())
        m_persistedPluginWidget = plugin;
    resetInstance();
    // FIXME - is this next line necessary?
    setWidget(nullptr);

    if (m_isCapturingMouseEvents) {
        if (LocalFrame* frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsNode(nullptr);
        m_isCapturingMouseEvents = false;
    }

    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }

    HTMLFrameOwnerElement::detach(context);
}

RenderObject* HTMLPlugInElement::createRenderer(RenderStyle* style)
{
    // Fallback content breaks the DOM->Renderer class relationship of this
    // class and all superclasses because createObject won't necessarily
    // return a RenderEmbeddedObject, RenderPart or even RenderWidget.
    if (useFallbackContent())
        return RenderObject::createObject(this, style);

    if (isImageType()) {
        RenderImage* image = new RenderImage(this);
        image->setImageResource(RenderImageResource::create());
        return image;
    }

    return new RenderEmbeddedObject(this);
}

void HTMLPlugInElement::willRecalcStyle(StyleRecalcChange)
{
    // FIXME: Why is this necessary? Manual re-attach is almost always wrong.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType())
        reattach();
}

void HTMLPlugInElement::finishParsingChildren()
{
    HTMLFrameOwnerElement::finishParsingChildren();
    if (useFallbackContent())
        return;

    setNeedsWidgetUpdate(true);
    if (inDocument())
        setNeedsStyleRecalc(SubtreeStyleChange);
}

void HTMLPlugInElement::resetInstance()
{
    m_pluginWrapper.clear();
}

SharedPersistent<v8::Object>* HTMLPlugInElement::pluginWrapper()
{
    LocalFrame* frame = document().frame();
    if (!frame)
        return 0;

    // If the host dynamically turns off JavaScript (or Java) we will still
    // return the cached allocated Bindings::Instance. Not supporting this
    // edge-case is OK.
    if (!m_pluginWrapper) {
        Widget* plugin;

        if (m_persistedPluginWidget)
            plugin = m_persistedPluginWidget.get();
        else
            plugin = pluginWidget();

        if (plugin)
            m_pluginWrapper = frame->script().createPluginWrapper(plugin);
    }
    return m_pluginWrapper.get();
}

Widget* HTMLPlugInElement::pluginWidget() const
{
    if (RenderWidget* renderWidget = renderWidgetForJSBindings())
        return renderWidget->widget();
    return 0;
}

bool HTMLPlugInElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr)
        return true;
    return HTMLFrameOwnerElement::isPresentationAttribute(name);
}

void HTMLPlugInElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr) {
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    } else if (name == heightAttr) {
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    } else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr) {
        applyAlignmentAttributeToStyle(value, style);
    } else {
        HTMLFrameOwnerElement::collectStyleForPresentationAttribute(name, value, style);
    }
}

void HTMLPlugInElement::defaultEventHandler(Event* event)
{
    // Firefox seems to use a fake event listener to dispatch events to plug-in
    // (tested with mouse events only). This is observable via different order
    // of events - in Firefox, event listeners specified in HTML attributes
    // fires first, then an event gets dispatched to plug-in, and only then
    // other event listeners fire. Hopefully, this difference does not matter in
    // practice.

    // FIXME: Mouse down and scroll events are passed down to plug-in via custom
    // code in EventHandler; these code paths should be united.

    RenderObject* r = renderer();
    if (!r || !r->isWidget())
        return;
    if (r->isEmbeddedObject()) {
        if (toRenderEmbeddedObject(r)->showsUnavailablePluginIndicator())
            return;
        if (displayState() < Playing)
            return;
    }
    RefPtr<Widget> widget = toRenderWidget(r)->widget();
    if (!widget)
        return;
    widget->handleEvent(event);
    if (event->defaultHandled())
        return;
    HTMLFrameOwnerElement::defaultEventHandler(event);
}

RenderWidget* HTMLPlugInElement::renderWidgetForJSBindings() const
{
    // Needs to load the plugin immediatedly because this function is called
    // when JavaScript code accesses the plugin.
    // FIXME: Check if dispatching events here is safe.
    document().updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasksSynchronously);
    return existingRenderWidget();
}

bool HTMLPlugInElement::isKeyboardFocusable() const
{
    if (!document().isActive())
        return false;
    return pluginWidget() && pluginWidget()->isPluginView() && toPluginView(pluginWidget())->supportsKeyboardFocus();
}

bool HTMLPlugInElement::hasCustomFocusLogic() const
{
    return !hasAuthorShadowRoot();
}

bool HTMLPlugInElement::isPluginElement() const
{
    return true;
}

bool HTMLPlugInElement::rendererIsFocusable() const
{
    if (HTMLFrameOwnerElement::supportsFocus() && HTMLFrameOwnerElement::rendererIsFocusable())
        return true;

    if (useFallbackContent() || !renderer() || !renderer()->isEmbeddedObject())
        return false;
    return !toRenderEmbeddedObject(renderer())->showsUnavailablePluginIndicator();
}

NPObject* HTMLPlugInElement::getNPObject()
{
    ASSERT(document().frame());
    if (!m_NPObject)
        m_NPObject = document().frame()->script().createScriptObjectForPluginElement(this);
    return m_NPObject;
}

bool HTMLPlugInElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (LocalFrame* frame = document().frame()) {
        KURL completedURL = document().completeURL(m_url);
        return frame->loader().client()->objectContentType(completedURL, m_serviceType, shouldPreferPlugInsForImages()) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

RenderEmbeddedObject* HTMLPlugInElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    if (!renderer() || !renderer()->isEmbeddedObject())
        return 0;
    return toRenderEmbeddedObject(renderer());
}

// We don't use m_url, as it may not be the final URL that the object loads,
// depending on <param> values.
bool HTMLPlugInElement::allowedToLoadFrameURL(const String& url)
{
    KURL completeURL = document().completeURL(url);
    if (contentFrame() && protocolIsJavaScript(completeURL)
        && !document().securityOrigin()->canAccess(contentDocument()->securityOrigin()))
        return false;
    return document().frame()->isURLAllowed(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInElement::wouldLoadAsNetscapePlugin(const String& url, const String& serviceType)
{
    ASSERT(document().frame());
    KURL completedURL;
    if (!url.isEmpty())
        completedURL = document().completeURL(url);
    return document().frame()->loader().client()->objectContentType(completedURL, serviceType, shouldPreferPlugInsForImages()) == ObjectContentNetscapePlugin;
}

bool HTMLPlugInElement::requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;

    // FIXME: None of this code should use renderers!
    RenderEmbeddedObject* renderer = renderEmbeddedObject();
    ASSERT(renderer);
    if (!renderer)
        return false;

    KURL completedURL = document().completeURL(url);
    if (!pluginIsLoadable(completedURL, mimeType))
        return false;

    bool useFallback;
    bool requireRenderer = true;
    if (shouldUsePlugin(completedURL, mimeType, hasFallbackContent(), useFallback))
        return loadPlugin(completedURL, mimeType, paramNames, paramValues, useFallback, requireRenderer);

    // If the plug-in element already contains a subframe,
    // loadOrRedirectSubframe will re-use it. Otherwise, it will create a new
    // frame and set it as the RenderPart's widget, causing what was previously
    // in the widget to be torn down.
    return loadOrRedirectSubframe(completedURL, getNameAttribute(), true);
}

bool HTMLPlugInElement::loadPlugin(const KURL& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback, bool requireRenderer)
{
    LocalFrame* frame = document().frame();

    if (!frame->loader().allowPlugins(AboutToInstantiatePlugin))
        return false;

    RenderEmbeddedObject* renderer = renderEmbeddedObject();
    // FIXME: This code should not depend on renderer!
    if ((!renderer && requireRenderer) || useFallback)
        return false;

    WTF_LOG(Plugins, "%p Plug-in URL: %s", this, m_url.utf8().data());
    WTF_LOG(Plugins, "   Loaded URL: %s", url.string().utf8().data());
    m_loadedUrl = url;

    RefPtr<Widget> widget = m_persistedPluginWidget;
    if (!widget) {
        bool loadManually = document().isPluginDocument() && !document().containsPlugins() && toPluginDocument(document()).shouldLoadPluginManually();
        FrameLoaderClient::DetachedPluginPolicy policy = requireRenderer ? FrameLoaderClient::FailOnDetachedPlugin : FrameLoaderClient::AllowDetachedPlugin;
        widget = frame->loader().client()->createPlugin(this, url, paramNames, paramValues, mimeType, loadManually, policy);
    }

    if (!widget) {
        if (renderer && !renderer->showsUnavailablePluginIndicator())
            renderer->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginMissing);
        return false;
    }

    if (renderer) {
        setWidget(widget);
        m_persistedPluginWidget = nullptr;
    } else if (widget != m_persistedPluginWidget) {
        m_persistedPluginWidget = widget;
    }
    document().setContainsPlugins();
    scheduleSVGFilterLayerUpdateHack();
    return true;
}

bool HTMLPlugInElement::shouldUsePlugin(const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback)
{
    // Allow other plug-ins to win over QuickTime because if the user has
    // installed a plug-in that can handle TIFF (which QuickTime can also
    // handle) they probably intended to override QT.
    if (document().frame()->page() && (mimeType == "image/tiff" || mimeType == "image/tif" || mimeType == "image/x-tiff")) {
        const PluginData* pluginData = document().frame()->page()->pluginData();
        String pluginName = pluginData ? pluginData->pluginNameForMimeType(mimeType) : String();
        if (!pluginName.isEmpty() && !pluginName.contains("QuickTime", false))
            return true;
    }

    ObjectContentType objectType = document().frame()->loader().client()->objectContentType(url, mimeType, shouldPreferPlugInsForImages());
    // If an object's content can't be handled and it has no fallback, let
    // it be handled as a plugin to show the broken plugin icon.
    useFallback = objectType == ObjectContentNone && hasFallback;
    return objectType == ObjectContentNone || objectType == ObjectContentNetscapePlugin || objectType == ObjectContentOtherPlugin;

}

void HTMLPlugInElement::dispatchErrorEvent()
{
    if (document().isPluginDocument() && document().ownerElement())
        document().ownerElement()->dispatchEvent(Event::create(EventTypeNames::error));
    else
        dispatchEvent(Event::create(EventTypeNames::error));
}

bool HTMLPlugInElement::pluginIsLoadable(const KURL& url, const String& mimeType)
{
    LocalFrame* frame = document().frame();
    Settings* settings = frame->settings();
    if (!settings)
        return false;

    if (MIMETypeRegistry::isJavaAppletMIMEType(mimeType) && !settings->javaEnabled())
        return false;

    if (document().isSandboxed(SandboxPlugins))
        return false;

    if (!document().securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame, url.string());
        return false;
    }

    AtomicString declaredMimeType = document().isPluginDocument() && document().ownerElement() ?
        document().ownerElement()->fastGetAttribute(HTMLNames::typeAttr) :
        fastGetAttribute(HTMLNames::typeAttr);
    if (!document().contentSecurityPolicy()->allowObjectFromSource(url)
        || !document().contentSecurityPolicy()->allowPluginType(mimeType, declaredMimeType, url)) {
        renderEmbeddedObject()->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy);
        return false;
    }

    return frame->loader().mixedContentChecker()->canRunInsecureContent(document().securityOrigin(), url);
}

void HTMLPlugInElement::didAddUserAgentShadowRoot(ShadowRoot&)
{
    userAgentShadowRoot()->appendChild(HTMLContentElement::create(document()));
}

void HTMLPlugInElement::willAddFirstAuthorShadowRoot()
{
    lazyReattachIfAttached();
}

bool HTMLPlugInElement::hasFallbackContent() const
{
    return false;
}

bool HTMLPlugInElement::useFallbackContent() const
{
    return hasAuthorShadowRoot();
}

}
