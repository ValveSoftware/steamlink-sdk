/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef RENDER_WIDGET_HOST_VIEW_QT_H
#define RENDER_WIDGET_HOST_VIEW_QT_H

#include "render_widget_host_view_qt_delegate.h"

#include "base/memory/weak_ptr.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/resources/transferable_resource.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/view_messages.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "qtwebenginecoreglobal_p.h"
#include <QMap>
#include <QPoint>
#include <QRect>
#include <QtGlobal>
#include <QtGui/qaccessible.h>
#include <QtGui/QTouchEvent>

#include "delegated_frame_node.h"

QT_BEGIN_NAMESPACE
class QEvent;
class QFocusEvent;
class QHoverEvent;
class QKeyEvent;
class QMouseEvent;
class QVariant;
class QWheelEvent;
class QAccessibleInterface;
QT_END_NAMESPACE

class WebContentsAdapterClient;

namespace content {
class RenderFrameHost;
class RenderWidgetHostImpl;
}

namespace QtWebEngineCore {

struct MultipleMouseClickHelper
{
    QPoint lastPressPosition;
    Qt::MouseButton lastPressButton;
    int clickCounter;
    ulong lastPressTimestamp;

    MultipleMouseClickHelper()
        : lastPressPosition(QPoint())
        , lastPressButton(Qt::NoButton)
        , clickCounter(0)
        , lastPressTimestamp(0)
    {
    }
};

class RenderWidgetHostViewQt
    : public content::RenderWidgetHostViewBase
    , public ui::GestureProviderClient
    , public RenderWidgetHostViewQtDelegateClient
    , public base::SupportsWeakPtr<RenderWidgetHostViewQt>
    , public cc::BeginFrameObserverBase
#ifndef QT_NO_ACCESSIBILITY
    , public QAccessible::ActivationObserver
#endif // QT_NO_ACCESSIBILITY
    , public content::TextInputManager::Observer
{
public:
    enum LoadVisuallyCommittedState {
        NotCommitted,
        DidFirstVisuallyNonEmptyPaint,
        DidFirstCompositorFrameSwap
    };

    RenderWidgetHostViewQt(content::RenderWidgetHost* widget);
    ~RenderWidgetHostViewQt();

    void setDelegate(RenderWidgetHostViewQtDelegate *delegate);
    void setAdapterClient(WebContentsAdapterClient *adapterClient);

    void InitAsChild(gfx::NativeView) override;
    void InitAsPopup(content::RenderWidgetHostView*, const gfx::Rect&) override;
    void InitAsFullscreen(content::RenderWidgetHostView*) override;
    content::RenderWidgetHost* GetRenderWidgetHost() const override;
    void SetSize(const gfx::Size& size) override;
    void SetBounds(const gfx::Rect&) override;
    gfx::Vector2dF GetLastScrollOffset() const override;
    gfx::Size GetPhysicalBackingSize() const override;
    gfx::NativeView GetNativeView() const override;
    gfx::NativeViewAccessible GetNativeViewAccessible() override;
    void Focus() override;
    bool HasFocus() const override;
    bool IsSurfaceAvailableForCopy() const override;
    void Show() override;
    void Hide() override;
    bool IsShowing() override;
    gfx::Rect GetViewBounds() const override;
    void SetBackgroundColor(SkColor color) override;
    bool LockMouse() override;
    void UnlockMouse() override;
    void UpdateCursor(const content::WebCursor&) override;
    void SetIsLoading(bool) override;
    void ImeCancelComposition() override;
    void ImeCompositionRangeChanged(const gfx::Range&, const std::vector<gfx::Rect>&) override;
    void RenderProcessGone(base::TerminationStatus, int) override;
    void Destroy() override;
    void SetTooltipText(const base::string16 &tooltip_text) override;
    void CopyFromCompositingSurface(const gfx::Rect& src_subrect, const gfx::Size& dst_size, const content::ReadbackRequestCallback& callback, const SkColorType preferred_color_type) override;
    void CopyFromCompositingSurfaceToVideoFrame(const gfx::Rect& src_subrect, const scoped_refptr<media::VideoFrame>& target, const base::Callback<void(const gfx::Rect&, bool)>& callback) override;

    bool CanCopyToVideoFrame() const override;
    bool HasAcceleratedSurface(const gfx::Size&) override;
    void OnSwapCompositorFrame(uint32_t output_surface_id, cc::CompositorFrame frame)  override;

    void GetScreenInfo(content::ScreenInfo* results);
    gfx::Rect GetBoundsInRootWindow() override;
    void ProcessAckedTouchEvent(const content::TouchEventWithLatencyInfo &touch, content::InputEventAckState ack_result) override;
    void ClearCompositorFrame() override;
    void LockCompositingSurface() override;
    void UnlockCompositingSurface() override;
    void SetNeedsBeginFrames(bool needs_begin_frames) override;

    // Overridden from ui::GestureProviderClient.
    void OnGestureEvent(const ui::GestureEventData& gesture) override;

    // Overridden from RenderWidgetHostViewQtDelegateClient.
    QSGNode *updatePaintNode(QSGNode *) override;
    void notifyResize() override;
    void notifyShown() override;
    void notifyHidden() override;
    void windowBoundsChanged() override;
    void windowChanged() override;
    bool forwardEvent(QEvent *) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) override;

