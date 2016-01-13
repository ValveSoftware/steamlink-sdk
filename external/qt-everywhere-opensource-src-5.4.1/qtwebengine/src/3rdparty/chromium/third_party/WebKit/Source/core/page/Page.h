/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef Page_h
#define Page_h

#include "core/dom/ViewportDescription.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SettingsDelegate.h"
#include "core/frame/UseCounter.h"
#include "core/page/PageAnimator.h"
#include "core/page/PageVisibilityState.h"
#include "platform/LifecycleContext.h"
#include "platform/Supplementable.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/Region.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class AutoscrollController;
class BackForwardClient;
class Chrome;
class ChromeClient;
class ClientRectList;
class ContextMenuClient;
class ContextMenuController;
class Document;
class DragCaretController;
class DragClient;
class DragController;
class EditorClient;
class FocusController;
class Frame;
class FrameHost;
class HistoryItem;
class InspectorClient;
class InspectorController;
class PageLifecycleNotifier;
class PlatformMouseEvent;
class PluginData;
class PointerLockController;
class Range;
class RenderBox;
class RenderObject;
class RenderTheme;
class StorageClient;
class VisibleSelection;
class ScrollableArea;
class ScrollingCoordinator;
class Settings;
class SpellCheckerClient;
class StorageNamespace;
class UndoStack;
class ValidationMessageClient;

typedef uint64_t LinkHash;

float deviceScaleFactor(LocalFrame*);

class Page FINAL : public NoBaseWillBeGarbageCollectedFinalized<Page>, public WillBeHeapSupplementable<Page>, public LifecycleContext<Page>, public SettingsDelegate {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Page);
    WTF_MAKE_NONCOPYABLE(Page);
    friend class Settings;
public:
    static void scheduleForcedStyleRecalcForAllPages();

    // It is up to the platform to ensure that non-null clients are provided where required.
    struct PageClients {
        WTF_MAKE_NONCOPYABLE(PageClients); WTF_MAKE_FAST_ALLOCATED;
    public:
        PageClients();
        ~PageClients();

        ChromeClient* chromeClient;
        ContextMenuClient* contextMenuClient;
        EditorClient* editorClient;
        DragClient* dragClient;
        InspectorClient* inspectorClient;
        BackForwardClient* backForwardClient;
        SpellCheckerClient* spellCheckerClient;
        StorageClient* storageClient;
    };

    explicit Page(PageClients&);
    virtual ~Page();

    void makeOrdinary();

    // This method returns all pages, incl. private ones associated with
    // inspector overlay, popups, SVGImage, etc.
    static HashSet<Page*>& allPages();
    // This method returns all ordinary pages.
    static HashSet<Page*>& ordinaryPages();

    FrameHost& frameHost() const { return *m_frameHost; }

    void setNeedsRecalcStyleInAllFrames();
    void updateAcceleratedCompositingSettings();

    ViewportDescription viewportDescription() const;

    static void refreshPlugins(bool reload);
    PluginData* pluginData() const;

    EditorClient& editorClient() const { return *m_editorClient; }
    SpellCheckerClient& spellCheckerClient() const { return *m_spellCheckerClient; }
    UndoStack& undoStack() const { return *m_undoStack; }

    void setMainFrame(Frame*);
    Frame* mainFrame() const { return m_mainFrame; }
    // Escape hatch for existing code that assumes that the root frame is
    // always a LocalFrame. With OOPI, this is not always the case. Code that
    // depends on this will generally have to be rewritten to propagate any
    // necessary state through all renderer processes for that page and/or
    // coordinate/rely on the browser process to help dispatch/coordinate work.
    LocalFrame* deprecatedLocalMainFrame() const { return toLocalFrame(m_mainFrame); }

    void documentDetached(Document*);

    bool openedByDOM() const;
    void setOpenedByDOM();

    void incrementSubframeCount() { ++m_subframeCount; }
    void decrementSubframeCount() { ASSERT(m_subframeCount); --m_subframeCount; }
    int subframeCount() const { checkSubframeCountConsistency(); return m_subframeCount; }

    PageAnimator& animator() { return m_animator; }
    Chrome& chrome() const { return *m_chrome; }
    AutoscrollController& autoscrollController() const { return *m_autoscrollController; }
    DragCaretController& dragCaretController() const { return *m_dragCaretController; }
    DragController& dragController() const { return *m_dragController; }
    FocusController& focusController() const { return *m_focusController; }
    ContextMenuController& contextMenuController() const { return *m_contextMenuController; }
    InspectorController& inspectorController() const { return *m_inspectorController; }
    PointerLockController& pointerLockController() const { return *m_pointerLockController; }
    ValidationMessageClient& validationMessageClient() const { return *m_validationMessageClient; }
    void setValidationMessageClient(PassOwnPtrWillBeRawPtr<ValidationMessageClient>);

    ScrollingCoordinator* scrollingCoordinator();

    String mainThreadScrollingReasonsAsText();
    PassRefPtrWillBeRawPtr<ClientRectList> nonFastScrollableRects(const LocalFrame*);

    Settings& settings() const { return *m_settings; }
    BackForwardClient& backForward() const { return *m_backForwardClient; }

    UseCounter& useCounter() { return m_useCounter; }

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    void unmarkAllTextMatches();

    // DefersLoading is used to delay loads during modal dialogs.
    // Modal dialogs are supposed to freeze all background processes
    // in the page, including prevent additional loads from staring/continuing.
    void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    void setPageScaleFactor(float scale, const IntPoint& origin);
    float pageScaleFactor() const;

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float);

    static void allVisitedStateChanged();
    static void visitedStateChanged(LinkHash visitedHash);

    StorageNamespace* sessionStorage(bool optionalCreate = true);
    StorageClient& storageClient() const { return *m_storageClient; }

    // Don't allow more than a certain number of frames in a page.
    // This seems like a reasonable upper bound, and otherwise mutually
    // recursive frameset pages can quickly bring the program to its knees
    // with exponential growth in the number of frames.
    static const int maxNumberOfFrames = 1000;

    PageVisibilityState visibilityState() const;
    void setVisibilityState(PageVisibilityState, bool);

    bool isCursorVisible() const;
    void setIsCursorVisible(bool isVisible) { m_isCursorVisible = isVisible; }

