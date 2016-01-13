/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "render_widget_host_view_qt.h"

#include "browser_accessibility_manager_qt.h"
#include "browser_accessibility_qt.h"
#include "chromium_overrides.h"
#include "delegated_frame_node.h"
#include "render_widget_host_view_qt_delegate.h"
#include "type_conversion.h"
#include "web_contents_adapter.h"
#include "web_contents_adapter_client.h"
#include "web_event_factory.h"

#include "base/command_line.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/gesture_detection/gesture_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/size_conversions.h"

#include <QEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QTextFormat>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QStyleHints>
#include <QVariant>
#include <QWheelEvent>
#include <QWindow>
#include <QtGui/qaccessible.h>

static inline Qt::InputMethodHints toQtInputMethodHints(ui::TextInputType inputType)
{
    switch (inputType) {
    case ui::TEXT_INPUT_TYPE_TEXT:
        return Qt::ImhPreferLowercase;
    case ui::TEXT_INPUT_TYPE_SEARCH:
        return Qt::ImhPreferLowercase | Qt::ImhNoAutoUppercase;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
        return Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase;
    case ui::TEXT_INPUT_TYPE_EMAIL:
        return Qt::ImhEmailCharactersOnly;
    case ui::TEXT_INPUT_TYPE_NUMBER:
        return Qt::ImhFormattedNumbersOnly;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
        return Qt::ImhDialableCharactersOnly;
    case ui::TEXT_INPUT_TYPE_URL:
        return Qt::ImhUrlCharactersOnly | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase;
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
        return Qt::ImhDate | Qt::ImhTime;
    case ui::TEXT_INPUT_TYPE_DATE:
    case ui::TEXT_INPUT_TYPE_MONTH:
    case ui::TEXT_INPUT_TYPE_WEEK:
        return Qt::ImhDate;
    case ui::TEXT_INPUT_TYPE_TIME:
        return Qt::ImhTime;
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
        return Qt::ImhMultiLine | Qt::ImhPreferLowercase;
    default:
        return Qt::ImhNone;
    }
}

static inline int firstAvailableId(const QMap<int, int> &map)
{
    ui::BitSet32 usedIds;
    QMap<int, int>::const_iterator end = map.end();
    for (QMap<int, int>::const_iterator it = map.begin(); it != end; ++it)
        usedIds.mark_bit(it.value());
    return usedIds.first_unmarked_bit();
}

static inline ui::GestureProvider::Config QtGestureProviderConfig() {
    ui::GestureProvider::Config config = ui::DefaultGestureProviderConfig();
    // Causes an assert in CreateWebGestureEventFromGestureEventData and we don't need them in Qt.
    config.gesture_begin_end_types_enabled = false;
    // Swipe gestures aren't forwarded, we don't use them and they abort the gesture detection.
    config.scale_gesture_detector_config.gesture_detector_config.swipe_enabled = config.gesture_detector_config.swipe_enabled = false;
    return config;
}

static inline bool compareTouchPoints(const QTouchEvent::TouchPoint &lhs, const QTouchEvent::TouchPoint &rhs)
{
    // TouchPointPressed < TouchPointMoved < TouchPointReleased
    return lhs.state() < rhs.state();
}

class MotionEventQt : public ui::MotionEvent {
public:
    MotionEventQt(const QList<QTouchEvent::TouchPoint> &touchPoints, const base::TimeTicks &eventTime, Action action, int index = -1)
        : touchPoints(touchPoints)
        , eventTime(eventTime)
        , action(action)
        , index(index)
    {
        // ACTION_DOWN and ACTION_UP must be accesssed through pointer_index 0
        Q_ASSERT((action != ACTION_DOWN && action != ACTION_UP) || index == 0);
    }

