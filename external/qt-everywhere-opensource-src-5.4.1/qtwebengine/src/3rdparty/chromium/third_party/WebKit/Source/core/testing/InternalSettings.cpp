/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/testing/InternalSettings.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorController.h"
#include "core/page/Page.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/Supplementable.h"
#include "platform/text/LocaleToScriptMapping.h"

#define InternalSettingsGuardForSettingsReturn(returnValue) \
    if (!settings()) { \
        exceptionState.throwDOMException(InvalidAccessError, "The settings object cannot be obtained."); \
        return returnValue; \
    }

#define InternalSettingsGuardForSettings()  \
    if (!settings()) { \
        exceptionState.throwDOMException(InvalidAccessError, "The settings object cannot be obtained."); \
        return; \
    }

#define InternalSettingsGuardForPage() \
    if (!page()) { \
        exceptionState.throwDOMException(InvalidAccessError, "The page object cannot be obtained."); \
        return; \
    }

namespace WebCore {

InternalSettings::Backup::Backup(Settings* settings)
    : m_originalCSSExclusionsEnabled(RuntimeEnabledFeatures::cssExclusionsEnabled())
    , m_originalAuthorShadowDOMForAnyElementEnabled(RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
    , m_originalCSP(RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled())
    , m_originalOverlayScrollbarsEnabled(RuntimeEnabledFeatures::overlayScrollbarsEnabled())
    , m_originalEditingBehavior(settings->editingBehaviorType())
    , m_originalTextAutosizingEnabled(settings->textAutosizingEnabled())
    , m_originalTextAutosizingWindowSizeOverride(settings->textAutosizingWindowSizeOverride())
    , m_originalAccessibilityFontScaleFactor(settings->accessibilityFontScaleFactor())
    , m_originalMediaTypeOverride(settings->mediaTypeOverride())
    , m_originalMockScrollbarsEnabled(settings->mockScrollbarsEnabled())
    , m_langAttributeAwareFormControlUIEnabled(RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
    , m_imagesEnabled(settings->imagesEnabled())
    , m_defaultVideoPosterURL(settings->defaultVideoPosterURL())
    , m_originalLayerSquashingEnabled(settings->layerSquashingEnabled())
    , m_originalPseudoClassesInMatchingCriteriaInAuthorShadowTreesEnabled(RuntimeEnabledFeatures::pseudoClassesInMatchingCriteriaInAuthorShadowTreesEnabled())
{
}

void InternalSettings::Backup::restoreTo(Settings* settings)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(m_originalCSSExclusionsEnabled);
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(m_originalAuthorShadowDOMForAnyElementEnabled);
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(m_originalCSP);
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_originalOverlayScrollbarsEnabled);
    settings->setEditingBehaviorType(m_originalEditingBehavior);
    settings->setTextAutosizingEnabled(m_originalTextAutosizingEnabled);
    settings->setTextAutosizingWindowSizeOverride(m_originalTextAutosizingWindowSizeOverride);
    settings->setAccessibilityFontScaleFactor(m_originalAccessibilityFontScaleFactor);
    settings->setMediaTypeOverride(m_originalMediaTypeOverride);
    settings->setMockScrollbarsEnabled(m_originalMockScrollbarsEnabled);
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(m_langAttributeAwareFormControlUIEnabled);
    settings->setImagesEnabled(m_imagesEnabled);
    settings->setDefaultVideoPosterURL(m_defaultVideoPosterURL);
    settings->setLayerSquashingEnabled(m_originalLayerSquashingEnabled);
    settings->genericFontFamilySettings().reset();
    RuntimeEnabledFeatures::setPseudoClassesInMatchingCriteriaInAuthorShadowTreesEnabled(m_originalPseudoClassesInMatchingCriteriaInAuthorShadowTreesEnabled);
}

#if ENABLE(OILPAN)
InternalSettings* InternalSettings::from(Page& page)
{
    if (!HeapSupplement<Page>::from(page, supplementName()))
        HeapSupplement<Page>::provideTo(page, supplementName(), new InternalSettings(page));
    return static_cast<InternalSettings*>(HeapSupplement<Page>::from(page, supplementName()));
}
#else
// We can't use RefCountedSupplement because that would try to make InternalSettings RefCounted
// and InternalSettings is already RefCounted via its base class, InternalSettingsGenerated.
// Instead, we manually make InternalSettings supplement Page.
class InternalSettingsWrapper : public Supplement<Page> {
public:
    explicit InternalSettingsWrapper(Page& page)
        : m_internalSettings(InternalSettings::create(page)) { }
    virtual ~InternalSettingsWrapper() { m_internalSettings->hostDestroyed(); }
#if ASSERT_ENABLED
    virtual bool isRefCountedWrapper() const OVERRIDE { return true; }
#endif
    InternalSettings* internalSettings() const { return m_internalSettings.get(); }

private:
    RefPtr<InternalSettings> m_internalSettings;
};

InternalSettings* InternalSettings::from(Page& page)
{
    if (!Supplement<Page>::from(page, supplementName()))
        Supplement<Page>::provideTo(page, supplementName(), adoptPtr(new InternalSettingsWrapper(page)));
    return static_cast<InternalSettingsWrapper*>(Supplement<Page>::from(page, supplementName()))->internalSettings();
}
#endif

const char* InternalSettings::supplementName()
{
    return "InternalSettings";
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Page& page)
    : InternalSettingsGenerated(&page)
    , m_page(&page)
    , m_backup(&page.settings())
{
}

void InternalSettings::resetToConsistentState()
{
    page()->setPageScaleFactor(1, IntPoint(0, 0));

    m_backup.restoreTo(settings());
    m_backup = Backup(settings());
    m_backup.m_originalTextAutosizingEnabled = settings()->textAutosizingEnabled();

    InternalSettingsGenerated::resetToConsistentState();
}

Settings* InternalSettings::settings() const
{
    if (!page())
        return 0;
    return &page()->settings();
}

void InternalSettings::setMockScrollbarsEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setMockScrollbarsEnabled(enabled);
}

