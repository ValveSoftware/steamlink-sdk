/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qmediacontrol_p.h"
#include <qmetadatawritercontrol.h>

QT_BEGIN_NAMESPACE


/*!
    \class QMetaDataWriterControl
    \inmodule QtMultimedia


    \ingroup multimedia_control


    \brief The QMetaDataWriterControl class provides write access to the
    meta-data of a QMediaService's media.

    If a QMediaService can provide write access to the meta-data of its
    current media it will implement QMetaDataWriterControl.  This control
    provides functions for both retrieving and setting meta-data values.
    Meta-data may be addressed by the keys defined in the
    QMediaMetaData namespace.

    The functionality provided by this control is exposed to application code
    by the meta-data members of QMediaObject, and so meta-data access is
    potentially available in any of the media object classes.  Any media
    service may implement QMetaDataControl.

    The interface name of QMetaDataWriterControl is \c org.qt-project.qt.metadatawritercontrol/5.0 as
    defined in QMetaDataWriterControl_iid.

    \sa QMediaService::requestControl(), QMediaObject
*/

/*!
    \macro QMetaDataWriterControl_iid

    \c org.qt-project.qt.metadatawritercontrol/5.0

    Defines the interface name of the QMetaDataWriterControl class.

    \relates QMetaDataWriterControl
*/

/*!
    Construct a QMetaDataWriterControl with \a parent. This class is meant as a base class
    for service specific meta data providers so this constructor is protected.
*/

QMetaDataWriterControl::QMetaDataWriterControl(QObject *parent):
    QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroy the meta-data writer control.
*/

QMetaDataWriterControl::~QMetaDataWriterControl()
{
}

/*!
    \fn bool QMetaDataWriterControl::isMetaDataAvailable() const

    Identifies if meta-data is available from a media service.

    Returns true if the meta-data is available and false otherwise.
*/

/*!
    \fn bool QMetaDataWriterControl::isWritable() const

    Identifies if a media service's meta-data can be edited.

    Returns true if the meta-data is writable and false otherwise.
*/

/*!
    \fn QVariant QMetaDataWriterControl::metaData(const QString &key) const

    Returns the meta-data for the given \a key.
*/

/*!
    \fn void QMetaDataWriterControl::setMetaData(const QString &key, const QVariant &value)

    Sets the \a value of the meta-data element with the given \a key.
*/

/*!
    \fn QMetaDataWriterControl::availableMetaData() const

    Returns a list of keys there is meta-data available for.
*/

/*!
    \fn void QMetaDataWriterControl::metaDataChanged()

    Signal the changes of meta-data.

    If multiple meta-data elements are changed,
    metaDataChanged(const QString &key, const QVariant &value) signal is emitted
    for each of them with metaDataChanged() changed emitted once.
*/

/*!
    \fn void QMetaDataWriterControl::metaDataChanged(const QString &key, const QVariant &value)

    Signal the changes of one meta-data element \a value with the given \a key.
*/

/*!
    \fn void QMetaDataWriterControl::metaDataAvailableChanged(bool available)

    Signal the availability of meta-data has changed, \a available will
    be true if the multimedia object has meta-data.
*/

/*!
    \fn void QMetaDataWriterControl::writableChanged(bool writable)

    Signal a change in the writable status of meta-data, \a writable will be
    true if meta-data elements can be added or adjusted.
*/

#include "moc_qmetadatawritercontrol.cpp"
QT_END_NAMESPACE

