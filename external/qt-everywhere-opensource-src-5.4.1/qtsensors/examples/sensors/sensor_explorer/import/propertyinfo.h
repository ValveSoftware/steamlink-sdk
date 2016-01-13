/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QPROPERTYINFO_H
#define QPROPERTYINFO_H

#include <QtQml/QtQml>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE

class QPropertyInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString typeName READ typeName CONSTANT)
    Q_PROPERTY(QString value READ value NOTIFY valueChanged)
    Q_PROPERTY(bool isWriteable READ isWriteable CONSTANT)

public:
    QPropertyInfo(QObject* parent = 0);
    QPropertyInfo(const QString& name, int index, bool writeable, const QString& typeName, const QString& value, QObject* parent=0);
    QString name();
    QString typeName();
    QString value();
    void setValue(const QString& value);
    int index();
    bool isWriteable();

Q_SIGNALS:
    void valueChanged();

private:
    int _index;
    bool _isWriteable;
    QString _name;
    QString _typeName;
    QString _value;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QPropertyInfo)

#endif // QPROPERTYINFO_H