void InternalSettings::setAuthorShadowDOMForAnyElementEnabled(bool isEnabled)
{
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(isEnabled);
}

void InternalSettings::setExperimentalContentSecurityPolicyFeaturesEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(enabled);
}

void InternalSettings::setPseudoClassesInMatchingCriteriaInAuthorShadowTreesEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setPseudoClassesInMatchingCriteriaInAuthorShadowTreesEnabled(enabled);
}

void InternalSettings::setOverlayScrollbarsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(enabled);
}

void InternalSettings::setViewportEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setViewportEnabled(enabled);
}

// FIXME: This is a temporary flag and should be removed once squashing is
// ready (crbug.com/261605).
void InternalSettings::setLayerSquashingEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setLayerSquashingEnabled(enabled);
}

void InternalSettings::setStandardFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setStandard(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setSerifFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setSerif(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setSansSerifFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setSansSerif(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setFixedFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setFixed(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setCursiveFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setCursive(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setFantasyFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setFantasy(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setPictographFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setPictograph(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setTextAutosizingEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingEnabled(enabled);
    m_page->inspectorController().setTextAutosizingEnabled(enabled);
}

void InternalSettings::setTextAutosizingWindowSizeOverride(int width, int height, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingWindowSizeOverride(IntSize(width, height));
}

void InternalSettings::setMediaTypeOverride(const String& mediaType, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setMediaTypeOverride(mediaType);
}

void InternalSettings::setAccessibilityFontScaleFactor(float fontScaleFactor, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setAccessibilityFontScaleFactor(fontScaleFactor);
}

void InternalSettings::setCSSExclusionsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(enabled);
}

void InternalSettings::setEditingBehavior(const String& editingBehavior, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    if (equalIgnoringCase(editingBehavior, "win"))
        settings()->setEditingBehaviorType(EditingWindowsBehavior);
    else if (equalIgnoringCase(editingBehavior, "mac"))
        settings()->setEditingBehaviorType(EditingMacBehavior);
    else if (equalIgnoringCase(editingBehavior, "unix"))
        settings()->setEditingBehaviorType(EditingUnixBehavior);
    else if (equalIgnoringCase(editingBehavior, "android"))
        settings()->setEditingBehaviorType(EditingAndroidBehavior);
    else
        exceptionState.throwDOMException(SyntaxError, "The editing behavior type provided ('" + editingBehavior + "') is invalid.");
}

void InternalSettings::setLangAttributeAwareFormControlUIEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(enabled);
}

void InternalSettings::setImagesEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setImagesEnabled(enabled);
}

void InternalSettings::setDefaultVideoPosterURL(const String& url, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setDefaultVideoPosterURL(url);
}

void InternalSettings::trace(Visitor* visitor)
{
    visitor->trace(m_page);
    InternalSettingsGenerated::trace(visitor);
#if ENABLE(OILPAN)
    HeapSupplement<Page>::trace(visitor);
#endif
}

}
