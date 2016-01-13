/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SearchInputType_h
#define SearchInputType_h

#include "core/html/forms/BaseTextInputType.h"
#include "platform/Timer.h"

namespace WebCore {

class SearchFieldCancelButtonElement;
class SearchFieldDecorationElement;

class SearchInputType FINAL : public BaseTextInputType {
public:
    static PassRefPtrWillBeRawPtr<InputType> create(HTMLInputElement&);

    void stopSearchEventTimer();

private:
    SearchInputType(HTMLInputElement&);
    virtual void countUsage() OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*) const OVERRIDE;
    virtual const AtomicString& formControlType() const OVERRIDE;
    virtual bool shouldRespectSpeechAttribute() OVERRIDE;
    virtual bool isSearchField() const OVERRIDE;
    virtual bool needsContainer() const OVERRIDE;
    virtual void createShadowSubtree() OVERRIDE;
    virtual void handleKeydownEvent(KeyboardEvent*) OVERRIDE;
    virtual void didSetValueByUserEdit(ValueChangeState) OVERRIDE;
    virtual bool supportsInputModeAttribute() const OVERRIDE;
    virtual void updateView() OVERRIDE;

    void searchEventTimerFired(Timer<SearchInputType>*);
    bool searchEventsShouldBeDispatched() const;
    void startSearchEventTimer();
    void updateCancelButtonVisibility();

    Timer<SearchInputType> m_searchEventTimer;
};

} // namespace WebCore

#endif // SearchInputType_h
