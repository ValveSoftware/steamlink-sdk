/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/html/HTMLLinkElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/events/EventSender.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/LinkManifest.h"
#include "core/html/imports/LinkImport.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/NetworkHintsInterface.h"
#include "core/style/StyleInheritedData.h"
#include "platform/ContentType.h"
#include "platform/Histogram.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebIconSizesParser.h"
#include "public/platform/WebSize.h"
#include "wtf/StdLibExtras.h"

namespace blink {

using namespace HTMLNames;

static LinkEventSender& linkLoadEventSender()
{
    DEFINE_STATIC_LOCAL(LinkEventSender, sharedLoadEventSender, (LinkEventSender::create(EventTypeNames::load)));
    return sharedLoadEventSender;
}

static bool styleSheetTypeIsSupported(const String& type)
{
    String trimmedType = ContentType(type).type();
    return trimmedType.isEmpty() || MIMETypeRegistry::isSupportedStyleSheetMIMEType(trimmedType);
}

inline HTMLLinkElement::HTMLLinkElement(Document& document, bool createdByParser)
    : HTMLElement(linkTag, document)
    , m_linkLoader(LinkLoader::create(this))
    , m_sizes(DOMTokenList::create(this))
    , m_relList(RelList::create(this))
    , m_createdByParser(createdByParser)
    , m_isInShadowTree(false)
{
}

HTMLLinkElement* HTMLLinkElement::create(Document& document, bool createdByParser)
{
    return new HTMLLinkElement(document, createdByParser);
}

HTMLLinkElement::~HTMLLinkElement()
{
}

void HTMLLinkElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == relAttr) {
        m_relAttribute = LinkRelAttribute(value);
        m_relList->setRelValues(value);
        process();
    } else if (name == hrefAttr) {
        // Log href attribute before logging resource fetching in process().
        logUpdateAttributeIfIsolatedWorldAndInDocument("link", hrefAttr, oldValue, value);
        process();
    } else if (name == typeAttr) {
        m_type = value;
        process();
    } else if (name == asAttr) {
        m_as = value;
        process();
    } else if (name == sizesAttr) {
        m_sizes->setValue(value);
    } else if (name == mediaAttr) {
        m_media = value.lower();
        process();
    } else if (name == scopeAttr) {
        m_scope = value;
        process();
    } else if (name == disabledAttr) {
        UseCounter::count(document(), UseCounter::HTMLLinkElementDisabled);
        if (LinkStyle* link = linkStyle())
            link->setDisabledState(!value.isNull());
    } else {
        if (name == titleAttr) {
            if (LinkStyle* link = linkStyle())
                link->setSheetTitle(value);
        }

        HTMLElement::parseAttribute(name, oldValue, value);
    }
}

bool HTMLLinkElement::shouldLoadLink()
{
    return inShadowIncludingDocument();
}

bool HTMLLinkElement::loadLink(const String& type, const String& as, const String& media, const KURL& url)
{
    return m_linkLoader->loadLink(m_relAttribute, crossOriginAttributeValue(fastGetAttribute(HTMLNames::crossoriginAttr)), type, as, media, url, document(), NetworkHintsInterfaceImpl());
}

LinkResource* HTMLLinkElement::linkResourceToProcess()
{
    bool visible = inShadowIncludingDocument() && !m_isInShadowTree;
    if (!visible) {
        ASSERT(!linkStyle() || !linkStyle()->hasSheet());
        return nullptr;
    }

    if (!m_link) {
        if (m_relAttribute.isImport()) {
            m_link = LinkImport::create(this);
        } else if (m_relAttribute.isManifest()) {
            m_link = LinkManifest::create(this);
        } else if (RuntimeEnabledFeatures::linkServiceWorkerEnabled() && m_relAttribute.isServiceWorker()) {
            if (document().frame())
                m_link = document().frame()->loader().client()->createServiceWorkerLinkResource(this);
        } else {
            LinkStyle* link = LinkStyle::create(this);
            if (fastHasAttribute(disabledAttr)) {
                UseCounter::count(document(), UseCounter::HTMLLinkElementDisabled);
                link->setDisabledState(true);
            }
            m_link = link;
        }
    }

    return m_link.get();
}

LinkStyle* HTMLLinkElement::linkStyle() const
{
    if (!m_link || m_link->type() != LinkResource::Style)
        return nullptr;
    return static_cast<LinkStyle*>(m_link.get());
}

LinkImport* HTMLLinkElement::linkImport() const
{
    if (!m_link || m_link->type() != LinkResource::Import)
        return nullptr;
    return static_cast<LinkImport*>(m_link.get());
}

