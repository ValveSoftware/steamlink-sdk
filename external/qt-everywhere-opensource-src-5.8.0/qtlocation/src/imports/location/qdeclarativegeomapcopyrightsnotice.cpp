/****************************************************************************
**
** Copyright (C) 2014 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qdeclarativegeomapcopyrightsnotice_p.h"

#include <QtGui/QTextDocument>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QPainter>
#include <QtQuick/private/qquickanchors_p.h>
#include <QtQuick/private/qquickanchors_p_p.h>

QT_BEGIN_NAMESPACE

QDeclarativeGeoMapCopyrightNotice::QDeclarativeGeoMapCopyrightNotice(QQuickItem *parent)
:   QQuickPaintedItem(parent), m_copyrightsHtml(0), m_copyrightsVisible(true)
{
    QQuickAnchors *anchors = property("anchors").value<QQuickAnchors *>();
    if (anchors) {
        anchors->setLeft(QQuickAnchorLine(parent, QQuickAnchors::LeftAnchor));
        anchors->setBottom(QQuickAnchorLine(parent, QQuickAnchors::BottomAnchor));
    }
}

QDeclarativeGeoMapCopyrightNotice::~QDeclarativeGeoMapCopyrightNotice()
{
}

/*!
    \internal
*/
void QDeclarativeGeoMapCopyrightNotice::paint(QPainter *painter)
{
    painter->drawImage(0, 0, m_copyrightsImage);
}

void QDeclarativeGeoMapCopyrightNotice::mousePressEvent(QMouseEvent *event)
{
    if (m_copyrightsHtml) {
        m_activeAnchor = m_copyrightsHtml->documentLayout()->anchorAt(event->pos());
        if (!m_activeAnchor.isEmpty())
            return;
    }

    QQuickPaintedItem::mousePressEvent(event);
}

void QDeclarativeGeoMapCopyrightNotice::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_copyrightsHtml) {
        QString anchor = m_copyrightsHtml->documentLayout()->anchorAt(event->pos());
        if (anchor == m_activeAnchor && !anchor.isEmpty()) {
            emit linkActivated(anchor);
            m_activeAnchor.clear();
        }
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMapCopyrightNotice::setCopyrightsVisible(bool visible)
{
    m_copyrightsVisible = visible;

    setVisible(!m_copyrightsImage.isNull() && visible);
}

/*!
    \internal
*/
void QDeclarativeGeoMapCopyrightNotice::setCopyrightsZ(int copyrightsZ)
{
    setZ(copyrightsZ);
    update();
}

/*!
    \internal
*/
void QDeclarativeGeoMapCopyrightNotice::copyrightsChanged(const QImage &copyrightsImage)
{
    delete m_copyrightsHtml;
    m_copyrightsHtml = 0;

    m_copyrightsImage = copyrightsImage;

    setWidth(m_copyrightsImage.width());
    setHeight(m_copyrightsImage.height());

    setKeepMouseGrab(false);
    setAcceptedMouseButtons(Qt::NoButton);
    setVisible(m_copyrightsVisible);

    update();
}

void QDeclarativeGeoMapCopyrightNotice::copyrightsChanged(const QString &copyrightsHtml)
{
    if (copyrightsHtml.isEmpty() || !m_copyrightsVisible) {
        m_copyrightsImage = QImage();
        setVisible(false);
        return;
    } else {
        setVisible(true);
    }

    if (!m_copyrightsHtml)
        m_copyrightsHtml = new QTextDocument(this);

    m_copyrightsHtml->setHtml(copyrightsHtml);

    m_copyrightsImage = QImage(m_copyrightsHtml->size().toSize(),
                               QImage::Format_ARGB32_Premultiplied);
    m_copyrightsImage.fill(qPremultiply(qRgba(255, 255, 255, 128)));

    QPainter painter(&m_copyrightsImage);
    //m_copyrightsHtml->drawContents(&painter);  // <- this uses the default application palette, that might have, f.ex., white text
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, QColor(QStringLiteral("black")));
    ctx.palette.setColor(QPalette::Link, QColor(QStringLiteral("blue")));
    m_copyrightsHtml->documentLayout()->draw(&painter, ctx);

    setWidth(m_copyrightsImage.width());
    setHeight(m_copyrightsImage.height());

    setContentsSize(m_copyrightsImage.size());

    setKeepMouseGrab(true);
    setAcceptedMouseButtons(Qt::LeftButton);

    update();
}

QT_END_NAMESPACE
