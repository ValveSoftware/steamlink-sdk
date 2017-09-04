/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "desktopinputselectioncontrol.h"
#include "inputcontext.h"
#include "inputselectionhandle.h"
#include "settings.h"
#include "platforminputcontext.h"

#include <QtCore/qpropertyanimation.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qimagereader.h>

namespace QtVirtualKeyboard {

DesktopInputSelectionControl::DesktopInputSelectionControl(QObject *parent, InputContext *inputContext)
    : QObject(parent),
      m_inputContext(inputContext),
      m_anchorSelectionHandle(),
      m_cursorSelectionHandle(),
      m_handleState(HandleIsReleased),
      m_enabled(false),
      m_anchorHandleVisible(false),
      m_cursorHandleVisible(false),
      m_eventFilterEnabled(true),
      m_handleWindowSize(40, 40*1.12)   // because a finger patch is slightly taller than its width
{
    QWindow *focusWindow = QGuiApplication::focusWindow();
    Q_ASSERT(focusWindow);
    connect(m_inputContext, &InputContext::selectionControlVisibleChanged, this, &DesktopInputSelectionControl::updateVisibility);
}

/*
 * Includes the hit area surrounding the visual handle
 */
QRect DesktopInputSelectionControl::handleRectForCursorRect(const QRectF &cursorRect) const
{
    const int topMargin = (m_handleWindowSize.height() - m_handleImage.size().height())/2;
    const QPoint pos(int(cursorRect.x() + (cursorRect.width() - m_handleWindowSize.width())/2),
                     int(cursorRect.bottom()) - topMargin);
    return QRect(pos, m_handleWindowSize);
}

/*
 * Includes the hit area surrounding the visual handle
 */
QRect DesktopInputSelectionControl::anchorHandleRect() const
{
    return handleRectForCursorRect(m_inputContext->anchorRectangle());
}

/*
 * Includes the hit area surrounding the visual handle
 */
QRect DesktopInputSelectionControl::cursorHandleRect() const
{
    return handleRectForCursorRect(m_inputContext->cursorRectangle());
}

void DesktopInputSelectionControl::updateAnchorHandlePosition()
{
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        const QPoint pos = focusWindow->mapToGlobal(anchorHandleRect().topLeft());
        m_anchorSelectionHandle->setPosition(pos);
    }
}

void DesktopInputSelectionControl::updateCursorHandlePosition()
{
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        const QPoint pos = focusWindow->mapToGlobal(cursorHandleRect().topLeft());
        m_cursorSelectionHandle->setPosition(pos);
    }
}

