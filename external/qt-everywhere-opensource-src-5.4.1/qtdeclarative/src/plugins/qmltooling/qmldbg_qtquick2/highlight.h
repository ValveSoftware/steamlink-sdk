/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include <QtCore/QPointer>
#include <QtCore/QPointF>
#include <QtGui/QTransform>
#include <QtQuick/QQuickPaintedItem>


namespace QmlJSDebugger {
namespace QtQuick2 {

class Highlight : public QQuickPaintedItem
{
    Q_OBJECT

public:
    Highlight(QQuickItem *parent);
    Highlight(QQuickItem *item, QQuickItem *parent);

    void setItem(QQuickItem *item);
    QQuickItem *item() {return m_item;}

protected:
    QTransform transform() {return m_transform;}

private:
    void initRenderDetails();

private slots:
    void adjust();

private:
    QPointer<QQuickItem> m_item;
    QTransform m_transform;
};

/**
 * A highlight suitable for indicating selection.
 */
class SelectionHighlight : public Highlight
{
    Q_OBJECT

public:
    SelectionHighlight(const QString &name, QQuickItem *item, QQuickItem *parent);
    void paint(QPainter *painter);
    void showName(const QPointF &displayPoint);

private slots:
    void disableNameDisplay();

private:
    QPointF m_displayPoint;
    QString m_name;
    bool m_nameDisplayActive;
};

/**
 * A highlight suitable for indicating hover.
 */
class HoverHighlight : public Highlight
{
public:
    HoverHighlight(QQuickItem *parent)
        : Highlight(parent)
    {
        setZ(1); // hover highlight on top of selection highlight
    }

    void paint(QPainter *painter);
};

} // namespace QtQuick2
} // namespace QmlJSDebugger

#endif // HIGHLIGHT_H