Document* HTMLLinkElement::import() const
{
    if (LinkImport* link = linkImport())
        return link->importedDocument();
    return nullptr;
}

void HTMLLinkElement::process()
{
    if (LinkResource* link = linkResourceToProcess())
        link->process();
}

Node::InsertionNotificationRequest HTMLLinkElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    logAddElementIfIsolatedWorldAndInDocument("link", relAttr, hrefAttr);
    if (!insertionPoint->inShadowIncludingDocument())
        return InsertionDone;

    m_isInShadowTree = isInShadowTree();
    if (m_isInShadowTree) {
        String message = "HTML element <link> is ignored in shadow tree.";
        document().addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
        return InsertionDone;
    }

    document().styleEngine().addStyleSheetCandidateNode(this);

    process();

    if (m_link)
        m_link->ownerInserted();

    return InsertionDone;
}

void HTMLLinkElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);
    if (!insertionPoint->inShadowIncludingDocument())
        return;

    m_linkLoader->released();

    if (m_isInShadowTree) {
        ASSERT(!linkStyle() || !linkStyle()->hasSheet());
        return;
    }
    document().styleEngine().removeStyleSheetCandidateNode(this);

    StyleSheet* removedSheet = sheet();

    if (m_link)
        m_link->ownerRemoved();

    document().styleEngine().setNeedsActiveStyleUpdate(removedSheet, FullStyleUpdate);
}

void HTMLLinkElement::finishParsingChildren()
{
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

bool HTMLLinkElement::styleSheetIsLoading() const
{
    return linkStyle() && linkStyle()->styleSheetIsLoading();
}

void HTMLLinkElement::linkLoaded()
{
    dispatchEvent(Event::create(EventTypeNames::load));
}

void HTMLLinkElement::linkLoadingErrored()
{
    dispatchEvent(Event::create(EventTypeNames::error));
}

void HTMLLinkElement::didStartLinkPrerender()
{
    dispatchEvent(Event::create(EventTypeNames::webkitprerenderstart));
}

void HTMLLinkElement::didStopLinkPrerender()
{
    dispatchEvent(Event::create(EventTypeNames::webkitprerenderstop));
}

void HTMLLinkElement::didSendLoadForLinkPrerender()
{
    dispatchEvent(Event::create(EventTypeNames::webkitprerenderload));
}

void HTMLLinkElement::didSendDOMContentLoadedForLinkPrerender()
{
    dispatchEvent(Event::create(EventTypeNames::webkitprerenderdomcontentloaded));
}

void HTMLLinkElement::valueWasSet()
{
    setSynchronizedLazyAttribute(HTMLNames::sizesAttr, m_sizes->value());
    WebVector<WebSize> webIconSizes = WebIconSizesParser::parseIconSizes(m_sizes->value());
    m_iconSizes.resize(webIconSizes.size());
    for (size_t i = 0; i < webIconSizes.size(); ++i)
        m_iconSizes[i] = webIconSizes[i];
    process();
}

bool HTMLLinkElement::sheetLoaded()
{
    ASSERT(linkStyle());
    return linkStyle()->sheetLoaded();
}

void HTMLLinkElement::notifyLoadedSheetAndAllCriticalSubresources(LoadedSheetErrorStatus errorStatus)
{
    ASSERT(linkStyle());
    linkStyle()->notifyLoadedSheetAndAllCriticalSubresources(errorStatus);
}

void HTMLLinkElement::dispatchPendingLoadEvents()
{
    linkLoadEventSender().dispatchPendingEvents();
}

void HTMLLinkElement::dispatchPendingEvent(LinkEventSender* eventSender)
{
    ASSERT_UNUSED(eventSender, eventSender == &linkLoadEventSender());
    ASSERT(m_link);
    if (m_link->hasLoaded())
        linkLoaded();
    else
        linkLoadingErrored();
}

void HTMLLinkElement::scheduleEvent()
{
    linkLoadEventSender().dispatchEventSoon(this);
}

void HTMLLinkElement::startLoadingDynamicSheet()
{
    ASSERT(linkStyle());
    linkStyle()->startLoadingDynamicSheet();
}

bool HTMLLinkElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || HTMLElement::isURLAttribute(attribute);
}

bool HTMLLinkElement::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == hrefAttr || HTMLElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLLinkElement::subResourceAttributeName() const
{
    // If the link element is not css, ignore it.
    if (equalIgnoringCase(getAttribute(typeAttr), "text/css")) {
        // FIXME: Add support for extracting links of sub-resources which
        // are inside style-sheet such as @import, @font-face, url(), etc.
        return hrefAttr;
    }
    return HTMLElement::subResourceAttributeName();
}