void DesktopInputSelectionControl::updateVisibility()
{
    if (!m_enabled) {
        // if VKB is hidden, we must hide the selection handles immediately,
        // because it might mean that the application is shutting down.
        m_anchorSelectionHandle->hide();
        m_cursorSelectionHandle->hide();
        m_anchorHandleVisible = false;
        m_cursorHandleVisible = false;
        return;
    }
    const bool wasAnchorVisible = m_anchorHandleVisible;
    const bool wasCursorVisible = m_cursorHandleVisible;
    const bool makeVisible = (m_inputContext->selectionControlVisible() || m_handleState == HandleIsMoving) && m_enabled;

    m_anchorHandleVisible = makeVisible;
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        QRectF globalAnchorRectangle = m_inputContext->anchorRectangle();
        QPoint tl = focusWindow->mapToGlobal(globalAnchorRectangle.toRect().topLeft());
        globalAnchorRectangle.moveTopLeft(tl);
        m_anchorHandleVisible = m_anchorHandleVisible
                && m_inputContext->anchorRectIntersectsClipRect()
                && !(m_inputContext->keyboardRectangle().intersects(globalAnchorRectangle));
    }

    if (wasAnchorVisible != m_anchorHandleVisible) {
        const qreal end = m_anchorHandleVisible ? 1 : 0;
        if (m_anchorHandleVisible)
            m_anchorSelectionHandle->show();
        QPropertyAnimation *anim = new QPropertyAnimation(m_anchorSelectionHandle.data(), "opacity");
        anim->setEndValue(end);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    m_cursorHandleVisible = makeVisible;
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        QRectF globalCursorRectangle = m_inputContext->cursorRectangle();
        QPoint tl = focusWindow->mapToGlobal(globalCursorRectangle.toRect().topLeft());
        globalCursorRectangle.moveTopLeft(tl);
        m_cursorHandleVisible = m_cursorHandleVisible
                && m_inputContext->cursorRectIntersectsClipRect()
                && !(m_inputContext->keyboardRectangle().intersects(globalCursorRectangle));

    }

    if (wasCursorVisible != m_cursorHandleVisible) {
        const qreal end = m_cursorHandleVisible ? 1 : 0;
        if (m_cursorHandleVisible)
            m_cursorSelectionHandle->show();
        QPropertyAnimation *anim = new QPropertyAnimation(m_cursorSelectionHandle.data(), "opacity");
        anim->setEndValue(end);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void DesktopInputSelectionControl::reloadGraphics()
{
    Settings *settings = Settings::instance();
    const QString stylePath = QString::fromLatin1(":/QtQuick/VirtualKeyboard/content/styles/%1/images/selectionhandle-bottom.svg")
                                .arg(settings->styleName());
    QImageReader imageReader(stylePath);
    QSize sz = imageReader.size(); // SVG handler will return default size
    sz.scale(20, 20, Qt::KeepAspectRatioByExpanding);
    imageReader.setScaledSize(sz);
    m_handleImage = imageReader.read();

    m_anchorSelectionHandle->applyImage(m_handleWindowSize); // applies m_handleImage for both selection handles
    m_cursorSelectionHandle->applyImage(m_handleWindowSize);
}

void DesktopInputSelectionControl::createHandles()
{
    if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
        Settings *settings = Settings::instance();
        connect(settings, &Settings::styleChanged, this, &DesktopInputSelectionControl::reloadGraphics);

        m_anchorSelectionHandle.reset(new InputSelectionHandle(this, focusWindow));
        m_cursorSelectionHandle.reset(new InputSelectionHandle(this, focusWindow));

        reloadGraphics();
        if (QCoreApplication *app = QCoreApplication::instance()) {
            connect(app, &QCoreApplication::aboutToQuit,
                    this, &DesktopInputSelectionControl::destroyHandles);
        }
    }
}

void DesktopInputSelectionControl::destroyHandles()
{
    m_anchorSelectionHandle.reset();
    m_cursorSelectionHandle.reset();
}

void DesktopInputSelectionControl::setEnabled(bool enable)
{
    // setEnabled(true) just means that the handles _can_ be made visible
    // This will typically be set when a input field gets focus (and having selection).
    m_enabled = enable;
    QWindow *focusWindow = QGuiApplication::focusWindow();
    if (enable) {
        connect(m_inputContext, &InputContext::anchorRectangleChanged, this, &DesktopInputSelectionControl::updateAnchorHandlePosition);
        connect(m_inputContext, &InputContext::cursorRectangleChanged, this, &DesktopInputSelectionControl::updateCursorHandlePosition);
        connect(m_inputContext, &InputContext::anchorRectIntersectsClipRectChanged, this, &DesktopInputSelectionControl::updateVisibility);
        connect(m_inputContext, &InputContext::cursorRectIntersectsClipRectChanged, this, &DesktopInputSelectionControl::updateVisibility);
        if (focusWindow)
            focusWindow->installEventFilter(this);
    } else {
        if (focusWindow)
            focusWindow->removeEventFilter(this);
        disconnect(m_inputContext, &InputContext::cursorRectIntersectsClipRectChanged, this, &DesktopInputSelectionControl::updateVisibility);
        disconnect(m_inputContext, &InputContext::anchorRectIntersectsClipRectChanged, this, &DesktopInputSelectionControl::updateVisibility);
        disconnect(m_inputContext, &InputContext::anchorRectangleChanged, this, &DesktopInputSelectionControl::updateAnchorHandlePosition);
        disconnect(m_inputContext, &InputContext::cursorRectangleChanged, this, &DesktopInputSelectionControl::updateCursorHandlePosition);
    }
    updateVisibility();
}

QImage *DesktopInputSelectionControl::handleImage()
{
    return &m_handleImage;
}

