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

#include "qquicklabel_p.h"
#include "qquicklabel_p_p.h"
#include "qquickcontrol_p.h"
#include "qquickcontrol_p_p.h"

#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickclipnode_p.h>

#ifndef QT_NO_ACCESSIBILITY
#include <QtQuick/private/qquickaccessibleattached_p.h>
#endif

QT_BEGIN_NAMESPACE

/*!
    \qmltype Label
    \inherits Text
    \instantiates QQuickLabel
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup text
    \brief Styled text label with inherited font.

    Label extends \l Text with styling and \l {Control::font}{font}
    inheritance. The default colors and font are style specific. Label
    can also have a visual \l background item.

    \image qtquickcontrols2-label.png

    \snippet qtquickcontrols2-label.qml 1

    You can use the properties of \l Text to change the appearance of the text as desired:

    \qml
     Label {
         text: "Hello world"
         font.pixelSize: 22
         font.italic: true
     }
    \endqml

    \sa {Customizing Label}
*/

QQuickLabel::QQuickLabel(QQuickItem *parent) :
    QQuickText(*(new QQuickLabelPrivate), parent)
{
    Q_D(QQuickLabel);
    QObjectPrivate::connect(this, &QQuickText::textChanged,
                            d, &QQuickLabelPrivate::_q_textChanged);
}

QQuickLabel::~QQuickLabel()
{
}

QQuickLabelPrivate::QQuickLabelPrivate()
    : background(nullptr), accessibleAttached(nullptr)
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installActivationObserver(this);
#endif
}

QQuickLabelPrivate::~QQuickLabelPrivate()
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::removeActivationObserver(this);
#endif
}

/*!
    \internal

    Determine which font is implicitly imposed on this control by its ancestors
    and QGuiApplication::font, resolve this against its own font (attributes from
    the implicit font are copied over). Then propagate this font to this
    control's children.
*/
void QQuickLabelPrivate::resolveFont()
{
    Q_Q(QQuickLabel);
    inheritFont(QQuickControlPrivate::parentFont(q));
}

void QQuickLabelPrivate::inheritFont(const QFont &f)
{
    Q_Q(QQuickLabel);
    QFont parentFont = font.resolve(f);
    parentFont.resolve(font.resolve() | f.resolve());

    const QFont defaultFont = QQuickControlPrivate::themeFont(QPlatformTheme::LabelFont);
    const QFont resolvedFont = parentFont.resolve(defaultFont);

    const bool changed = resolvedFont != sourceFont;
    q->QQuickText::setFont(resolvedFont);
    if (changed)
        emit q->fontChanged();
}

void QQuickLabelPrivate::_q_textChanged(const QString &text)
{
#ifndef QT_NO_ACCESSIBILITY
    if (accessibleAttached)
        accessibleAttached->setName(text);
#else
    Q_UNUSED(text)
#endif
}

#ifndef QT_NO_ACCESSIBILITY
void QQuickLabelPrivate::accessibilityActiveChanged(bool active)
{
    if (accessibleAttached || !active)
        return;

    Q_Q(QQuickLabel);
    accessibleAttached = qobject_cast<QQuickAccessibleAttached *>(qmlAttachedPropertiesObject<QQuickAccessibleAttached>(q, true));
    if (accessibleAttached) {
        accessibleAttached->setRole(accessibleRole());
        accessibleAttached->setName(text);
    } else {
        qWarning() << "QQuickLabel: " << q << " QQuickAccessibleAttached object creation failed!";
    }
}

QAccessible::Role QQuickLabelPrivate::accessibleRole() const
{
    return QAccessible::StaticText;
}
#endif

/*
   Deletes "delegate" if Component.completed() has been emitted,
   otherwise stores it in pendingDeletions.
*/
void QQuickLabelPrivate::deleteDelegate(QObject *delegate)
{
    if (componentComplete)
        delete delegate;
    else
        pendingDeletions.append(delegate);
}

QFont QQuickLabel::font() const
{
    return QQuickText::font();
}

void QQuickLabel::setFont(const QFont &font)
{
    Q_D(QQuickLabel);
    if (d->font.resolve() == font.resolve() && d->font == font)
        return;

    d->font = font;
    d->resolveFont();
}

/*!
    \qmlproperty Item QtQuick.Controls::Label::background

    This property holds the background item.

    \note If the background item has no explicit size specified, it automatically
          follows the control's size. In most cases, there is no need to specify
          width or height for a background item.

    \sa {Customizing Label}
*/
QQuickItem *QQuickLabel::background() const
{
    Q_D(const QQuickLabel);
    return d->background;
}

void QQuickLabel::setBackground(QQuickItem *background)
{
    Q_D(QQuickLabel);
    if (d->background == background)
        return;

    d->deleteDelegate(d->background);
    d->background = background;
    if (background) {
        background->setParentItem(this);
        if (qFuzzyIsNull(background->z()))
            background->setZ(-1);
    }
    emit backgroundChanged();
}

void QQuickLabel::classBegin()
{
    Q_D(QQuickLabel);
    QQuickText::classBegin();
    d->resolveFont();
}

void QQuickLabel::componentComplete()
{
    Q_D(QQuickLabel);
    QQuickText::componentComplete();
#ifndef QT_NO_ACCESSIBILITY
    if (!d->accessibleAttached && QAccessible::isActive())
        d->accessibilityActiveChanged(true);
#endif

    qDeleteAll(d->pendingDeletions);
    d->pendingDeletions.clear();
}

void QQuickLabel::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    Q_D(QQuickLabel);
    QQuickText::itemChange(change, value);
    if (change == ItemParentHasChanged && value.item)
        d->resolveFont();
}

void QQuickLabel::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickLabel);
    QQuickText::geometryChanged(newGeometry, oldGeometry);
    if (d->background) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(d->background);
        if (!p->widthValid) {
            d->background->setWidth(newGeometry.width());
            p->widthValid = false;
        }
        if (!p->heightValid) {
            d->background->setHeight(newGeometry.height());
            p->heightValid = false;
        }
    }
}

QT_END_NAMESPACE
