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

#include "selectionlistmodel.h"
#include "abstractinputmethod.h"
#include <QtCore/private/qabstractitemmodel_p.h>

namespace QtVirtualKeyboard {

class SelectionListModelPrivate : public QAbstractItemModelPrivate
{
public:
    SelectionListModelPrivate() :
        QAbstractItemModelPrivate(),
        dataSource(0),
        type(SelectionListModel::WordCandidateList),
        rowCount(0)
    {
    }

    QHash<int, QByteArray> roles;
    AbstractInputMethod *dataSource;
    SelectionListModel::Type type;
    int rowCount;
};

/*!
    \qmltype SelectionListModel
    \instantiates QtVirtualKeyboard::SelectionListModel
    \inqmlmodule QtQuick.VirtualKeyboard
    \ingroup qtvirtualkeyboard-qml
    \brief Provides a data model for the selection lists.

    The SelectionListModel is a data model for word candidates
    provided by the input method.

    An instance of SelectionListModel cannot be created directly.
    Instead, the InputEngine manages the instances and provides
    access to the model by InputEngine::wordCandidateListModel
    property.

    The model exposes the following data roles for the list delegate:
    \list
        \li \c display Display text for item
        \li \c wordCompletionLength Word completion length for item
    \endlist

    The activeItemChanged signal indicates which item is currently
    highlighted by the input method. The view should respond to this
    signal by highlighting the corresponding item in the list.

    The user selection is handled by the selectItem() method. The view
    should be invoke this method when the user selects an item from the
    list.
*/

/*!
    \class QtVirtualKeyboard::SelectionListModel

    \inmodule QtVirtualKeyboard

    \brief List model for selection lists.

    \internal

    This class acts as a bridge between the UI and the
    input method that provides the data for selection
    lists.
*/

/*!
    \enum QtVirtualKeyboard::SelectionListModel::Type

    This enum specifies the type of selection list.

    \value WordCandidateList
           Shows list of word candidates
*/

/*!
    \enum QtVirtualKeyboard::SelectionListModel::Role

    This enum specifies a role of the data requested.

    \value DisplayRole
           The data to be rendered in form of text.
    \value WordCompletionLengthRole
           An integer specifying the length of the word
           the completion part expressed as the
           number of characters counted from the
           end of the string.
*/

SelectionListModel::SelectionListModel(QObject *parent) :
    QAbstractListModel(*new SelectionListModelPrivate(), parent)
{
    Q_D(SelectionListModel);
    d->roles[DisplayRole] = "display";
    d->roles[WordCompletionLengthRole] = "wordCompletionLength";
}

/*!
    \internal
*/
SelectionListModel::~SelectionListModel()
{
}

/*!
    \internal
*/
void SelectionListModel::setDataSource(AbstractInputMethod *dataSource, Type type)
{
    Q_D(SelectionListModel);
    if (d->dataSource) {
        disconnect(this, SLOT(selectionListChanged(int)));
        disconnect(this, SLOT(selectionListActiveItemChanged(int, int)));
    }
    d->type = type;
    if (d->dataSource) {
        d->dataSource = 0;
        selectionListChanged(type);
        selectionListActiveItemChanged(type, -1);
    }
    d->dataSource = dataSource;
    if (d->dataSource) {
        connect(d->dataSource, SIGNAL(selectionListChanged(int)), SLOT(selectionListChanged(int)));
        connect(d->dataSource, SIGNAL(selectionListActiveItemChanged(int, int)), SLOT(selectionListActiveItemChanged(int, int)));
    }
}

/*!
    \internal
*/
AbstractInputMethod *SelectionListModel::dataSource() const
{
    Q_D(const SelectionListModel);
    return d->dataSource;
}

/*!
    \internal
*/
int SelectionListModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const SelectionListModel);
    Q_UNUSED(parent)
    return d->rowCount;
}

/*!
    \internal
*/
QVariant SelectionListModel::data(const QModelIndex &index, int role) const
{
    Q_D(const SelectionListModel);
    return d->dataSource ? d->dataSource->selectionListData(d->type, index.row(), role) : QVariant();
}

/*!
    \internal
*/
QHash<int,QByteArray> SelectionListModel::roleNames() const
{
    Q_D(const SelectionListModel);
    return d->roles;
}

/*! \qmlmethod void SelectionListModel::selectItem(int index)

    This method should be called when the user selects an item at position
    \a index from the list.
    The selection is forwarded to the input method for further processing.
*/
/*!
    \fn void QtVirtualKeyboard::SelectionListModel::selectItem(int index)

    This method should be called when the user selects an item at position
    \a index from the list.
    The selection is forwarded to the input method for further processing.
*/
void SelectionListModel::selectItem(int index)
{
    Q_D(SelectionListModel);
    if (index >= 0 && index < d->rowCount && d->dataSource) {
        emit itemSelected(index);
        d->dataSource->selectionListItemSelected(d->type, index);
    }
}

/*!
 * \internal
 */
QVariant SelectionListModel::dataAt(int index, int role) const
{
    return data(this->index(index, 0), role);
}

/*!
    \internal
*/
void SelectionListModel::selectionListChanged(int type)
{
    Q_D(SelectionListModel);
    if (static_cast<Type>(type) == d->type) {
        int oldCount = d->rowCount;
        int newCount = d->dataSource ? d->dataSource->selectionListItemCount(d->type) : 0;
        if (newCount) {
            int changedCount = qMin(oldCount, newCount);
            if (changedCount)
                dataChanged(index(0), index(changedCount - 1));
            if (oldCount > newCount) {
                beginRemoveRows(QModelIndex(), newCount, oldCount - 1);
                d->rowCount = newCount;
                endRemoveRows();
            } else if (oldCount < newCount) {
                beginInsertRows(QModelIndex(), oldCount, newCount - 1);
                d->rowCount = newCount;
                endInsertRows();
            }
        } else {
            beginResetModel();
            d->rowCount = 0;
            endResetModel();
        }
    }
}

/*!
    \internal
*/
void SelectionListModel::selectionListActiveItemChanged(int type, int index)
{
    Q_D(SelectionListModel);
    if (static_cast<Type>(type) == d->type) {
        emit activeItemChanged(index);
    }
}

/*!
    \qmlsignal void SelectionListModel::activeItemChanged(int index)

    This signal is emitted when the active item in the list changes. The
    UI should react to this signal by highlighting the item at \a index in
    the list.
*/
/*!
    \fn void QtVirtualKeyboard::SelectionListModel::activeItemChanged(int index)

    This signal is emitted when the active item in the list changes. The
    UI should react to this signal by highlighting the item at \a index in
    the list.
*/

/*!
    \qmlsignal void SelectionListModel::itemSelected(int index)

    This signal is emitted when an item at \a index is selected by the user.
*/
/*!
    \fn void QtVirtualKeyboard::SelectionListModel::itemSelected(int index)

    This signal is emitted when an item at \a index is selected by the user.
*/

} // namespace QtVirtualKeyboard
