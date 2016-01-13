/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VTTCue_h
#define VTTCue_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/track/TextTrackCue.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class Document;
class ExecutionContext;
class VTTCue;
class VTTScanner;

class VTTCueBox FINAL : public HTMLDivElement {
public:
    static PassRefPtrWillBeRawPtr<VTTCueBox> create(Document& document, VTTCue* cue)
    {
        return adoptRefWillBeNoop(new VTTCueBox(document, cue));
    }

    VTTCue* getCue() const { return m_cue; }
    void applyCSSProperties(const IntSize& videoSize);

    virtual void trace(Visitor*) OVERRIDE;

private:
    VTTCueBox(Document&, VTTCue*);

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    RawPtrWillBeMember<VTTCue> m_cue;
};

class VTTCue FINAL : public TextTrackCue, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<VTTCue> create(Document& document, double startTime, double endTime, const String& text)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new VTTCue(document, startTime, endTime, text));
    }

    virtual ~VTTCue();

    const String& vertical() const;
    void setVertical(const String&);

    bool snapToLines() const { return m_snapToLines; }
    void setSnapToLines(bool);

    int line() const { return m_linePosition; }
    void setLine(int, ExceptionState&);

    int position() const { return m_textPosition; }
    void setPosition(int, ExceptionState&);

    int size() const { return m_cueSize; }
    void setSize(int, ExceptionState&);

    const String& align() const;
    void setAlign(const String&);

    const String& text() const { return m_text; }
    void setText(const String&);

    void parseSettings(const String&);

    PassRefPtrWillBeRawPtr<DocumentFragment> getCueAsHTML();
    PassRefPtrWillBeRawPtr<DocumentFragment> createCueRenderingTree();

    const String& regionId() const { return m_regionId; }
    void setRegionId(const String&);

    virtual void updateDisplay(const IntSize& videoSize, HTMLDivElement& container) OVERRIDE;

    virtual void updateDisplayTree(double movieTime) OVERRIDE;
    virtual void removeDisplayTree() OVERRIDE;
    virtual void notifyRegionWhenRemovingDisplayTree(bool notifyRegion) OVERRIDE;

    void markFutureAndPastNodes(ContainerNode*, double previousTimestamp, double movieTime);

    int calculateComputedLinePosition();

    std::pair<double, double> getCSSPosition() const;

    CSSValueID getCSSAlignment() const;
    int getCSSSize() const;
    CSSValueID getCSSWritingDirection() const;
    CSSValueID getCSSWritingMode() const;

    enum WritingDirection {
        Horizontal = 0,
        VerticalGrowingLeft,
        VerticalGrowingRight,
        NumberOfWritingDirections
    };
    WritingDirection getWritingDirection() const { return m_writingDirection; }

    enum CueAlignment {
        Start = 0,
        Middle,
        End,
        Left,
        Right,
        NumberOfAlignments
    };
    CueAlignment getAlignment() const { return m_cueAlignment; }

    virtual ExecutionContext* executionContext() const OVERRIDE;

#ifndef NDEBUG
    virtual String toString() const OVERRIDE;
#endif

    virtual void trace(Visitor*) OVERRIDE;

private:
    VTTCue(Document&, double startTime, double endTime, const String& text);

    Document& document() const;

    VTTCueBox& ensureDisplayTree();
    PassRefPtrWillBeRawPtr<VTTCueBox> getDisplayTree(const IntSize& videoSize);

    virtual void cueDidChange() OVERRIDE;

    void createVTTNodeTree();
    void copyVTTNodeToDOMTree(ContainerNode* vttNode, ContainerNode* root);

    std::pair<double, double> getPositionCoordinates() const;

    void calculateDisplayParameters();

    enum CueSetting {
        None,
        Vertical,
        Line,
        Position,
        Size,
        Align,
        RegionId
    };
    CueSetting settingName(VTTScanner&);

    String m_text;
    int m_linePosition;
    int m_computedLinePosition;
    int m_textPosition;
    int m_cueSize;
    WritingDirection m_writingDirection;
    CueAlignment m_cueAlignment;
    String m_regionId;

    RefPtrWillBeMember<DocumentFragment> m_vttNodeTree;
    RefPtrWillBeMember<HTMLDivElement> m_cueBackgroundBox;
    RefPtrWillBeMember<VTTCueBox> m_displayTree;

    CSSValueID m_displayDirection;
    int m_displaySize;
    std::pair<float, float> m_displayPosition;

    bool m_snapToLines : 1;
    bool m_displayTreeShouldChange : 1;
    bool m_notifyRegion : 1;
};

// VTTCue is currently the only TextTrackCue subclass.
DEFINE_TYPE_CASTS(VTTCue, TextTrackCue, cue, true, true);

} // namespace WebCore

#endif