bool DesktopInputSelectionControl::eventFilter(QObject *object, QEvent *event)
{
    QWindow *focusWindow = QGuiApplication::focusWindow();
    if (!m_cursorSelectionHandle || !m_eventFilterEnabled || object != focusWindow)
        return false;
    const bool windowMoved = event->type() == QEvent::Move;
    const bool windowResized = event->type() == QEvent::Resize;
    if (windowMoved || windowResized) {
        if (m_enabled) {
            if (windowMoved) {
                updateAnchorHandlePosition();
                updateCursorHandlePosition();
            }
            updateVisibility();
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        const QPoint mousePos = me->screenPos().toPoint();

        // calculate distances from mouse pos to each handle,
        // then choose to interact with the nearest handle
        struct SelectionHandleInfo {
            qreal squaredDistance;
            QPoint delta;
            QRect rect;
        };
        SelectionHandleInfo handles[2];
        handles[AnchorHandle].rect = anchorHandleRect();
        handles[CursorHandle].rect = cursorHandleRect();

        for (int i = 0; i <= CursorHandle; ++i) {
            SelectionHandleInfo &h = handles[i];
            QPoint curHandleCenter = focusWindow->mapToGlobal(h.rect.center());  // ### map to desktoppanel
            const QPoint delta = mousePos - curHandleCenter;
            h.delta = delta;
            h.squaredDistance = QPoint::dotProduct(delta, delta);
        }

        // (squared) distances calculated, pick the closest handle
        HandleType closestHandle = (handles[AnchorHandle].squaredDistance < handles[CursorHandle].squaredDistance ? AnchorHandle : CursorHandle);

        // Can not be replaced with me->windowPos(); because the event might be forwarded from the window of the handle
        const QPoint windowPos = focusWindow->mapFromGlobal(mousePos);
        if (m_anchorHandleVisible && handles[closestHandle].rect.contains(windowPos)) {
            m_currentDragHandle = closestHandle;
            m_distanceBetweenMouseAndCursor = handles[closestHandle].delta -  QPoint(0, m_handleWindowSize.height()/2 + 4);
            m_handleState = HandleIsHeld;
            m_handleDragStartedPosition = mousePos;
            const QRect otherRect = handles[1 - closestHandle].rect;
            m_otherSelectionPoint = QPoint(otherRect.x() + otherRect.width()/2, otherRect.top() - 4);

            QMouseEvent *mouseEvent = new QMouseEvent(me->type(), me->localPos(), me->windowPos(), me->screenPos(),
                                                      me->button(), me->buttons(), me->modifiers(), me->source());
            m_eventQueue.append(mouseEvent);
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QPoint mousePos = me->screenPos().toPoint();
        if (m_handleState == HandleIsHeld) {
            QPoint delta = m_handleDragStartedPosition - mousePos;
            const int startDragDistance = QGuiApplication::styleHints()->startDragDistance();
            if (QPoint::dotProduct(delta, delta) > startDragDistance * startDragDistance)
                m_handleState = HandleIsMoving;
        }
        if (m_handleState == HandleIsMoving) {
            QPoint cursorPos = mousePos - m_distanceBetweenMouseAndCursor;
            cursorPos = focusWindow->mapFromGlobal(cursorPos);
            if (m_currentDragHandle == CursorHandle)
                m_inputContext->setSelectionOnFocusObject(m_otherSelectionPoint, cursorPos);
            else
                m_inputContext->setSelectionOnFocusObject(cursorPos, m_otherSelectionPoint);
            qDeleteAll(m_eventQueue);
            m_eventQueue.clear();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (m_handleState == HandleIsMoving) {
            m_handleState = HandleIsReleased;
            qDeleteAll(m_eventQueue);
            m_eventQueue.clear();
            return true;
        } else {
            if (QWindow *focusWindow = QGuiApplication::focusWindow()) {
                // playback event queue. These are events that were not designated
                // for the handles in hindsight.
                // This is typically MousePress and MouseRelease (not interleaved with MouseMove)
                // that should instead go through to the underlying input editor
                m_eventFilterEnabled = false;
                while (!m_eventQueue.isEmpty()) {
                    QMouseEvent *e = m_eventQueue.takeFirst();
                    QCoreApplication::sendEvent(focusWindow, e);
                    delete e;
                }
                m_eventFilterEnabled = true;
            }
            m_handleState = HandleIsReleased;
        }
    }
    return false;
}
} //    namespace QtVirtualKeyboard