    virtual int GetId() const Q_DECL_OVERRIDE { return 0; }
    virtual Action GetAction() const Q_DECL_OVERRIDE { return action; }
    virtual int GetActionIndex() const Q_DECL_OVERRIDE { return index; }
    virtual size_t GetPointerCount() const Q_DECL_OVERRIDE { return touchPoints.size(); }
    virtual int GetPointerId(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).id(); }
    virtual float GetX(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).pos().x(); }
    virtual float GetY(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).pos().y(); }
    virtual float GetRawX(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).screenPos().x(); }
    virtual float GetRawY(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).screenPos().y(); }
    virtual float GetTouchMajor(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).rect().height(); }
    virtual float GetPressure(size_t pointer_index) const Q_DECL_OVERRIDE { return touchPoints.at(pointer_index).pressure(); }
    virtual base::TimeTicks GetEventTime() const Q_DECL_OVERRIDE { return eventTime; }

    virtual size_t GetHistorySize() const Q_DECL_OVERRIDE { return 0; }
    virtual base::TimeTicks GetHistoricalEventTime(size_t historical_index) const Q_DECL_OVERRIDE { return base::TimeTicks(); }
    virtual float GetHistoricalTouchMajor(size_t pointer_index, size_t historical_index) const Q_DECL_OVERRIDE { return 0; }
    virtual float GetHistoricalX(size_t pointer_index, size_t historical_index) const Q_DECL_OVERRIDE { return 0; }
    virtual float GetHistoricalY(size_t pointer_index, size_t historical_index) const Q_DECL_OVERRIDE { return 0; }
    virtual ToolType GetToolType(size_t pointer_index) const Q_DECL_OVERRIDE { return ui::MotionEvent::TOOL_TYPE_UNKNOWN; }
    virtual int GetButtonState() const Q_DECL_OVERRIDE { return 0; }

    virtual scoped_ptr<MotionEvent> Cancel() const Q_DECL_OVERRIDE { Q_UNREACHABLE(); return scoped_ptr<MotionEvent>(); }

    virtual scoped_ptr<MotionEvent> Clone() const Q_DECL_OVERRIDE {
        return scoped_ptr<MotionEvent>(new MotionEventQt(*this));
    }

private:
    QList<QTouchEvent::TouchPoint> touchPoints;
    base::TimeTicks eventTime;
    Action action;
    int index;
};

RenderWidgetHostViewQt::RenderWidgetHostViewQt(content::RenderWidgetHost* widget)
    : m_host(content::RenderWidgetHostImpl::From(widget))
    , m_gestureProvider(QtGestureProviderConfig(), this)
    , m_sendMotionActionDown(false)
    , m_frameNodeData(new DelegatedFrameNodeData)
    , m_needsDelegatedFrameAck(false)
    , m_didFirstVisuallyNonEmptyLayout(false)
    , m_adapterClient(0)
    , m_anchorPositionWithinSelection(0)
    , m_cursorPositionWithinSelection(0)
    , m_initPending(false)
{
    m_host->SetView(this);

    QAccessible::installActivationObserver(this);
    if (QAccessible::isActive())
        content::BrowserAccessibilityStateImpl::GetInstance()->EnableAccessibility();
}

RenderWidgetHostViewQt::~RenderWidgetHostViewQt()
{
    QAccessible::removeActivationObserver(this);
}

void RenderWidgetHostViewQt::setDelegate(RenderWidgetHostViewQtDelegate* delegate)
{
    m_delegate.reset(delegate);
}

void RenderWidgetHostViewQt::setAdapterClient(WebContentsAdapterClient *adapterClient)
{
    Q_ASSERT(!m_adapterClient);

    m_adapterClient = adapterClient;
    if (m_initPending)
        InitAsChild(0);
}

void RenderWidgetHostViewQt::InitAsChild(gfx::NativeView)
{
    if (!m_adapterClient) {
        m_initPending = true;
        return;
    }
    m_initPending = false;
    m_delegate->initAsChild(m_adapterClient);
}

void RenderWidgetHostViewQt::InitAsPopup(content::RenderWidgetHostView*, const gfx::Rect& rect)
{
    m_delegate->initAsPopup(toQt(rect));
}

void RenderWidgetHostViewQt::InitAsFullscreen(content::RenderWidgetHostView*)
{
}

content::RenderWidgetHost* RenderWidgetHostViewQt::GetRenderWidgetHost() const
{
    return m_host;
}

void RenderWidgetHostViewQt::SetSize(const gfx::Size& size)
{
    int width = size.width();
    int height = size.height();

    m_delegate->resize(width,height);
}

void RenderWidgetHostViewQt::SetBounds(const gfx::Rect& screenRect)
{
    // This is called when webkit has sent us a Move message.
    if (IsPopup())
        m_delegate->move(toQt(screenRect.origin()));
    SetSize(screenRect.size());
}

gfx::Size RenderWidgetHostViewQt::GetPhysicalBackingSize() const
{
    if (!m_delegate || !m_delegate->window() || !m_delegate->window()->screen())
        return gfx::Size();

    const QScreen* screen = m_delegate->window()->screen();
    gfx::SizeF size = toGfx(m_delegate->screenRect().size());
    return gfx::ToCeiledSize(gfx::ScaleSize(size, screen->devicePixelRatio()));
}

gfx::NativeView RenderWidgetHostViewQt::GetNativeView() const
{
    // gfx::NativeView is a typedef to a platform specific view
    // pointer (HWND, NSView*, GtkWidget*) and other ports use
    // this function in the renderer_host layer when setting up
    // the view hierarchy and for generating snapshots in tests.
    // Since we manage the view hierarchy in Qt its value hasn't
    // been meaningful.
    return gfx::NativeView();
}

gfx::NativeViewId RenderWidgetHostViewQt::GetNativeViewId() const
{
    return 0;
}

