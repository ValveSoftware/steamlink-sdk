/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
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

#include "qquickpagelayout_p_p.h"
#include "qquickcontrol_p.h"
#include "qquicktoolbar_p.h"
#include "qquicktabbar_p.h"
#include "qquickdialogbuttonbox_p.h"

#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

static const QQuickItemPrivate::ChangeTypes ItemChanges = QQuickItemPrivate::Geometry | QQuickItemPrivate::Visibility | QQuickItemPrivate::Destroyed
                                                        | QQuickItemPrivate::ImplicitWidth | QQuickItemPrivate::ImplicitHeight;

namespace {
    enum Position {
        Header,
        Footer
    };

    Q_STATIC_ASSERT(int(Header) == int(QQuickTabBar::Header));
    Q_STATIC_ASSERT(int(Footer) == int(QQuickTabBar::Footer));

    Q_STATIC_ASSERT(int(Header) == int(QQuickToolBar::Header));
    Q_STATIC_ASSERT(int(Footer) == int(QQuickToolBar::Footer));

    Q_STATIC_ASSERT(int(Header) == int(QQuickDialogButtonBox::Header));
    Q_STATIC_ASSERT(int(Footer) == int(QQuickDialogButtonBox::Footer));
}

static void setPosition(QQuickItem *item, Position position)
{
    if (QQuickToolBar *toolBar = qobject_cast<QQuickToolBar *>(item))
        toolBar->setPosition(static_cast<QQuickToolBar::Position>(position));
    else if (QQuickTabBar *tabBar = qobject_cast<QQuickTabBar *>(item))
        tabBar->setPosition(static_cast<QQuickTabBar::Position>(position));
    else if (QQuickDialogButtonBox *buttonBox = qobject_cast<QQuickDialogButtonBox *>(item))
        buttonBox->setPosition(static_cast<QQuickDialogButtonBox::Position>(position));
}

QQuickPageLayout::QQuickPageLayout(QQuickControl *control)
    : m_header(nullptr), m_footer(nullptr), m_control(control)
{
}

QQuickPageLayout::~QQuickPageLayout()
{
    if (m_header)
        QQuickItemPrivate::get(m_header)->removeItemChangeListener(this, ItemChanges);
    if (m_footer)
        QQuickItemPrivate::get(m_footer)->removeItemChangeListener(this, ItemChanges);
}

QQuickItem *QQuickPageLayout::header() const
{
    return m_header;
}

bool QQuickPageLayout::setHeader(QQuickItem *header)
{
    if (m_header == header)
        return false;

    if (m_header) {
        QQuickItemPrivate::get(m_header)->removeItemChangeListener(this, ItemChanges);
        m_header->setParentItem(nullptr);
    }
    m_header = header;
    if (header) {
        header->setParentItem(m_control);
        QQuickItemPrivate::get(header)->addItemChangeListener(this, ItemChanges);
        if (qFuzzyIsNull(header->z()))
            header->setZ(1);
        setPosition(header, Header);
    }
    return true;
}

QQuickItem *QQuickPageLayout::footer() const
{
    return m_footer;
}

bool QQuickPageLayout::setFooter(QQuickItem *footer)
{
    if (m_footer == footer)
        return false;

    if (m_footer) {
        QQuickItemPrivate::get(m_footer)->removeItemChangeListener(this, ItemChanges);
        m_footer->setParentItem(nullptr);
    }
    m_footer = footer;
    if (footer) {
        footer->setParentItem(m_control);
        QQuickItemPrivate::get(footer)->addItemChangeListener(this, ItemChanges);
        if (qFuzzyIsNull(footer->z()))
            footer->setZ(1);
        setPosition(footer, Footer);
    }
    return true;
}

void QQuickPageLayout::update()
{
    QQuickItem *content = m_control->contentItem();

    const qreal hh = m_header && m_header->isVisible() ? m_header->height() : 0;
    const qreal fh = m_footer && m_footer->isVisible() ? m_footer->height() : 0;
    const qreal hsp = hh > 0 ? m_control->spacing() : 0;
    const qreal fsp = fh > 0 ? m_control->spacing() : 0;

    content->setY(m_control->topPadding() + hh + hsp);
    content->setX(m_control->leftPadding());
    content->setWidth(m_control->availableWidth());
    content->setHeight(m_control->availableHeight() - hh - fh - hsp - fsp);

    if (m_header)
        m_header->setWidth(m_control->width());

    if (m_footer) {
        m_footer->setY(m_control->height() - m_footer->height());
        m_footer->setWidth(m_control->width());
    }
}

void QQuickPageLayout::itemVisibilityChanged(QQuickItem *)
{
    update();
}

void QQuickPageLayout::itemImplicitWidthChanged(QQuickItem *)
{
    update();
}

void QQuickPageLayout::itemImplicitHeightChanged(QQuickItem *)
{
    update();
}

void QQuickPageLayout::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    update();
}

void QQuickPageLayout::itemDestroyed(QQuickItem *item)
{
    if (item == m_header)
        m_header = nullptr;
    else if (item == m_footer)
        m_footer = nullptr;
}

QT_END_NAMESPACE
