/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickdroparea_p.h"
#include "qquickdrag_p.h"
#include "qquickitem_p.h"

#include <private/qv4arraybuffer_p.h>

#ifndef QT_NO_DRAGANDDROP

QT_BEGIN_NAMESPACE

QQuickDropAreaDrag::QQuickDropAreaDrag(QQuickDropAreaPrivate *d, QObject *parent)
    : QObject(parent)
    , d(d)
{
}

QQuickDropAreaDrag::~QQuickDropAreaDrag()
{
}

class QQuickDropAreaPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QQuickDropArea)

public:
    QQuickDropAreaPrivate();
    ~QQuickDropAreaPrivate();

    bool hasMatchingKey(const QStringList &keys) const;

    QStringList getKeys(const QMimeData *mimeData) const;

    QStringList keys;
    QRegExp keyRegExp;
    QPointF dragPosition;
    QQuickDropAreaDrag *drag;
    QPointer<QObject> source;
    bool containsDrag;
};

QQuickDropAreaPrivate::QQuickDropAreaPrivate()
    : drag(0)
    , containsDrag(false)
{
}

QQuickDropAreaPrivate::~QQuickDropAreaPrivate()
{
    delete drag;
}

/*!
    \qmltype DropArea
    \instantiates QQuickDropArea
    \inqmlmodule QtQuick
    \ingroup qtquick-input
    \brief For specifying drag and drop handling in an area

    A DropArea is an invisible item which receives events when other items are
    dragged over it.

    The \l Drag attached property can be used to notify the DropArea when an Item is
    dragged over it.

    The \l keys property can be used to filter drag events which don't include
    a matching key.

    The \l drag.source property is communicated to the source of a drag event as
    the recipient of a drop on the drag target.

    \sa {Qt Quick Examples - Drag and Drop}, {Qt Quick Examples - externaldraganddrop}
*/

QQuickDropArea::QQuickDropArea(QQuickItem *parent)
    : QQuickItem(*new QQuickDropAreaPrivate, parent)
{
    setFlags(ItemAcceptsDrops);
}

QQuickDropArea::~QQuickDropArea()
{
}

/*!
    \qmlproperty bool QtQuick::DropArea::containsDrag

    This property identifies whether the DropArea currently contains any
    dragged items.
*/

bool QQuickDropArea::containsDrag() const
{
    Q_D(const QQuickDropArea);
    return d->containsDrag;
}

/*!
    \qmlproperty stringlist QtQuick::DropArea::keys

    This property holds a list of drag keys a DropArea will accept.

    If no keys are listed the DropArea will accept events from any drag source,
    otherwise the drag source must have at least one compatible key.

    \sa QtQuick::Drag::keys
*/

QStringList QQuickDropArea::keys() const
{
    Q_D(const QQuickDropArea);
    return d->keys;
}

void QQuickDropArea::setKeys(const QStringList &keys)
{
    Q_D(QQuickDropArea);
    if (d->keys != keys) {
        d->keys = keys;

        if (keys.isEmpty()) {
            d->keyRegExp = QRegExp();
        } else {
            QString pattern = QLatin1Char('(') + QRegExp::escape(keys.first());
            for (int i = 1; i < keys.count(); ++i)
                pattern += QLatin1Char('|') + QRegExp::escape(keys.at(i));
            pattern += QLatin1Char(')');
            d->keyRegExp = QRegExp(pattern.replace(QLatin1String("\\*"), QLatin1String(".+")));
        }
        emit keysChanged();
    }
}

QQuickDropAreaDrag *QQuickDropArea::drag()
{
    Q_D(QQuickDropArea);
    if (!d->drag)
        d->drag = new QQuickDropAreaDrag(d);
    return d->drag;
}

/*!
    \qmlproperty Object QtQuick::DropArea::drag.source

    This property holds the source of a drag.
*/

QObject *QQuickDropAreaDrag::source() const
{
    return d->source;
}

/*!
    \qmlpropertygroup QtQuick::DropArea::drag
    \qmlproperty qreal QtQuick::DropArea::drag.x
    \qmlproperty qreal QtQuick::DropArea::drag.y

    These properties hold the coordinates of the last drag event.
*/

qreal QQuickDropAreaDrag::x() const
{
    return d->dragPosition.x();
}

qreal QQuickDropAreaDrag::y() const
{
    return d->dragPosition.y();
}

/*!
    \qmlsignal QtQuick::DropArea::positionChanged(DragEvent drag)

    This signal is emitted when the position of a drag has changed.

    The corresponding handler is \c onPositionChanged.
*/

void QQuickDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    Q_D(QQuickDropArea);
    if (!d->containsDrag)
        return;

    d->dragPosition = event->pos();
    if (d->drag)
        emit d->drag->positionChanged();

    event->accept();
    QQuickDropEvent dragTargetEvent(d, event);
    emit positionChanged(&dragTargetEvent);
}