KURL HTMLLinkElement::href() const
{
    return document().completeURL(getAttribute(hrefAttr));
}

const AtomicString& HTMLLinkElement::rel() const
{
    return getAttribute(relAttr);
}

const AtomicString& HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

bool HTMLLinkElement::async() const
{
    return fastHasAttribute(HTMLNames::asyncAttr);
}

IconType HTMLLinkElement::getIconType() const
{
    return m_relAttribute.getIconType();
}

const Vector<IntSize>& HTMLLinkElement::iconSizes() const
{
    return m_iconSizes;
}

DOMTokenList* HTMLLinkElement::sizes() const
{
    return m_sizes.get();
}

DEFINE_TRACE(HTMLLinkElement)
{
    visitor->trace(m_link);
    visitor->trace(m_sizes);
    visitor->trace(m_linkLoader);
    visitor->trace(m_relList);
    HTMLElement::trace(visitor);
    LinkLoaderClient::trace(visitor);
    DOMTokenListObserver::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(HTMLLinkElement)
{
    visitor->traceWrappers(m_relList);
    HTMLElement::traceWrappers(visitor);
}

LinkStyle* LinkStyle::create(HTMLLinkElement* owner)
{
    return new LinkStyle(owner);
}

LinkStyle::LinkStyle(HTMLLinkElement* owner)
    : LinkResource(owner)
    , m_disabledState(Unset)
    , m_pendingSheetType(None)
    , m_loading(false)
    , m_firedLoad(false)
    , m_loadedSheet(false)
    , m_fetchFollowingCORS(false)
{
}

LinkStyle::~LinkStyle()
{
}

Document& LinkStyle::document()
{
    return m_owner->document();
}

enum StyleSheetCacheStatus {
    StyleSheetNewEntry,
    StyleSheetInDiskCache,
    StyleSheetInMemoryCache,
    StyleSheetCacheStatusCount,
};

void LinkStyle::setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource* cachedStyleSheet)
{
    if (!m_owner->inShadowIncludingDocument()) {
        ASSERT(!m_sheet);
        return;
    }

    // See the comment in PendingScript.cpp about why this check is necessary
    // here, instead of in the resource fetcher. https://crbug.com/500701.
    if (!cachedStyleSheet->errorOccurred() && m_owner->fastHasAttribute(HTMLNames::integrityAttr) && cachedStyleSheet->resourceBuffer() && !SubresourceIntegrity::CheckSubresourceIntegrity(*m_owner, cachedStyleSheet->resourceBuffer()->data(), cachedStyleSheet->resourceBuffer()->size(), KURL(baseURL, href), *cachedStyleSheet)) {
        m_loading = false;
        removePendingSheet();
        notifyLoadedSheetAndAllCriticalSubresources(Node::ErrorOccurredLoadingSubresource);
        return;
    }
    // While the stylesheet is asynchronously loading, the owner can be moved under
    // shadow tree.  In that case, cancel any processing on the loaded content.
    if (m_owner->isInShadowTree()) {
        m_loading = false;
        removePendingSheet();
        if (m_sheet)
            clearSheet();
        return;
    }

    CSSParserContext parserContext(m_owner->document(), nullptr, baseURL, charset);

    DEFINE_STATIC_LOCAL(EnumerationHistogram, restoredCachedStyleSheetHistogram, ("Blink.RestoredCachedStyleSheet", 2));
    DEFINE_STATIC_LOCAL(EnumerationHistogram, restoredCachedStyleSheet2Histogram, ("Blink.RestoredCachedStyleSheet2", StyleSheetCacheStatusCount));

    if (StyleSheetContents* restoredSheet = const_cast<CSSStyleSheetResource*>(cachedStyleSheet)->restoreParsedStyleSheet(parserContext)) {
        ASSERT(restoredSheet->isCacheableForResource());
        ASSERT(!restoredSheet->isLoading());

        if (m_sheet)
            clearSheet();
        m_sheet = CSSStyleSheet::create(restoredSheet, m_owner);
        m_sheet->setMediaQueries(MediaQuerySet::create(m_owner->media()));
        m_sheet->setTitle(m_owner->title());
        setCrossOriginStylesheetStatus(m_sheet.get());

        m_loading = false;
        restoredSheet->checkLoaded();

        restoredCachedStyleSheetHistogram.count(true);
        restoredCachedStyleSheet2Histogram.count(StyleSheetInMemoryCache);
        return;
    }
    restoredCachedStyleSheetHistogram.count(false);
    StyleSheetCacheStatus cacheStatus = cachedStyleSheet->response().wasCached() ? StyleSheetInDiskCache : StyleSheetNewEntry;
    restoredCachedStyleSheet2Histogram.count(cacheStatus);

    StyleSheetContents* styleSheet = StyleSheetContents::create(href, parserContext);

    if (m_sheet)
        clearSheet();

    m_sheet = CSSStyleSheet::create(styleSheet, m_owner);
    m_sheet->setMediaQueries(MediaQuerySet::create(m_owner->media()));
    m_sheet->setTitle(m_owner->title());
    setCrossOriginStylesheetStatus(m_sheet.get());

    styleSheet->parseAuthorStyleSheet(cachedStyleSheet, m_owner->document().getSecurityOrigin());

    m_loading = false;
    styleSheet->notifyLoadedSheet(cachedStyleSheet);
    styleSheet->checkLoaded();

    if (styleSheet->isCacheableForResource())
        const_cast<CSSStyleSheetResource*>(cachedStyleSheet)->saveParsedStyleSheet(styleSheet);
    clearResource();
}