    // Overridden from content::TextInputManager::Observer
    void OnUpdateTextInputStateCalled(content::TextInputManager *text_input_manager, RenderWidgetHostViewBase *updated_view, bool did_update_state) override;
    void OnSelectionBoundsChanged(content::TextInputManager *text_input_manager, RenderWidgetHostViewBase *updated_view) override;
    void OnTextSelectionChanged(content::TextInputManager *text_input_manager, RenderWidgetHostViewBase *updated_view) override;

    // cc::BeginFrameObserverBase implementation.
    bool OnBeginFrameDerivedImpl(const cc::BeginFrameArgs& args) override;
    void OnBeginFrameSourcePausedChanged(bool paused) override;

    void handleMouseEvent(QMouseEvent*);
    void handleKeyEvent(QKeyEvent*);
    void handleWheelEvent(QWheelEvent*);
    void handleTouchEvent(QTouchEvent*);
    void handleGestureEvent(QNativeGestureEvent *);
    void handleHoverEvent(QHoverEvent*);
    void handleFocusEvent(QFocusEvent*);
    void handleInputMethodEvent(QInputMethodEvent*);
    void handleInputMethodQueryEvent(QInputMethodQueryEvent*);

#if defined(OS_MACOSX)
    void SetActive(bool active) override { QT_NOT_YET_IMPLEMENTED }
    bool IsSpeaking() const override { QT_NOT_YET_IMPLEMENTED; return false; }
    void SpeakSelection() override { QT_NOT_YET_IMPLEMENTED }
    void StopSpeaking() override { QT_NOT_YET_IMPLEMENTED }
    bool SupportsSpeech() const override { QT_NOT_YET_IMPLEMENTED; return false; }
    void ShowDefinitionForSelection() override { QT_NOT_YET_IMPLEMENTED }
    ui::AcceleratedWidgetMac *GetAcceleratedWidgetMac() const override { QT_NOT_YET_IMPLEMENTED; return nullptr; }
#endif // defined(OS_MACOSX)


    // Overridden from content::BrowserAccessibilityDelegate
    content::BrowserAccessibilityManager* CreateBrowserAccessibilityManager(content::BrowserAccessibilityDelegate* delegate, bool for_root_frame) override;
#ifndef QT_NO_ACCESSIBILITY
    void accessibilityActiveChanged(bool active) override;
#endif // QT_NO_ACCESSIBILITY
    LoadVisuallyCommittedState getLoadVisuallyCommittedState() const { return m_loadVisuallyCommittedState; }
    void setLoadVisuallyCommittedState(LoadVisuallyCommittedState state) { m_loadVisuallyCommittedState = state; }

    gfx::SizeF lastContentsSize() const { return m_lastContentsSize; }

private:
    void sendDelegatedFrameAck();
    void processMotionEvent(const ui::MotionEvent &motionEvent);
    void clearPreviousTouchMotionState();
    QList<QTouchEvent::TouchPoint> mapTouchPointIds(const QList<QTouchEvent::TouchPoint> &inputPoints);
    float dpiScale() const;
    void updateNeedsBeginFramesInternal();

    bool IsPopup() const;

    void selectionChanged();
    content::RenderFrameHost *getFocusedFrameHost();
    ui::TextInputType getTextInputType() const;

    content::RenderWidgetHostImpl *m_host;
    ui::FilteredGestureProvider m_gestureProvider;
    base::TimeDelta m_eventsToNowDelta;
    bool m_sendMotionActionDown;
    bool m_touchMotionStarted;
    QMap<int, int> m_touchIdMapping;
    QList<QTouchEvent::TouchPoint> m_previousTouchPoints;
    std::unique_ptr<RenderWidgetHostViewQtDelegate> m_delegate;

    QExplicitlySharedDataPointer<ChromiumCompositorData> m_chromiumCompositorData;
    cc::ReturnedResourceArray m_resourcesToRelease;
    bool m_needsDelegatedFrameAck;
    LoadVisuallyCommittedState m_loadVisuallyCommittedState;
    uint32_t m_pendingOutputSurfaceId;

    QMetaObject::Connection m_adapterClientDestroyedConnection;
    WebContentsAdapterClient *m_adapterClient;
    MultipleMouseClickHelper m_clickHelper;

    bool m_imeInProgress;
    bool m_receivedEmptyImeText;
    QPoint m_previousMousePosition;

    bool m_initPending;

    std::unique_ptr<cc::SyntheticBeginFrameSource> m_beginFrameSource;
    bool m_needsBeginFrames;
    bool m_addedFrameObserver;

    gfx::Vector2dF m_lastScrollOffset;
    gfx::SizeF m_lastContentsSize;

    uint m_imState;
    int m_anchorPositionWithinSelection;
    int m_cursorPositionWithinSelection;
    uint m_cursorPosition;
    bool m_emptyPreviousSelection;
    QString m_surroundingText;
};

} // namespace QtWebEngineCore

#endif // RENDER_WIDGET_HOST_VIEW_QT_H