#ifndef NDEBUG
    void setIsPainting(bool painting) { m_isPainting = painting; }
    bool isPainting() const { return m_isPainting; }
#endif

    double timerAlignmentInterval() const;

    class MultisamplingChangedObserver : public WillBeGarbageCollectedMixin {
    public:
        virtual void multisamplingChanged(bool) = 0;
    };

    void addMultisamplingChangedObserver(MultisamplingChangedObserver*);
    void removeMultisamplingChangedObserver(MultisamplingChangedObserver*);

    void didCommitLoad(LocalFrame*);

    void acceptLanguagesChanged();

    static void networkStateChanged(bool online);
    PassOwnPtr<LifecycleNotifier<Page> > createLifecycleNotifier();

    void trace(Visitor*);
    void willBeDestroyed();

protected:
    PageLifecycleNotifier& lifecycleNotifier();

private:
    void initGroup();

#if ASSERT_ENABLED
    void checkSubframeCountConsistency() const;
#else
    void checkSubframeCountConsistency() const { }
#endif

    void setTimerAlignmentInterval(double);

    void setNeedsLayoutInAllFrames();

    // SettingsDelegate overrides.
    virtual void settingsChanged(SettingsDelegate::ChangeType) OVERRIDE;

    PageAnimator m_animator;
    const OwnPtr<AutoscrollController> m_autoscrollController;
    const OwnPtr<Chrome> m_chrome;
    const OwnPtrWillBeMember<DragCaretController> m_dragCaretController;
    const OwnPtrWillBeMember<DragController> m_dragController;
    const OwnPtr<FocusController> m_focusController;
    const OwnPtr<ContextMenuController> m_contextMenuController;
    const OwnPtr<InspectorController> m_inspectorController;
    const OwnPtrWillBeMember<PointerLockController> m_pointerLockController;
    OwnPtr<ScrollingCoordinator> m_scrollingCoordinator;
    const OwnPtrWillBeMember<UndoStack> m_undoStack;

    // Typically, the main frame and Page should both be owned by the embedder,
    // which must call Page::willBeDestroyed() prior to destroying Page. This
    // call detaches the main frame and clears this pointer, thus ensuring that
    // this field only references a live main frame.
    //
    // However, there are several locations (InspectorOverlay, SVGImage, and
    // WebPagePopupImpl) which don't hold a reference to the main frame at all
    // after creating it. These are still safe because they always create a
    // Frame with a FrameView. FrameView and Frame hold references to each
    // other, thus keeping each other alive. The call to willBeDestroyed()
    // breaks this cycle, so the frame is still properly destroyed once no
    // longer needed.
    Frame* m_mainFrame;

    mutable RefPtr<PluginData> m_pluginData;

    BackForwardClient* m_backForwardClient;
    EditorClient* const m_editorClient;
    SpellCheckerClient* const m_spellCheckerClient;
    StorageClient* m_storageClient;
    OwnPtrWillBeMember<ValidationMessageClient> m_validationMessageClient;

    UseCounter m_useCounter;

    int m_subframeCount;
    bool m_openedByDOM;

    bool m_tabKeyCyclesThroughElements;
    bool m_defersLoading;

    float m_deviceScaleFactor;

    OwnPtr<StorageNamespace> m_sessionStorage;

    double m_timerAlignmentInterval;

    PageVisibilityState m_visibilityState;

    bool m_isCursorVisible;

#ifndef NDEBUG
    bool m_isPainting;
#endif

    WillBeHeapHashSet<RawPtrWillBeWeakMember<MultisamplingChangedObserver> > m_multisamplingChangedObservers;

    // A pointer to all the interfaces provided to in-process Frames for this Page.
    // FIXME: Most of the members of Page should move onto FrameHost.
    OwnPtrWillBeMember<FrameHost> m_frameHost;
};

} // namespace WebCore

#endif // Page_h
