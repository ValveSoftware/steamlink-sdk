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

#include "qcameraviewfinder.h"
#include "qvideowidget_p.h"

#include <qcamera.h>
#include <qvideodeviceselectorcontrol.h>
#include <private/qmediaobject_p.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

/*!
    \class QCameraViewfinder


    \brief The QCameraViewfinder class provides a camera viewfinder widget.

    \inmodule QtMultimediaWidgets
    \ingroup camera

    \snippet multimedia-snippets/camera.cpp Camera

*/

class QCameraViewfinderPrivate : public QVideoWidgetPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QCameraViewfinder)
public:
    QCameraViewfinderPrivate():
        QVideoWidgetPrivate()
    {
    }
};

/*!
    Constructs a new camera viewfinder widget.

    The \a parent is passed to QVideoWidget.
*/

QCameraViewfinder::QCameraViewfinder(QWidget *parent)
    :QVideoWidget(*new QCameraViewfinderPrivate, parent)
{
}

/*!
    Destroys a camera viewfinder widget.
*/
QCameraViewfinder::~QCameraViewfinder()
{
}

/*!
  \reimp
*/
QMediaObject *QCameraViewfinder::mediaObject() const
{
    return QVideoWidget::mediaObject();
}

/*!
  \reimp
*/
bool QCameraViewfinder::setMediaObject(QMediaObject *object)
{
    return QVideoWidget::setMediaObject(object);
}

QT_END_NAMESPACE

#include "moc_qcameraviewfinder.cpp"
