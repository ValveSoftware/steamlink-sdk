// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEditor_h
#define LayoutEditor_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSRuleList.h"
#include "core/dom/Element.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSPrimitiveValue;
class InspectorCSSAgent;
class InspectorDOMAgent;
class ScriptController;

class CORE_EXPORT LayoutEditor final : public GarbageCollectedFinalized<LayoutEditor> {
public:
    static LayoutEditor* create(Element* element, InspectorCSSAgent* cssAgent, InspectorDOMAgent* domAgent, ScriptController* scriptController)
    {
        return new LayoutEditor(element, cssAgent, domAgent, scriptController);
    }

    ~LayoutEditor();
    void dispose();

    Element* element() { return m_element.get(); }
    void overlayStartedPropertyChange(const String&);
    void overlayPropertyChanged(float);
    void overlayEndedPropertyChange();
    void commitChanges();
    void nextSelector();
    void previousSelector();
    void rebuild();
    DECLARE_TRACE();

private:
    LayoutEditor(Element*, InspectorCSSAgent*, InspectorDOMAgent*, ScriptController*);
    const CSSPrimitiveValue* getPropertyCSSValue(CSSPropertyID) const;
    std::unique_ptr<protocol::DictionaryValue> createValueDescription(const String&);
    void appendAnchorFor(protocol::ListValue*, const String&, const String&);
    bool setCSSPropertyValueInCurrentRule(const String&);
    void editableSelectorUpdated(bool hasChanged) const;
    void evaluateInOverlay(const String&, std::unique_ptr<protocol::Value>) const;
    std::unique_ptr<protocol::DictionaryValue> currentSelectorInfo(CSSStyleDeclaration*) const;
    bool growInside(String propertyName, const CSSPrimitiveValue*);

    Member<Element> m_element;
    Member<InspectorCSSAgent> m_cssAgent;
    Member<InspectorDOMAgent> m_domAgent;
    Member<ScriptController> m_scriptController;
    CSSPropertyID m_changingProperty;
    float m_propertyInitialValue;
    float m_factor;
    CSSPrimitiveValue::UnitType m_valueUnitType;
    bool m_isDirty;

    HeapVector<Member<CSSStyleDeclaration>> m_matchedStyles;
    HashMap<String, bool> m_growsInside;
    unsigned m_currentRuleIndex;
};

} // namespace blink


#endif // !defined(LayoutEditor_h)
