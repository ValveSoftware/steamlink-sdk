// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxy_h
#define CompositorProxy_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/CompositorProxyClient.h"
#include "core/dom/DOMMatrix.h"
#include "core/dom/Element.h"
#include "platform/graphics/CompositorMutableState.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DOMMatrix;
class ExceptionState;
class ExecutionContext;

class CORE_EXPORT CompositorProxy final : public GarbageCollectedFinalized<CompositorProxy>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static CompositorProxy* create(ExecutionContext*, Element*, const Vector<String>& attributeArray, ExceptionState&);
    static CompositorProxy* create(ExecutionContext*, uint64_t element, uint32_t compositorMutableProperties);
    virtual ~CompositorProxy();

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_client);
    }

    uint64_t elementId() const { return m_elementId; }
    uint32_t compositorMutableProperties() const { return m_compositorMutableProperties; }
    bool supports(const String& attribute) const;

    bool initialized() const { return m_connected && m_state.get(); }
    bool connected() const { return m_connected; }
    void disconnect();

    double opacity(ExceptionState&) const;
    double scrollLeft(ExceptionState&) const;
    double scrollTop(ExceptionState&) const;
    DOMMatrix* transform(ExceptionState&) const;

    void setOpacity(double, ExceptionState&);
    void setScrollLeft(double, ExceptionState&);
    void setScrollTop(double, ExceptionState&);
    void setTransform(DOMMatrix*, ExceptionState&);

    void takeCompositorMutableState(std::unique_ptr<CompositorMutableState>);

protected:
    CompositorProxy(uint64_t elementId, uint32_t compositorMutableProperties);
    CompositorProxy(Element&, const Vector<String>& attributeArray);
    CompositorProxy(uint64_t element, uint32_t compositorMutableProperties, CompositorProxyClient*);

private:
    bool raiseExceptionIfNotMutable(uint32_t compositorMutableProperty, ExceptionState&) const;
    void disconnectInternal();

    const uint64_t m_elementId = 0;
    const uint32_t m_compositorMutableProperties = 0;

    bool m_connected = true;
    Member<CompositorProxyClient> m_client;
    std::unique_ptr<CompositorMutableState> m_state;
};

} // namespace blink

#endif // CompositorProxy_h
