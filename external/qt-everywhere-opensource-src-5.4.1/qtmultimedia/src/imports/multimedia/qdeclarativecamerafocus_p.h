/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QDECLARATIVECAMERAFOCUS_H
#define QDECLARATIVECAMERAFOCUS_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qabstractitemmodel.h>
#include <qcamera.h>
#include <qcamerafocus.h>
#include "qdeclarativecamera_p.h"

QT_BEGIN_NAMESPACE

class FocusZonesModel;
class QDeclarativeCamera;

class QDeclarativeCameraFocus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(FocusMode focusMode READ focusMode WRITE setFocusMode NOTIFY focusModeChanged)
    Q_PROPERTY(FocusPointMode focusPointMode READ focusPointMode WRITE setFocusPointMode NOTIFY focusPointModeChanged)
    Q_PROPERTY(QPointF customFocusPoint READ customFocusPoint WRITE setCustomFocusPoint NOTIFY customFocusPointChanged)
    Q_PROPERTY(QObject *focusZones READ focusZones CONSTANT)

    Q_ENUMS(FocusMode)
    Q_ENUMS(FocusPointMode)
public:
    enum FocusMode {
        FocusManual = QCameraFocus::ManualFocus,
        FocusHyperfocal = QCameraFocus::HyperfocalFocus,
        FocusInfinity = QCameraFocus::InfinityFocus,
        FocusAuto = QCameraFocus::AutoFocus,
        FocusContinuous = QCameraFocus::ContinuousFocus,
        FocusMacro = QCameraFocus::MacroFocus
    };

    enum FocusPointMode {
        FocusPointAuto = QCameraFocus::FocusPointAuto,
        FocusPointCenter = QCameraFocus::FocusPointCenter,
        FocusPointFaceDetection = QCameraFocus::FocusPointFaceDetection,
        FocusPointCustom = QCameraFocus::FocusPointCustom
    };

    ~QDeclarativeCameraFocus();

    FocusMode focusMode() const;
    FocusPointMode focusPointMode() const;
    QPointF customFocusPoint() const;

    QAbstractListModel *focusZones() const;

    Q_INVOKABLE bool isFocusModeSupported(FocusMode mode) const;
    Q_INVOKABLE bool isFocusPointModeSupported(FocusPointMode mode) const;

public Q_SLOTS:
    void setFocusMode(FocusMode);
    void setFocusPointMode(FocusPointMode mode);
    void setCustomFocusPoint(const QPointF &point);

Q_SIGNALS:
    void focusModeChanged(FocusMode);
    void focusPointModeChanged(FocusPointMode);
    void customFocusPointChanged(const QPointF &);

private Q_SLOTS:
    void updateFocusZones();

private:
    friend class QDeclarativeCamera;
    QDeclarativeCameraFocus(QCamera *camera, QObject *parent = 0);

    QCameraFocus *m_focus;
    FocusZonesModel *m_focusZones;
};

class FocusZonesModel : public QAbstractListModel
{
Q_OBJECT
public:
    enum FocusZoneRoles {
        StatusRole = Qt::UserRole + 1,
        AreaRole
    };

    FocusZonesModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

public slots:
    void setFocusZones(const QCameraFocusZoneList &zones);

private:
    QCameraFocusZoneList m_focusZones;
};


QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeCameraFocus))

#endif