gfx::NativeViewAccessible RenderWidgetHostViewQt::GetNativeViewAccessible()
{
    return 0;
}

void RenderWidgetHostViewQt::CreateBrowserAccessibilityManagerIfNeeded()
{
    if (GetBrowserAccessibilityManager())
        return;

    SetBrowserAccessibilityManager(new content::BrowserAccessibilityManagerQt(
        m_adapterClient->accessibilityParentObject(),
        content::BrowserAccessibilityManagerQt::GetEmptyDocument(),
        this));
}

// Set focus to the associated View component.
void RenderWidgetHostViewQt::Focus()
{
    m_host->SetInputMethodActive(true);
    if (!IsPopup())
        m_delegate->setKeyboardFocus();
    m_host->Focus();
}

bool RenderWidgetHostViewQt::HasFocus() const
{
    return m_delegate->hasKeyboardFocus();
}

bool RenderWidgetHostViewQt::IsSurfaceAvailableForCopy() const
{
    return true;
}

void RenderWidgetHostViewQt::Show()
{
    m_delegate->show();
}

void RenderWidgetHostViewQt::Hide()
{
    m_delegate->hide();
}

bool RenderWidgetHostViewQt::IsShowing()
{
    return m_delegate->isVisible();
}

// Retrieve the bounds of the View, in screen coordinates.
gfx::Rect RenderWidgetHostViewQt::GetViewBounds() const
{
    QRectF p = m_delegate->contentsRect();
    float s = dpiScale();
    gfx::Point p1(floor(p.x() / s), floor(p.y() / s));
    gfx::Point p2(ceil(p.right() /s), ceil(p.bottom() / s));
    return gfx::BoundingRect(p1, p2);
}

// Return value indicates whether the mouse is locked successfully or not.
bool RenderWidgetHostViewQt::LockMouse()
{
    QT_NOT_USED
    return false;
}
void RenderWidgetHostViewQt::UnlockMouse()
{
    QT_NOT_USED
}

void RenderWidgetHostViewQt::WasShown()
{
    m_host->WasShown();
}

void RenderWidgetHostViewQt::WasHidden()
{
    m_host->WasHidden();
}

void RenderWidgetHostViewQt::MovePluginWindows(const std::vector<content::WebPluginGeometry>&)
{
    // QT_NOT_YET_IMPLEMENTED
}

void RenderWidgetHostViewQt::Blur()
{
    m_host->SetInputMethodActive(false);
    m_host->Blur();
}

void RenderWidgetHostViewQt::UpdateCursor(const content::WebCursor &webCursor)
{
    content::WebCursor::CursorInfo cursorInfo;
    webCursor.GetCursorInfo(&cursorInfo);
    Qt::CursorShape shape;
    switch (cursorInfo.type) {
    case blink::WebCursorInfo::TypePointer:
        shape = Qt::ArrowCursor;
        break;
    case blink::WebCursorInfo::TypeCross:
        shape = Qt::CrossCursor;
        break;
    case blink::WebCursorInfo::TypeHand:
        shape = Qt::PointingHandCursor;
        break;
    case blink::WebCursorInfo::TypeIBeam:
        shape = Qt::IBeamCursor;
        break;
    case blink::WebCursorInfo::TypeWait:
        shape = Qt::WaitCursor;
        break;
    case blink::WebCursorInfo::TypeHelp:
        shape = Qt::WhatsThisCursor;
        break;
    case blink::WebCursorInfo::TypeEastResize:
    case blink::WebCursorInfo::TypeWestResize:
    case blink::WebCursorInfo::TypeEastWestResize:
    case blink::WebCursorInfo::TypeEastPanning:
    case blink::WebCursorInfo::TypeWestPanning:
        shape = Qt::SizeHorCursor;
        break;
    case blink::WebCursorInfo::TypeNorthResize:
    case blink::WebCursorInfo::TypeSouthResize:
    case blink::WebCursorInfo::TypeNorthSouthResize:
    case blink::WebCursorInfo::TypeNorthPanning:
    case blink::WebCursorInfo::TypeSouthPanning:
        shape = Qt::SizeVerCursor;
        break;
    case blink::WebCursorInfo::TypeNorthEastResize:
    case blink::WebCursorInfo::TypeSouthWestResize:
    case blink::WebCursorInfo::TypeNorthEastSouthWestResize:
    case blink::WebCursorInfo::TypeNorthEastPanning:
    case blink::WebCursorInfo::TypeSouthWestPanning:
        shape = Qt::SizeBDiagCursor;
        break;
    case blink::WebCursorInfo::TypeNorthWestResize:
    case blink::WebCursorInfo::TypeSouthEastResize:
    case blink::WebCursorInfo::TypeNorthWestSouthEastResize:
    case blink::WebCursorInfo::TypeNorthWestPanning:
    case blink::WebCursorInfo::TypeSouthEastPanning:
        shape = Qt::SizeFDiagCursor;
        break;
    case blink::WebCursorInfo::TypeColumnResize:
        shape = Qt::SplitHCursor;
        break;
    case blink::WebCursorInfo::TypeRowResize:
        shape = Qt::SplitVCursor;
        break;
    case blink::WebCursorInfo::TypeMiddlePanning:
    case blink::WebCursorInfo::TypeMove:
        shape = Qt::SizeAllCursor;
        break;
    case blink::WebCursorInfo::TypeVerticalText:
    case blink::WebCursorInfo::TypeCell:
    case blink::WebCursorInfo::TypeContextMenu:
    case blink::WebCursorInfo::TypeAlias:
    case blink::WebCursorInfo::TypeProgress:
    case blink::WebCursorInfo::TypeCopy:
    case blink::WebCursorInfo::TypeZoomIn:
    case blink::WebCursorInfo::TypeZoomOut:
        // FIXME: Load from the resource bundle.
        shape = Qt::ArrowCursor;
        break;
    case blink::WebCursorInfo::TypeNoDrop:
    case blink::WebCursorInfo::TypeNotAllowed:
        shape = Qt::ForbiddenCursor;
        break;
    case blink::WebCursorInfo::TypeNone:
        shape = Qt::BlankCursor;
        break;
    case blink::WebCursorInfo::TypeGrab:
        shape = Qt::OpenHandCursor;
        break;
    case blink::WebCursorInfo::TypeGrabbing:
        shape = Qt::ClosedHandCursor;
        break;
    case blink::WebCursorInfo::TypeCustom:
        // FIXME: Extract from the CursorInfo.
        shape = Qt::ArrowCursor;
        break;
    default:
        Q_UNREACHABLE();
        shape = Qt::ArrowCursor;
    }
    m_delegate->updateCursor(QCursor(shape));
}

