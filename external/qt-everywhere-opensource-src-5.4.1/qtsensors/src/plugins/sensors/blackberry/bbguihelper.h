/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
#ifndef BBGUIHELPER_H
#define BBGUIHELPER_H

#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QObject>

// We can't depend on QtGui in this plugin, only on BPS.
// This class provides replacements for some QtGui functions, implemented using BPS.
class BbGuiHelper : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit BbGuiHelper(QObject *parent = 0);
    ~BbGuiHelper();

    // Orientation 0 is device held in normal position (Blackberry logo readable), then orientation
    // increases in 90 degrees steps counter-clockwise, i.e. rotating the device to the left.
    // Therefore the range is 0 to 270 degrees.
    int currentOrientation() const;

    bool applicationActive() const;

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;

signals:
    void orientationChanged();
    void applicationActiveChanged();

private:
    void readOrientation();
    void readApplicationActiveState();

    int m_currentOrientation;
    bool m_applicationActive;
};

#endif
