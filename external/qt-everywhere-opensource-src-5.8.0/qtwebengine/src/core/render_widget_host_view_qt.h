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
#include "cc/resources/transferable_resource.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
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
#ifndef QT_NO_ACCESSIBILITY
    , public QAccessible::ActivationObserver
#endif // QT_NO_ACCESSIBILITY
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

    virtual void InitAsChild(gfx::NativeView) Q_DECL_OVERRIDE;
    virtual void InitAsPopup(content::RenderWidgetHostView*, const gfx::Rect&) Q_DECL_OVERRIDE;
    virtual void InitAsFullscreen(content::RenderWidgetHostView*) Q_DECL_OVERRIDE;
    virtual content::RenderWidgetHost* GetRenderWidgetHost() const Q_DECL_OVERRIDE;
    virtual void SetSize(const gfx::Size& size) Q_DECL_OVERRIDE;
    virtual void SetBounds(const gfx::Rect&) Q_DECL_OVERRIDE;
    virtual gfx::Vector2dF GetLastScrollOffset() const Q_DECL_OVERRIDE;
    virtual gfx::Size GetPhysicalBackingSize() const Q_DECL_OVERRIDE;
    virtual gfx::NativeView GetNativeView() const Q_DECL_OVERRIDE;
    virtual gfx::NativeViewAccessible GetNativeViewAccessible() Q_DECL_OVERRIDE;
    virtual void Focus() Q_DECL_OVERRIDE;
    virtual bool HasFocus() const Q_DECL_OVERRIDE;
    virtual bool IsSurfaceAvailableForCopy() const Q_DECL_OVERRIDE;
    virtual void Show() Q_DECL_OVERRIDE;
    virtual void Hide() Q_DECL_OVERRIDE;
    virtual bool IsShowing() Q_DECL_OVERRIDE;
    virtual gfx::Rect GetViewBounds() const Q_DECL_OVERRIDE;
    virtual void SetBackgroundColor(SkColor color) Q_DECL_OVERRIDE;
    virtual bool LockMouse() Q_DECL_OVERRIDE;
    virtual void UnlockMouse() Q_DECL_OVERRIDE;
    virtual void UpdateCursor(const content::WebCursor&) Q_DECL_OVERRIDE;
    virtual void SetIsLoading(bool) Q_DECL_OVERRIDE;
    virtual void TextInputStateChanged(const content::TextInputState& params) Q_DECL_OVERRIDE;
    virtual void ImeCancelComposition() Q_DECL_OVERRIDE;
    virtual void ImeCompositionRangeChanged(const gfx::Range&, const std::vector<gfx::Rect>&) Q_DECL_OVERRIDE;
    virtual void RenderProcessGone(base::TerminationStatus, int) Q_DECL_OVERRIDE;
    virtual void Destroy() Q_DECL_OVERRIDE;
    virtual void SetTooltipText(const base::string16 &tooltip_text) Q_DECL_OVERRIDE;
    virtual void SelectionBoundsChanged(const ViewHostMsg_SelectionBounds_Params&) Q_DECL_OVERRIDE;
    virtual void CopyFromCompositingSurface(const gfx::Rect& src_subrect, const gfx::Size& dst_size, const content::ReadbackRequestCallback& callback, const SkColorType preferred_color_type) Q_DECL_OVERRIDE;
    virtual void CopyFromCompositingSurfaceToVideoFrame(const gfx::Rect& src_subrect, const scoped_refptr<media::VideoFrame>& target, const base::Callback<void(const gfx::Rect&, bool)>& callback) Q_DECL_OVERRIDE;

    virtual bool CanCopyToVideoFrame() const Q_DECL_OVERRIDE;
    virtual bool HasAcceleratedSurface(const gfx::Size&) Q_DECL_OVERRIDE;
    virtual void OnSwapCompositorFrame(uint32_t output_surface_id, cc::CompositorFrame frame)  Q_DECL_OVERRIDE;

    virtual void GetScreenInfo(blink::WebScreenInfo* results) Q_DECL_OVERRIDE;
    virtual gfx::Rect GetBoundsInRootWindow() Q_DECL_OVERRIDE;
    virtual void ProcessAckedTouchEvent(const content::TouchEventWithLatencyInfo &touch, content::InputEventAckState ack_result) Q_DECL_OVERRIDE;
    virtual void ClearCompositorFrame() Q_DECL_OVERRIDE;
    virtual void LockCompositingSurface() Q_DECL_OVERRIDE;
    virtual void UnlockCompositingSurface() Q_DECL_OVERRIDE;

    // Overridden from RenderWidgetHostViewBase.
    virtual void SelectionChanged(const base::string16 &text, size_t offset, const gfx::Range &range) Q_DECL_OVERRIDE;

    // Overridden from ui::GestureProviderClient.
    virtual void OnGestureEvent(const ui::GestureEventData& gesture) Q_DECL_OVERRIDE;

    // Overridden from RenderWidgetHostViewQtDelegateClient.
    virtual QSGNode *updatePaintNode(QSGNode *) Q_DECL_OVERRIDE;
    virtual void notifyResize() Q_DECL_OVERRIDE;
    virtual void notifyShown() Q_DECL_OVERRIDE;
    virtual void notifyHidden() Q_DECL_OVERRIDE;
    virtual void windowBoundsChanged() Q_DECL_OVERRIDE;
    virtual void windowChanged() Q_DECL_OVERRIDE;
    virtual bool forwardEvent(QEvent *) Q_DECL_OVERRIDE;
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const Q_DECL_OVERRIDE;

    void handleMouseEvent(QMouseEvent*);
    void handleKeyEvent(QKeyEvent*);
    void handleWheelEvent(QWheelEvent*);
    void handleTouchEvent(QTouchEvent*);
    void handleHoverEvent(QHoverEvent*);
    void handleFocusEvent(QFocusEvent*);
    void handleInputMethodEvent(QInputMethodEvent*);

#if defined(OS_MACOSX)
    virtual void SetActive(bool active) Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }
    virtual bool IsSpeaking() const Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED; return false; }
    virtual void SpeakSelection() Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }
    virtual void StopSpeaking() Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }
    virtual bool SupportsSpeech() const Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED; return false; }
    virtual void ShowDefinitionForSelection() Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }
    virtual ui::AcceleratedWidgetMac *GetAcceleratedWidgetMac() const Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED; return nullptr; }
#endif // defined(OS_MACOSX)


    // Overridden from content::BrowserAccessibilityDelegate
    virtual content::BrowserAccessibilityManager* CreateBrowserAccessibilityManager(content::BrowserAccessibilityDelegate* delegate, bool for_root_frame) Q_DECL_OVERRIDE;
#ifndef QT_NO_ACCESSIBILITY
    virtual void accessibilityActiveChanged(bool active) Q_DECL_OVERRIDE;
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

    bool IsPopup() const;

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

    ui::TextInputType m_currentInputType;
    bool m_imeInProgress;
    bool m_receivedEmptyImeText;
    QRect m_cursorRect;
    size_t m_anchorPositionWithinSelection;
    size_t m_cursorPositionWithinSelection;
    QPoint m_lockedMousePosition;

    bool m_initPending;

    gfx::Vector2dF m_lastScrollOffset;
    gfx::SizeF m_lastContentsSize;
};

} // namespace QtWebEngineCore

#endif // RENDER_WIDGET_HOST_VIEW_QT_H