void RenderWidgetHostViewQt::SetIsLoading(bool)
{
    // We use WebContentsDelegateQt::LoadingStateChanged to notify about loading state.
}

void RenderWidgetHostViewQt::TextInputStateChanged(const ViewHostMsg_TextInputState_Params& params)
{
    m_currentInputType = params.type;
    m_delegate->inputMethodStateChanged(static_cast<bool>(params.type));
}

void RenderWidgetHostViewQt::ImeCancelComposition()
{
    qApp->inputMethod()->reset();
}

void RenderWidgetHostViewQt::ImeCompositionRangeChanged(const gfx::Range&, const std::vector<gfx::Rect>&)
{
    // FIXME: not implemented?
    QT_NOT_YET_IMPLEMENTED
}

void RenderWidgetHostViewQt::RenderProcessGone(base::TerminationStatus, int)
{
    Destroy();
}

void RenderWidgetHostViewQt::Destroy()
{
    delete this;
}

void RenderWidgetHostViewQt::SetTooltipText(const base::string16 &tooltip_text)
{
    m_delegate->setTooltip(toQt(tooltip_text));
}

void RenderWidgetHostViewQt::SelectionBoundsChanged(const ViewHostMsg_SelectionBounds_Params &params)
{
    if (selection_range_.IsValid()) {
        if (params.is_anchor_first) {
            m_anchorPositionWithinSelection = selection_range_.GetMin() - selection_text_offset_;
            m_cursorPositionWithinSelection = selection_range_.GetMax() - selection_text_offset_;
        } else {
            m_anchorPositionWithinSelection = selection_range_.GetMax() - selection_text_offset_;
            m_cursorPositionWithinSelection = selection_range_.GetMin() - selection_text_offset_;
        }
    }

    gfx::Rect caretRect = gfx::UnionRects(params.anchor_rect, params.focus_rect);
    m_cursorRect = QRect(caretRect.x(), caretRect.y(), caretRect.width(), caretRect.height());
}

void RenderWidgetHostViewQt::ScrollOffsetChanged()
{
    // Not used.
}

void RenderWidgetHostViewQt::CopyFromCompositingSurface(const gfx::Rect& src_subrect, const gfx::Size& /* dst_size */, const base::Callback<void(bool, const SkBitmap&)>& callback, const SkBitmap::Config config)
{
    NOTIMPLEMENTED();
    callback.Run(false, SkBitmap());
}

void RenderWidgetHostViewQt::CopyFromCompositingSurfaceToVideoFrame(const gfx::Rect& src_subrect, const scoped_refptr<media::VideoFrame>& target, const base::Callback<void(bool)>& callback)
{
    NOTIMPLEMENTED();
    callback.Run(false);
}

bool RenderWidgetHostViewQt::CanCopyToVideoFrame() const
{
    return false;
}

void RenderWidgetHostViewQt::AcceleratedSurfaceInitialized(int host_id, int route_id)
{
}