bool LinkStyle::sheetLoaded()
{
    if (!styleSheetIsLoading()) {
        removePendingSheet();
        return true;
    }
    return false;
}

void LinkStyle::notifyLoadedSheetAndAllCriticalSubresources(Node::LoadedSheetErrorStatus errorStatus)
{
    if (m_firedLoad)
        return;
    m_loadedSheet = (errorStatus == Node::NoErrorLoadingSubresource);
    if (m_owner)
        m_owner->scheduleEvent();
    m_firedLoad = true;
}

void LinkStyle::startLoadingDynamicSheet()
{
    ASSERT(m_pendingSheetType < Blocking);
    addPendingSheet(Blocking);
}

void LinkStyle::clearSheet()
{
    ASSERT(m_sheet);
    ASSERT(m_sheet->ownerNode() == m_owner);
    m_sheet.release()->clearOwnerNode();
}

bool LinkStyle::styleSheetIsLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return m_sheet->contents()->isLoading();
}

void LinkStyle::addPendingSheet(PendingSheetType type)
{
    if (type <= m_pendingSheetType)
        return;
    m_pendingSheetType = type;

    if (m_pendingSheetType == NonBlocking)
        return;
    m_owner->document().styleEngine().addPendingSheet(m_styleEngineContext);
}

void LinkStyle::removePendingSheet()
{
    PendingSheetType type = m_pendingSheetType;
    m_pendingSheetType = None;

    if (type == None)
        return;
    if (type == NonBlocking) {
        // Tell StyleEngine to re-compute styleSheets of this m_owner's treescope.
        m_owner->document().styleEngine().modifiedStyleSheetCandidateNode(m_owner);
        return;
    }

    m_owner->document().styleEngine().removePendingSheet(m_owner, m_styleEngineContext);
}

void LinkStyle::setDisabledState(bool disabled)
{
    LinkStyle::DisabledState oldDisabledState = m_disabledState;
    m_disabledState = disabled ? Disabled : EnabledViaScript;
    if (oldDisabledState != m_disabledState) {
        // If we change the disabled state while the sheet is still loading, then we have to
        // perform three checks:
        if (styleSheetIsLoading()) {
            // Check #1: The sheet becomes disabled while loading.
            if (m_disabledState == Disabled)
                removePendingSheet();

            // Check #2: An alternate sheet becomes enabled while it is still loading.
            if (m_owner->relAttribute().isAlternate() && m_disabledState == EnabledViaScript)
                addPendingSheet(Blocking);

            // Check #3: A main sheet becomes enabled while it was still loading and
            // after it was disabled via script. It takes really terrible code to make this
            // happen (a double toggle for no reason essentially). This happens on
            // virtualplastic.net, which manages to do about 12 enable/disables on only 3
            // sheets. :)
            if (!m_owner->relAttribute().isAlternate() && m_disabledState == EnabledViaScript && oldDisabledState == Disabled)
                addPendingSheet(Blocking);

            // If the sheet is already loading just bail.
            return;
        }

        if (m_sheet) {
            m_sheet->setDisabled(disabled);
            return;
        }

        if (m_disabledState == EnabledViaScript && m_owner->shouldProcessStyle())
            process();
    }
}

