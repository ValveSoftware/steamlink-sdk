/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "TitleBar.h"

#include <QtCore>
#include <QtWidgets>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , m_progress(0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void TitleBar::setHost(const QString &host)
{
    m_host = host;
    update();
}

void TitleBar::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void TitleBar::setProgress(int percent)
{
    m_progress = percent;
    update();
}

QSize TitleBar::sizeHint() const
{
    return minimumSizeHint();
}

QSize TitleBar::minimumSizeHint() const
{
    QFontMetrics fm = fontMetrics();
    return QSize(100, fm.height());
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    QString title = m_host;
    if (!m_title.isEmpty())
        title.append(": ").append(m_title);

    QPalette pal = palette();
    QPainter p(this);
    p.fillRect(event->rect(), pal.color(QPalette::Highlight));

    if (m_progress > 0) {

        QRect box = rect();
        box.setLeft(16);
        box.setWidth(width() - box.left() - 110);

        p.setPen(pal.color(QPalette::HighlightedText));
        p.setOpacity(0.8);
        p.drawText(box, Qt::AlignLeft + Qt::AlignVCenter, title);

        int x = width() - 100 - 5;
        int y = 1;
        int h = height() - 4;

        p.setOpacity(1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(pal.color(QPalette::HighlightedText));
        p.drawRect(x, y, 100, h);
        p.setPen(Qt::NoPen);
        p.setBrush(pal.color(QPalette::HighlightedText));
        p.drawRect(x, y, m_progress, h);
    } else {

        QRect box = rect();
        box.setLeft(16);
        box.setWidth(width() - box.left() - 5);
        p.setPen(pal.color(QPalette::HighlightedText));
        p.drawText(box, Qt::AlignLeft + Qt::AlignVCenter, title);
    }

    p.end();
}