void RenderWidgetHostViewQt::AcceleratedSurfaceBuffersSwapped(const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params, int gpu_host_id)
{
    AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
    ack_params.sync_point = 0;
    content::RenderWidgetHostImpl::AcknowledgeBufferPresent(params.route_id, gpu_host_id, ack_params);
    content::RenderWidgetHostImpl::CompositorFrameDrawn(params.latency_info);
}

void RenderWidgetHostViewQt::AcceleratedSurfacePostSubBuffer(const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params, int gpu_host_id)
{
    AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
    ack_params.sync_point = 0;
    content::RenderWidgetHostImpl::AcknowledgeBufferPresent(params.route_id, gpu_host_id, ack_params);
    content::RenderWidgetHostImpl::CompositorFrameDrawn(params.latency_info);
}

void RenderWidgetHostViewQt::AcceleratedSurfaceSuspend()
{
    QT_NOT_YET_IMPLEMENTED
}

void RenderWidgetHostViewQt::AcceleratedSurfaceRelease()
{
    QT_NOT_YET_IMPLEMENTED
}

bool RenderWidgetHostViewQt::HasAcceleratedSurface(const gfx::Size&)
{
    return false;
}

void RenderWidgetHostViewQt::OnSwapCompositorFrame(uint32 output_surface_id, scoped_ptr<cc::CompositorFrame> frame)
{
    Q_ASSERT(!m_needsDelegatedFrameAck);
    m_needsDelegatedFrameAck = true;
    m_pendingOutputSurfaceId = output_surface_id;
    Q_ASSERT(frame->delegated_frame_data);
    Q_ASSERT(!m_frameNodeData->frameData || m_frameNodeData->frameData->resource_list.empty());
    m_frameNodeData->frameData = frame->delegated_frame_data.Pass();
    m_frameNodeData->frameDevicePixelRatio = frame->metadata.device_scale_factor;

    // Support experimental.viewport.devicePixelRatio, see GetScreenInfo implementation below.
    float dpiScale = this->dpiScale();
    if (dpiScale != 0 && dpiScale != 1)
        m_frameNodeData->frameDevicePixelRatio /= dpiScale;

    m_delegate->update();

    if (m_didFirstVisuallyNonEmptyLayout) {
        m_adapterClient->loadVisuallyCommitted();
        m_didFirstVisuallyNonEmptyLayout = false;
    }
}

void RenderWidgetHostViewQt::GetScreenInfo(blink::WebScreenInfo* results)
{
    QWindow* window = m_delegate->window();
    if (!window)
        return;
    GetScreenInfoFromNativeWindow(window, results);

    // Support experimental.viewport.devicePixelRatio
    results->deviceScaleFactor *= dpiScale();
}

gfx::Rect RenderWidgetHostViewQt::GetBoundsInRootWindow()
{
    if (!m_delegate->window())
        return gfx::Rect();

    QRect r = m_delegate->window()->frameGeometry();
    return gfx::Rect(r.x(), r.y(), r.width(), r.height());
}

gfx::GLSurfaceHandle RenderWidgetHostViewQt::GetCompositingSurface()
{
    return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
}

void RenderWidgetHostViewQt::SelectionChanged(const base::string16 &text, size_t offset, const gfx::Range &range)
{
    content::RenderWidgetHostViewBase::SelectionChanged(text, offset, range);
    m_adapterClient->selectionChanged();

#if defined(USE_X11)
    // Set the CLIPBOARD_TYPE_SELECTION to the ui::Clipboard.
    ui::ScopedClipboardWriter clipboard_writer(
                ui::Clipboard::GetForCurrentThread(),
                ui::CLIPBOARD_TYPE_SELECTION);
    clipboard_writer.WriteText(text);
#endif
}

void RenderWidgetHostViewQt::OnGestureEvent(const ui::GestureEventData& gesture)
{
    m_host->ForwardGestureEvent(content::CreateWebGestureEventFromGestureEventData(gesture));
}

QSGNode *RenderWidgetHostViewQt::updatePaintNode(QSGNode *oldNode)
{
    DelegatedFrameNode *frameNode = static_cast<DelegatedFrameNode *>(oldNode);
    if (!frameNode)
        frameNode = new DelegatedFrameNode;

    frameNode->commit(m_frameNodeData.data(), &m_resourcesToRelease);

    // This is possibly called from the Qt render thread, post the ack back to the UI
    // to tell the child compositors to release resources and trigger a new frame.
    if (m_needsDelegatedFrameAck) {
        m_needsDelegatedFrameAck = false;
        content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
            base::Bind(&RenderWidgetHostViewQt::sendDelegatedFrameAck, AsWeakPtr()));
    }

    return frameNode;
}

void RenderWidgetHostViewQt::notifyResize()
{
    m_host->WasResized();
}

