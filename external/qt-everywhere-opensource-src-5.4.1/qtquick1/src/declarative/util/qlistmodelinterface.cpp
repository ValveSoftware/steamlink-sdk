/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qlistmodelinterface_p.h"

QT_BEGIN_NAMESPACE

/*!
  \internal
  \class QListModelInterface
  \since 4.7
  \brief The QListModelInterface class can be subclassed to provide C++ models to QDeclarativeGraphics Views

  This class is comprised primarily of pure virtual functions which
  you must implement in a subclass. You can then use the subclass
  as a model for a QDeclarativeGraphics view, such as a QDeclarativeListView.
*/

/*! \fn QListModelInterface::QListModelInterface(QObject *parent)
  Constructs a QListModelInterface with the specified \a parent.
*/

  /*! \fn QListModelInterface::QListModelInterface(QObjectPrivate &dd, QObject *parent)

  \internal
 */

/*! \fn QListModelInterface::~QListModelInterface()
  The destructor is virtual.
 */

/*! \fn int QListModelInterface::count() const
  Returns the number of data entries in the model.
*/

/*! \fn QVariant QListModelInterface::data(int index, int role) const
  Returns the data at the given \a index for the specified \a roles.
*/

/*! \fn QList<int> QListModelInterface::roles() const
  Returns the list of roles for which the list model interface
  provides data.
*/

/*! \fn QString QListModelInterface::toString(int role) const
  Returns a string description of the specified \a role.
*/

/*! \fn void QListModelInterface::itemsInserted(int index, int count)
  Emit this signal when \a count items are inserted at \a index.
 */

/*! \fn void QListModelInterface::itemsRemoved(int index, int count)
  Emit this signal when \a count items are removed at \a index.
 */

/*! \fn void QListModelInterface::itemsMoved(int from, int to, int count)
  Emit this signal when \a count items are moved from index \a from
  to index \a to.
 */

/*! \fn void QListModelInterface::itemsChanged(int index, int count, const QList<int> &roles)
  Emit this signal when \a count items at \a index have had their
  \a roles changed.
 */

QT_END_NAMESPACE