bool QQuickDropAreaPrivate::hasMatchingKey(const QStringList &keys) const
{
    if (keyRegExp.isEmpty())
        return true;

    QRegExp copy = keyRegExp;
    for (const QString &key : keys) {
        if (copy.exactMatch(key))
            return true;
    }
    return false;
}

QStringList QQuickDropAreaPrivate::getKeys(const QMimeData *mimeData) const
{
    if (const QQuickDragMimeData *dragMime = qobject_cast<const QQuickDragMimeData *>(mimeData))
        return dragMime->keys();
    return mimeData->formats();
}

/*!
    \qmlsignal QtQuick::DropArea::entered(DragEvent drag)

    This signal is emitted when a \a drag enters the bounds of a DropArea.

    The corresponding handler is \c onEntered.
*/

void QQuickDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    Q_D(QQuickDropArea);
    const QMimeData *mimeData = event->mimeData();
    if (!d->effectiveEnable || d->containsDrag || !mimeData || !d->hasMatchingKey(d->getKeys(mimeData)))
        return;

    d->dragPosition = event->pos();

    event->accept();

    QQuickDropEvent dragTargetEvent(d, event);
    emit entered(&dragTargetEvent);
    if (!event->isAccepted())
        return;

    d->containsDrag = true;
    if (QQuickDragMimeData *dragMime = qobject_cast<QQuickDragMimeData *>(const_cast<QMimeData *>(mimeData)))
        d->source = dragMime->source();
    else
        d->source = event->source();
    d->dragPosition = event->pos();
    if (d->drag) {
        emit d->drag->positionChanged();
        emit d->drag->sourceChanged();
    }
    emit containsDragChanged();
}

/*!
    \qmlsignal QtQuick::DropArea::exited()

    This signal is emitted when a drag exits the bounds of a DropArea.

    The corresponding handler is \c onExited.
*/

void QQuickDropArea::dragLeaveEvent(QDragLeaveEvent *)
{
    Q_D(QQuickDropArea);
    if (!d->containsDrag)
        return;

    emit exited();

    d->containsDrag = false;
    d->source = 0;
    emit containsDragChanged();
    if (d->drag)
        emit d->drag->sourceChanged();
}

/*!
    \qmlsignal QtQuick::DropArea::dropped(DragEvent drop)

    This signal is emitted when a drop event occurs within the bounds of
    a DropArea.

    The corresponding handler is \c onDropped.
*/

void QQuickDropArea::dropEvent(QDropEvent *event)
{
    Q_D(QQuickDropArea);
    if (!d->containsDrag)
        return;

    QQuickDropEvent dragTargetEvent(d, event);
    emit dropped(&dragTargetEvent);

    d->containsDrag = false;
    d->source = 0;
    emit containsDragChanged();
    if (d->drag)
        emit d->drag->sourceChanged();
}

/*!
    \qmltype DragEvent
    \instantiates QQuickDropEvent
    \inqmlmodule QtQuick
    \ingroup qtquick-input-events
    \brief Provides information about a drag event

    The position of the drag event can be obtained from the \l x and \l y
    properties, and the \l keys property identifies the drag keys of the event
    \l {drag.source}{source}.

    The existence of specific drag types can be determined using the \l hasColor,
    \l hasHtml, \l hasText, and \l hasUrls properties.

    The list of all supplied formats can be determined using the \l formats property.

    Specific drag types can be obtained using the \l colorData, \l html, \l text,
    and \l urls properties.

    A string version of any available mimeType can be obtained using \l getDataAsString.
*/

/*!
    \qmlproperty real QtQuick::DragEvent::x

    This property holds the x coordinate of a drag event.
*/

/*!
    \qmlproperty real QtQuick::DragEvent::y

    This property holds the y coordinate of a drag event.
*/

/*!
    \qmlproperty Object QtQuick::DragEvent::drag.source

    This property holds the source of a drag event.
*/

/*!
    \qmlproperty stringlist QtQuick::DragEvent::keys

    This property holds a list of keys identifying the data type or source of a
    drag event.
*/

/*!
    \qmlproperty enumeration QtQuick::DragEvent::action

    This property holds the action that the \l {drag.source}{source} is to perform on an accepted drop.

    The drop action may be one of:

    \list
    \li Qt.CopyAction Copy the data to the target.
    \li Qt.MoveAction Move the data from the source to the target.
    \li Qt.LinkAction Create a link from the source to the target.
    \li Qt.IgnoreAction Ignore the action (do nothing with the data).
    \endlist
*/

/*!
    \qmlproperty flags QtQuick::DragEvent::supportedActions

    This property holds the set of \l {action}{actions} supported by the
    drag source.
*/

/*!
    \qmlproperty flags QtQuick::DragEvent::proposedAction
    \since 5.2

    This property holds the set of \l {action}{actions} proposed by the
    drag source.
*/

/*!
    \qmlproperty bool QtQuick::DragEvent::accepted

    This property holds whether the drag event was accepted by a handler.

    The default value is true.
*/