void RenderWidgetHostViewQt::windowBoundsChanged()
{
    m_host->SendScreenRects();
    if (m_delegate->window())
        m_host->NotifyScreenInfoChanged();
}

void RenderWidgetHostViewQt::windowChanged()
{
    if (m_delegate->window())
        m_host->NotifyScreenInfoChanged();
}

bool RenderWidgetHostViewQt::forwardEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        Focus(); // Fall through.
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        handleMouseEvent(static_cast<QMouseEvent*>(event));
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        handleKeyEvent(static_cast<QKeyEvent*>(event));
        break;
    case QEvent::Wheel:
        handleWheelEvent(static_cast<QWheelEvent*>(event));
        break;
    case QEvent::TouchBegin:
        Focus(); // Fall through.
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        handleTouchEvent(static_cast<QTouchEvent*>(event));
        break;
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:
        handleHoverEvent(static_cast<QHoverEvent*>(event));
        break;
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        handleFocusEvent(static_cast<QFocusEvent*>(event));
        break;
    case QEvent::InputMethod:
        handleInputMethodEvent(static_cast<QInputMethodEvent*>(event));
        break;
    default:
        return false;
    }
    return true;
}

QVariant RenderWidgetHostViewQt::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return QVariant(m_currentInputType != ui::TEXT_INPUT_TYPE_NONE);
    case Qt::ImCursorRectangle:
        return m_cursorRect;
    case Qt::ImFont:
        return QVariant();
    case Qt::ImCursorPosition:
        return static_cast<uint>(m_cursorPositionWithinSelection);
    case Qt::ImAnchorPosition:
        return static_cast<uint>(m_anchorPositionWithinSelection);
    case Qt::ImSurroundingText:
        return toQt(selection_text_);
    case Qt::ImCurrentSelection:
        return toQt(GetSelectedText());
    case Qt::ImMaximumTextLength:
        return QVariant(); // No limit.
    case Qt::ImHints:
        return int(toQtInputMethodHints(m_currentInputType));
    default:
        return QVariant();
    }
}

void RenderWidgetHostViewQt::ProcessAckedTouchEvent(const content::TouchEventWithLatencyInfo &touch, content::InputEventAckState ack_result) {
    Q_UNUSED(touch);
    const bool eventConsumed = ack_result == content::INPUT_EVENT_ACK_STATE_CONSUMED;
    m_gestureProvider.OnTouchEventAck(eventConsumed);
}

void RenderWidgetHostViewQt::sendDelegatedFrameAck()
{
    cc::CompositorFrameAck ack;
    m_resourcesToRelease.swap(ack.resources);
    content::RenderWidgetHostImpl::SendSwapCompositorFrameAck(
        m_host->GetRoutingID(), m_pendingOutputSurfaceId,
        m_host->GetProcess()->GetID(), ack);
}

void RenderWidgetHostViewQt::processMotionEvent(const ui::MotionEvent &motionEvent)
{
    if (!m_gestureProvider.OnTouchEvent(motionEvent))
        return;

    // Short-circuit touch forwarding if no touch handlers exist.
    if (!m_host->ShouldForwardTouchEvent()) {
        const bool eventConsumed = false;
        m_gestureProvider.OnTouchEventAck(eventConsumed);
        return;
    }

    m_host->ForwardTouchEvent(content::CreateWebTouchEventFromMotionEvent(motionEvent));
}

QList<QTouchEvent::TouchPoint> RenderWidgetHostViewQt::mapTouchPointIds(const QList<QTouchEvent::TouchPoint> &inputPoints)
{
    QList<QTouchEvent::TouchPoint> outputPoints = inputPoints;
    for (int i = 0; i < outputPoints.size(); ++i) {
        QTouchEvent::TouchPoint &point = outputPoints[i];

        int qtId = point.id();
        QMap<int, int>::const_iterator it = m_touchIdMapping.find(qtId);
        if (it == m_touchIdMapping.end())
            it = m_touchIdMapping.insert(qtId, firstAvailableId(m_touchIdMapping));
        point.setId(it.value());

        if (point.state() == Qt::TouchPointReleased)
            m_touchIdMapping.remove(qtId);
    }

    return outputPoints;
}

float RenderWidgetHostViewQt::dpiScale() const
{
    return m_adapterClient ? m_adapterClient->dpiScale() : 1.0;
}

bool RenderWidgetHostViewQt::IsPopup() const
{
    return popup_type_ != blink::WebPopupTypeNone;
}

