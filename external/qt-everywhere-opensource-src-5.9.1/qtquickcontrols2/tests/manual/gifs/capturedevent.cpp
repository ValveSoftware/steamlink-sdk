/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "capturedevent.h"

#include <QMetaEnum>
#include <QMouseEvent>

namespace {
    static inline bool isMouseEvent(const QEvent &event)
    {
        return event.type() >= QEvent::MouseButtonPress && event.type() <= QEvent::MouseMove;
    }
}

CapturedEvent::CapturedEvent()
{
}

CapturedEvent::CapturedEvent(const QEvent &event, int delay)
{
    setEvent(event);
    setDelay(delay);
}

void CapturedEvent::setEvent(const QEvent &event)
{
    mType = event.type();

    if (isMouseEvent(event)) {
        const QMouseEvent *mouseEvent = static_cast<const QMouseEvent*>(&event);
        mPos = mouseEvent->pos();
        mMouseButton = mouseEvent->button();
    }
}

QEvent::Type CapturedEvent::type() const
{
    return mType;
}

int CapturedEvent::delay() const
{
    return mDelay;
}

void CapturedEvent::setDelay(int delay)
{
    mDelay = delay;

    mCppCommand.clear();

    // We generate the C++ command here instead of when the event is captured,
    // because events() might trim some events, causing the delay of some events to change.
    // If we did it earlier, the events wouldn't have correct delays.
    if (mType == QEvent::MouseMove) {
        mCppCommand = QString::fromLatin1("QTest::mouseMove(&view, QPoint(%1, %2), %3);")
            .arg(mPos.x())
            .arg(mPos.y())
            .arg(mDelay);

    } else if (mType >= QEvent::MouseButtonPress && mType <= QEvent::MouseButtonDblClick) {
        QString eventTestFunctionName = (mType == QEvent::MouseButtonPress
            ? "mousePress" : (mType == QEvent::MouseButtonRelease
            ? "mouseRelease" : "mouseDClick"));
        QString buttonStr = QMetaEnum::fromType<Qt::MouseButtons>().valueToKey(mMouseButton);
        mCppCommand = QString::fromLatin1("QTest::%1(&view, Qt::%2, Qt::NoModifier, QPoint(%3, %4), %5);")
            .arg(eventTestFunctionName)
            .arg(buttonStr)
            .arg(mPos.x())
            .arg(mPos.y())
            .arg(mDelay);
    }
}

QString CapturedEvent::cppCommand() const
{
    return mCppCommand;
}

