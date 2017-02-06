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

#ifndef QSCXMLCPPDATAMODEL_H
#define QSCXMLCPPDATAMODEL_H

#include <QtScxml/qscxmldatamodel.h>

#define Q_SCXML_DATAMODEL \
    public: \
        QString evaluateToString(QScxmlExecutableContent::EvaluatorId id, bool *ok) Q_DECL_OVERRIDE Q_DECL_FINAL; \
        bool evaluateToBool(QScxmlExecutableContent::EvaluatorId id, bool *ok) Q_DECL_OVERRIDE Q_DECL_FINAL; \
        QVariant evaluateToVariant(QScxmlExecutableContent::EvaluatorId id, bool *ok) Q_DECL_OVERRIDE Q_DECL_FINAL; \
        void evaluateToVoid(QScxmlExecutableContent::EvaluatorId id, bool *ok) Q_DECL_OVERRIDE Q_DECL_FINAL; \
    private:

QT_BEGIN_NAMESPACE

class QScxmlCppDataModelPrivate;
class Q_SCXML_EXPORT QScxmlCppDataModel: public QScxmlDataModel
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QScxmlCppDataModel)
public:
    explicit QScxmlCppDataModel(QObject *parent = nullptr);

    Q_INVOKABLE bool setup(const QVariantMap &initialDataValues) Q_DECL_OVERRIDE;

    void evaluateAssignment(QScxmlExecutableContent::EvaluatorId id, bool *ok) Q_DECL_OVERRIDE;
    void evaluateInitialization(QScxmlExecutableContent::EvaluatorId id, bool *ok) Q_DECL_OVERRIDE;
    void evaluateForeach(QScxmlExecutableContent::EvaluatorId id, bool *ok, ForeachLoopBody *body) Q_DECL_OVERRIDE;

    void setScxmlEvent(const QScxmlEvent &scxmlEvent) Q_DECL_OVERRIDE Q_DECL_FINAL;
    const QScxmlEvent &scxmlEvent() const;

    QVariant scxmlProperty(const QString &name) const Q_DECL_OVERRIDE;
    bool hasScxmlProperty(const QString &name) const Q_DECL_OVERRIDE;
    bool setScxmlProperty(const QString &name, const QVariant &value, const QString &context) Q_DECL_OVERRIDE;

    bool inState(const QString &stateName) const;
};

QT_END_NAMESPACE

#endif // QSCXMLCPPDATAMODEL_H