void RenderWidgetHostViewQt::handleMouseEvent(QMouseEvent* event)
{
    blink::WebMouseEvent webEvent = WebEventFactory::toWebMouseEvent(event, dpiScale());
    if (event->type() == QMouseEvent::MouseButtonPress) {
        if (event->button() != m_clickHelper.lastPressButton
            || (event->timestamp() - m_clickHelper.lastPressTimestamp > static_cast<ulong>(qGuiApp->styleHints()->mouseDoubleClickInterval()))
            || (event->pos() - m_clickHelper.lastPressPosition).manhattanLength() > qGuiApp->styleHints()->startDragDistance())
            m_clickHelper.clickCounter = 0;

        m_clickHelper.lastPressTimestamp = event->timestamp();
        webEvent.clickCount = ++m_clickHelper.clickCounter;
        m_clickHelper.lastPressButton = event->button();
        m_clickHelper.lastPressPosition = QPointF(event->pos()).toPoint();
    }

    m_host->ForwardMouseEvent(webEvent);
}

void RenderWidgetHostViewQt::handleKeyEvent(QKeyEvent *ev)
{
    content::NativeWebKeyboardEvent webEvent = WebEventFactory::toWebKeyboardEvent(ev);
    m_host->ForwardKeyboardEvent(webEvent);
    if (webEvent.type == blink::WebInputEvent::RawKeyDown && !ev->text().isEmpty()) {
        webEvent.type = blink::WebInputEvent::Char;
        m_host->ForwardKeyboardEvent(webEvent);
    }
}

void RenderWidgetHostViewQt::handleInputMethodEvent(QInputMethodEvent *ev)
{
    if (!m_host)
        return;

    QString commitString = ev->commitString();
    QString preeditString = ev->preeditString();

    int replacementStart = ev->replacementStart();
    int replacementLength = ev->replacementLength();

    int cursorPositionInPreeditString = -1;
    gfx::Range selectionRange = gfx::Range::InvalidRange();

    const QList<QInputMethodEvent::Attribute> &attributes = ev->attributes();
    std::vector<blink::WebCompositionUnderline> underlines;

    Q_FOREACH (const QInputMethodEvent::Attribute &attribute, attributes) {
        switch (attribute.type) {
        case QInputMethodEvent::TextFormat: {
            if (preeditString.isEmpty())
                break;

            QTextCharFormat textCharFormat = attribute.value.value<QTextFormat>().toCharFormat();
            QColor qcolor = textCharFormat.underlineColor();
            blink::WebColor color = SkColorSetARGB(qcolor.alpha(), qcolor.red(), qcolor.green(), qcolor.blue());
            int start = qMin(attribute.start, (attribute.start + attribute.length));
            int end = qMax(attribute.start, (attribute.start + attribute.length));
            underlines.push_back(blink::WebCompositionUnderline(start, end, color, false));
            break;
        }
        case QInputMethodEvent::Cursor:
            if (attribute.length)
                cursorPositionInPreeditString = attribute.start;
            break;
        case QInputMethodEvent::Selection:
            selectionRange.set_start(qMin(attribute.start, (attribute.start + attribute.length)));
            selectionRange.set_end(qMax(attribute.start, (attribute.start + attribute.length)));
            break;
        default:
            break;
        }
    }

    if (preeditString.isEmpty()) {
        gfx::Range replacementRange = (replacementLength > 0) ? gfx::Range(replacementStart, replacementStart + replacementLength)
                                                              : gfx::Range::InvalidRange();
        m_host->ImeConfirmComposition(toString16(commitString), replacementRange, false);
    } else {
        if (!selectionRange.IsValid()) {
            // We did not receive a valid selection range, hence the range is going to mark the cursor position.
            int newCursorPosition = (cursorPositionInPreeditString < 0) ? preeditString.length() : cursorPositionInPreeditString;
            selectionRange.set_start(newCursorPosition);
            selectionRange.set_end(newCursorPosition);
        }
        m_host->ImeSetComposition(toString16(preeditString), underlines, selectionRange.start(), selectionRange.end());
    }
}

void RenderWidgetHostViewQt::AccessibilitySetFocus(int acc_obj_id)
{
    if (!m_host)
        return;
    m_host->AccessibilitySetFocus(acc_obj_id);
}

void RenderWidgetHostViewQt::AccessibilityDoDefaultAction(int acc_obj_id)
{
    if (!m_host)
      return;
    m_host->AccessibilityDoDefaultAction(acc_obj_id);
}

void RenderWidgetHostViewQt::AccessibilityScrollToMakeVisible(int acc_obj_id, gfx::Rect subfocus)
{
    if (!m_host)
        return;
    m_host->AccessibilityScrollToMakeVisible(acc_obj_id, subfocus);
}

void RenderWidgetHostViewQt::AccessibilityScrollToPoint(int acc_obj_id, gfx::Point point)
{
    if (!m_host)
      return;
    m_host->AccessibilityScrollToPoint(acc_obj_id, point);
}

void RenderWidgetHostViewQt::AccessibilitySetTextSelection(int acc_obj_id, int start_offset, int end_offset)
{
    if (!m_host)
        return;
    m_host->AccessibilitySetTextSelection(acc_obj_id, start_offset, end_offset);
}

