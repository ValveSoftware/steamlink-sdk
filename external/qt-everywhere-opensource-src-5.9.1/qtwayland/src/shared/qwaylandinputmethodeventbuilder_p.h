/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QWAYLANDINPUTMETHODEVENTBUILDER_H
#define QWAYLANDINPUTMETHODEVENTBUILDER_H

#include <QInputMethodEvent>

QT_BEGIN_NAMESPACE

class QWaylandInputMethodEventBuilder
{
public:
    QWaylandInputMethodEventBuilder();
    ~QWaylandInputMethodEventBuilder();

    void reset();

    void setCursorPosition(int32_t index, int32_t anchor);
    void setDeleteSurroundingText(uint32_t beforeLength, uint32_t afterLength);

    void addPreeditStyling(uint32_t index, uint32_t length, uint32_t style);
    void setPreeditCursor(int32_t index);

    QInputMethodEvent buildCommit(const QString &text);
    QInputMethodEvent buildPreedit(const QString &text);

    static int indexFromWayland(const QString &text, int length, int base = 0);
    static int indexToWayland(const QString &text, int length, int base = 0);
private:
    QPair<int, int> replacementForDeleteSurrounding();

    int32_t m_anchor;
    int32_t m_cursor;
    uint32_t m_deleteBefore;
    uint32_t m_deleteAfter;

    int32_t m_preeditCursor;
    QList<QInputMethodEvent::Attribute> m_preeditStyles;
};

struct QWaylandInputMethodContentType {
    uint32_t hint;
    uint32_t purpose;

    static QWaylandInputMethodContentType convert(Qt::InputMethodHints hints);
};


QT_END_NAMESPACE

#endif // QWAYLANDINPUTMETHODEVENTBUILDER_H