/*!
    \qmlmethod QtQuick::DragEvent::accept()
    \qmlmethod QtQuick::DragEvent::accept(enumeration action)

    Accepts the drag event.

    If an \a action is specified it will overwrite the value of the \l action property.
*/

/*!
    \qmlmethod QtQuick::DragEvent::acceptProposedAction()
    \since 5.2

    Accepts the drag event with the \l proposedAction.
*/

/*!
    \qmlproperty bool QtQuick::DragEvent::hasColor
    \since 5.2

    This property holds whether the drag event contains a color item.
*/

/*!
    \qmlproperty bool QtQuick::DragEvent::hasHtml
    \since 5.2

    This property holds whether the drag event contains a html item.
*/

/*!
    \qmlproperty bool QtQuick::DragEvent::hasText
    \since 5.2

    This property holds whether the drag event contains a text item.
*/

/*!
    \qmlproperty bool QtQuick::DragEvent::hasUrls
    \since 5.2

    This property holds whether the drag event contains one or more url items.
*/

/*!
    \qmlproperty color QtQuick::DragEvent::colorData
    \since 5.2

    This property holds color data, if any.
*/

/*!
    \qmlproperty string QtQuick::DragEvent::html
    \since 5.2

    This property holds html data, if any.
*/

/*!
    \qmlproperty string QtQuick::DragEvent::text
    \since 5.2

    This property holds text data, if any.
*/

/*!
    \qmlproperty urllist QtQuick::DragEvent::urls
    \since 5.2

    This property holds a list of urls, if any.
*/

/*!
    \qmlproperty stringlist QtQuick::DragEvent::formats
    \since 5.2

    This property holds a list of mime type formats contained in the drag data.
*/

/*!
    \qmlmethod string QtQuick::DragEvent::getDataAsString(string format)
    \since 5.2

    Returns the data for the given \a format converted to a string. \a format should be one contained in the \l formats property.
*/

/*!
    \qmlmethod string QtQuick::DragEvent::getDataAsArrayBuffer(string format)
    \since 5.5

    Returns the data for the given \a format into an ArrayBuffer, which can
    easily be translated into a QByteArray. \a format should be one contained in the \l formats property.
*/

QObject *QQuickDropEvent::source()
{
    if (const QQuickDragMimeData *dragMime = qobject_cast<const QQuickDragMimeData *>(event->mimeData()))
        return dragMime->source();
    else
        return event->source();
}

QStringList QQuickDropEvent::keys() const
{
    return d->getKeys(event->mimeData());
}

bool QQuickDropEvent::hasColor() const
{
    return event->mimeData()->hasColor();
}

bool QQuickDropEvent::hasHtml() const
{
    return event->mimeData()->hasHtml();
}

bool QQuickDropEvent::hasText() const
{
    return event->mimeData()->hasText();
}

bool QQuickDropEvent::hasUrls() const
{
    return event->mimeData()->hasUrls();
}

QVariant QQuickDropEvent::colorData() const
{
    return event->mimeData()->colorData();
}

QString QQuickDropEvent::html() const
{
    return event->mimeData()->html();
}

QString QQuickDropEvent::text() const
{
    return event->mimeData()->text();
}

QList<QUrl> QQuickDropEvent::urls() const
{
    return event->mimeData()->urls();
}

QStringList QQuickDropEvent::formats() const
{
    return event->mimeData()->formats();
}

void QQuickDropEvent::getDataAsString(QQmlV4Function *args)
{
    if (args->length() != 0) {
        QV4::ExecutionEngine *v4 = args->v4engine();
        QV4::Scope scope(v4);
        QV4::ScopedValue v(scope, (*args)[0]);
        QString format = v->toQString();
        QString rv = QString::fromUtf8(event->mimeData()->data(format));
        args->setReturnValue(v4->newString(rv)->asReturnedValue());
    }
}

void QQuickDropEvent::getDataAsArrayBuffer(QQmlV4Function *args)
{
    if (args->length() != 0) {
        QV4::ExecutionEngine *v4 = args->v4engine();
        QV4::Scope scope(v4);
        QV4::ScopedValue v(scope, (*args)[0]);
        const QString format = v->toQString();
        args->setReturnValue(v4->newArrayBuffer(event->mimeData()->data(format))->asReturnedValue());
    }
}

void QQuickDropEvent::acceptProposedAction(QQmlV4Function *)
{
    event->acceptProposedAction();
}

void QQuickDropEvent::accept(QQmlV4Function *args)
{
    Qt::DropAction action = event->dropAction();

    if (args->length() >= 1) {
        QV4::Scope scope(args->v4engine());
        QV4::ScopedValue v(scope, (*args)[0]);
        if (v->isInt32())
            action = Qt::DropAction(v->integerValue());
    }

    // get action from arguments.
    event->setDropAction(action);
    event->accept();
}


QT_END_NAMESPACE

#endif // QT_NO_DRAGANDDROP