void LinkStyle::setCrossOriginStylesheetStatus(CSSStyleSheet* sheet)
{
    if (m_fetchFollowingCORS && resource() && !resource()->errorOccurred()) {
        // Record the security origin the CORS access check succeeded at, if cross origin.
        // Only origins that are script accessible to it may access the stylesheet's rules.
        sheet->setAllowRuleAccessFromOrigin(m_owner->document().getSecurityOrigin());
    }
    m_fetchFollowingCORS = false;
}

void LinkStyle::process()
{
    ASSERT(m_owner->shouldProcessStyle());
    String type = m_owner->typeValue().lower();
    String as = m_owner->asValue().lower();
    String media = m_owner->media().lower();
    LinkRequestBuilder builder(m_owner);

    if (m_owner->relAttribute().getIconType() != InvalidIcon && builder.url().isValid() && !builder.url().isEmpty()) {
        if (!m_owner->shouldLoadLink())
            return;
        if (!document().getSecurityOrigin()->canDisplay(builder.url()))
            return;
        if (!document().contentSecurityPolicy()->allowImageFromSource(builder.url()))
            return;
        if (document().frame() && document().frame()->loader().client())
            document().frame()->loader().client()->dispatchDidChangeIcons(m_owner->relAttribute().getIconType());
    }

    if (!m_owner->loadLink(type, as, media, builder.url()))
        return;

    if (m_disabledState != Disabled && m_owner->relAttribute().isStyleSheet() && styleSheetTypeIsSupported(type) && shouldLoadResource() && builder.url().isValid()) {
        if (resource()) {
            removePendingSheet();
            clearResource();
            clearFetchFollowingCORS();
        }

        if (!m_owner->shouldLoadLink())
            return;

        m_loading = true;

        String title = m_owner->title();
        if (!title.isEmpty() && !m_owner->isAlternate() && m_disabledState != EnabledViaScript)
            document().styleEngine().setPreferredStylesheetSetNameIfNotSet(title);

        bool mediaQueryMatches = true;
        LocalFrame* frame = loadingFrame();
        if (!m_owner->media().isEmpty() && frame && frame->document()) {
            RefPtr<ComputedStyle> documentStyle = StyleResolver::styleForDocument(*frame->document());
            MediaQuerySet* media = MediaQuerySet::create(m_owner->media());
            MediaQueryEvaluator evaluator(frame);
            mediaQueryMatches = evaluator.eval(media);
        }

        // Don't hold up layout tree construction and script execution on stylesheets
        // that are not needed for the layout at the moment.
        bool blocking = mediaQueryMatches && !m_owner->isAlternate() && m_owner->isCreatedByParser();
        addPendingSheet(blocking ? Blocking : NonBlocking);

        // Load stylesheets that are not needed for the layout immediately with low priority.
        // When the link element is created by scripts, load the stylesheets asynchronously but in high priority.
        bool lowPriority = !mediaQueryMatches || m_owner->isAlternate();
        FetchRequest request = builder.build(lowPriority);
        request.setContentSecurityPolicyNonce(m_owner->fastGetAttribute(HTMLNames::nonceAttr));
        CrossOriginAttributeValue crossOrigin = crossOriginAttributeValue(m_owner->fastGetAttribute(HTMLNames::crossoriginAttr));
        if (crossOrigin != CrossOriginAttributeNotSet) {
            request.setCrossOriginAccessControl(document().getSecurityOrigin(), crossOrigin);
            setFetchFollowingCORS();
        }
        setResource(CSSStyleSheetResource::fetch(request, document().fetcher()));

        if (m_loading && !resource()) {
            // The request may have been denied if (for example) the stylesheet is local and the document is remote, or if there was a Content Security Policy Failure.
            // setCSSStyleSheet() can be called synchronuosly in setResource() and thus resource() is null and |m_loading| is false in such cases even if the request succeeds.
            m_loading = false;
            removePendingSheet();
            notifyLoadedSheetAndAllCriticalSubresources(Node::ErrorOccurredLoadingSubresource);
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        StyleSheet* removedSheet = m_sheet.get();
        clearSheet();
        document().styleEngine().setNeedsActiveStyleUpdate(removedSheet, FullStyleUpdate);
    }
}

void LinkStyle::setSheetTitle(const String& title)
{
    if (m_sheet)
        m_sheet->setTitle(title);
}

void LinkStyle::ownerRemoved()
{
    if (m_sheet)
        clearSheet();

    if (styleSheetIsLoading())
        removePendingSheet();
}

DEFINE_TRACE(LinkStyle)
{
    visitor->trace(m_sheet);
    LinkResource::trace(visitor);
    ResourceOwner<StyleSheetResource>::trace(visitor);
}

} // namespace blink
