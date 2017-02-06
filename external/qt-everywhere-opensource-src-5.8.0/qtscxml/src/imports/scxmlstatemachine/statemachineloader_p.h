/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#ifndef STATEMACHINELOADER_P_H
#define STATEMACHINELOADER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QUrl>
#include <QtScxml/qscxmlstatemachine.h>
#include <private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

class QScxmlStateMachineLoader: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QScxmlStateMachine* stateMachine READ stateMachine DESIGNABLE false NOTIFY stateMachineChanged)
    Q_PROPERTY(QVariantMap initialValues READ initialValues WRITE setInitialValues NOTIFY initialValuesChanged)
    Q_PROPERTY(QScxmlDataModel* dataModel READ dataModel WRITE setDataModel NOTIFY dataModelChanged)


public:
    explicit QScxmlStateMachineLoader(QObject *parent = 0);

    QScxmlStateMachine *stateMachine() const;

    QUrl source();
    void setSource(const QUrl &source);

    QVariantMap initialValues() const;
    void setInitialValues(const QVariantMap &initialValues);

    QScxmlDataModel *dataModel() const;
    void setDataModel(QScxmlDataModel *dataModel);

Q_SIGNALS:
    void sourceChanged();
    void initialValuesChanged();
    void stateMachineChanged();
    void dataModelChanged();

private:
    bool parse(const QUrl &source);

private:
    QUrl m_source;
    QVariantMap m_initialValues;
    QScxmlDataModel *m_dataModel;
    QScxmlDataModel *m_implicitDataModel;
    QScxmlStateMachine *m_stateMachine;
};

QT_END_NAMESPACE

#endif // STATEMACHINELOADER_P_H