bool RenderWidgetHostViewQt::AccessibilityViewHasFocus() const
{
    return HasFocus();
}

void RenderWidgetHostViewQt::AccessibilityFatalError()
{
    if (!m_host)
        return;
    m_host->AccessibilityFatalError();
    SetBrowserAccessibilityManager(NULL);
}

void RenderWidgetHostViewQt::accessibilityActiveChanged(bool active)
{
    if (active)
        content::BrowserAccessibilityStateImpl::GetInstance()->EnableAccessibility();
    else
        content::BrowserAccessibilityStateImpl::GetInstance()->DisableAccessibility();
}

void RenderWidgetHostViewQt::handleWheelEvent(QWheelEvent *ev)
{
    m_host->ForwardWheelEvent(WebEventFactory::toWebWheelEvent(ev, dpiScale()));
}

void RenderWidgetHostViewQt::handleTouchEvent(QTouchEvent *ev)
{
    // Chromium expects the touch event timestamps to be comparable to base::TimeTicks::Now().
    // Most importantly we also have to preserve the relative time distance between events.
    // Calculate a delta between event timestamps and Now() on the first received event, and
    // apply this delta to all successive events. This delta is most likely smaller than it
    // should by calculating it here but this will hopefully cause less than one frame of delay.
    base::TimeTicks eventTimestamp = base::TimeTicks() + base::TimeDelta::FromMilliseconds(ev->timestamp());
    if (m_eventsToNowDelta == base::TimeDelta())
        m_eventsToNowDelta = base::TimeTicks::Now() - eventTimestamp;
    eventTimestamp += m_eventsToNowDelta;

    QList<QTouchEvent::TouchPoint> touchPoints = mapTouchPointIds(ev->touchPoints());

    if (ev->type() == QEvent::TouchCancel) {
        MotionEventQt cancelEvent(touchPoints, eventTimestamp, ui::MotionEvent::ACTION_CANCEL);
        processMotionEvent(cancelEvent);
        return;
    }

    if (ev->type() == QEvent::TouchBegin)
        m_sendMotionActionDown = true;

    // Make sure that ACTION_POINTER_DOWN is delivered before ACTION_MOVE,
    // and ACTION_MOVE before ACTION_POINTER_UP.
    std::sort(touchPoints.begin(), touchPoints.end(), compareTouchPoints);

    for (int i = 0; i < touchPoints.size(); ++i) {
        ui::MotionEvent::Action action;
        switch (touchPoints[i].state()) {
        case Qt::TouchPointPressed:
            if (m_sendMotionActionDown) {
                action = ui::MotionEvent::ACTION_DOWN;
                m_sendMotionActionDown = false;
            } else
                action = ui::MotionEvent::ACTION_POINTER_DOWN;
            break;
        case Qt::TouchPointMoved:
            action = ui::MotionEvent::ACTION_MOVE;
            break;
        case Qt::TouchPointReleased:
            action = touchPoints.size() > 1 ? ui::MotionEvent::ACTION_POINTER_UP : ui::MotionEvent::ACTION_UP;
            break;
        default:
            // Ignore Qt::TouchPointStationary
            continue;
        }

        MotionEventQt motionEvent(touchPoints, eventTimestamp, action, i);
        processMotionEvent(motionEvent);
    }
}

void RenderWidgetHostViewQt::handleHoverEvent(QHoverEvent *ev)
{
    m_host->ForwardMouseEvent(WebEventFactory::toWebMouseEvent(ev, dpiScale()));
}

void RenderWidgetHostViewQt::handleFocusEvent(QFocusEvent *ev)
{
    if (ev->gotFocus()) {
        m_host->GotFocus();
        m_host->SetActive(true);
        Q_ASSERT(m_host->IsRenderView());
        if (ev->reason() == Qt::TabFocusReason)
            static_cast<content::RenderViewHostImpl*>(m_host)->SetInitialFocus(false);
        else if (ev->reason() == Qt::BacktabFocusReason)
            static_cast<content::RenderViewHostImpl*>(m_host)->SetInitialFocus(true);
        ev->accept();
    } else if (ev->lostFocus()) {
        m_host->SetActive(false);
        m_host->Blur();
        ev->accept();
    }
}

QAccessibleInterface *RenderWidgetHostViewQt::GetQtAccessible()
{
    // Assume we have a screen reader doing stuff
    CreateBrowserAccessibilityManagerIfNeeded();
    content::BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
    content::BrowserAccessibility *acc = GetBrowserAccessibilityManager()->GetRoot();
    content::BrowserAccessibilityQt *accQt = static_cast<content::BrowserAccessibilityQt*>(acc);
    return accQt;
}

void RenderWidgetHostViewQt::didFirstVisuallyNonEmptyLayout()
{
    m_didFirstVisuallyNonEmptyLayout = true;
}
