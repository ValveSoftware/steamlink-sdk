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

#include "qscxmldatamodel_p.h"
#include "qscxmlnulldatamodel.h"
#include "qscxmlecmascriptdatamodel.h"
#include "qscxmlstatemachine_p.h"

QT_BEGIN_NAMESPACE

/*!
  \class QScxmlDataModel::ForeachLoopBody
  \brief The ForeachLoopBody class represents a function to be executed on
  each iteration of an SCXML foreach loop.
  \since 5.8
  \inmodule QtScxml
 */

/*!
  Destroys a foreach loop body.
 */
QScxmlDataModel::ForeachLoopBody::~ForeachLoopBody()
{}

/*!
  \fn QScxmlDataModel::ForeachLoopBody::run(bool *ok)

  This function is executed on each iteration. If the execution fails, \a ok is
  set to \c false, otherwise it is set to \c true.
 */

/*!
 * \class QScxmlDataModel
 * \brief The QScxmlDataModel class is the data model base class for a Qt SCXML
 * state machine.
 * \since 5.7
 * \inmodule QtScxml
 *
 * SCXML data models are described in
 * \l {SCXML Specification - 5 Data Model and Data Manipulation}. For more
 * information about supported data models, see \l {SCXML Compliance}.
 *
 * One data model can only belong to one state machine.
 *
 * \sa QScxmlStateMachine QScxmlCppDataModel QScxmlEcmaScriptDataModel QScxmlNullDataModel
 */

/*!
  \property QScxmlDataModel::stateMachine

  \brief The state machine this data model belongs to.

  A data model can only belong to a single state machine and a state machine
  can only have one data model. This relation needs to be set up before the
  state machine is started. Setting this property on a data model will
  automatically set the corresponding \c dataModel property on the
  \a stateMachine.
*/

/*!
 * Creates a new data model, with the parent object \a parent.
 */
QScxmlDataModel::QScxmlDataModel(QObject *parent)
    : QObject(*(new QScxmlDataModelPrivate), parent)
{
}

/*!
  Creates a new data model from the private object \a dd, with the parent
  object \a parent.
 */
QScxmlDataModel::QScxmlDataModel(QScxmlDataModelPrivate &dd, QObject *parent) :
    QObject(dd, parent)
{
}

/*!
 * Sets the state machine this model belongs to to \a stateMachine. There is a
 * 1:1 relation between state machines and models. After setting the state
 * machine once you cannot change it anymore. Any further attempts to set the
 * state machine using this method will be ignored.
 */
void QScxmlDataModel::setStateMachine(QScxmlStateMachine *stateMachine)
{
    Q_D(QScxmlDataModel);

    if (d->m_stateMachine == Q_NULLPTR && stateMachine != Q_NULLPTR) {
        d->m_stateMachine = stateMachine;
        if (stateMachine)
            stateMachine->setDataModel(this);
        emit stateMachineChanged(stateMachine);
    }
}

/*!
 * Returns the state machine associated with the data model.
 */
QScxmlStateMachine *QScxmlDataModel::stateMachine() const
{
    Q_D(const QScxmlDataModel);
    return d->m_stateMachine;
}

QScxmlDataModel *QScxmlDataModelPrivate::instantiateDataModel(DocumentModel::Scxml::DataModelType type)
{
    QScxmlDataModel *dataModel = Q_NULLPTR;
    switch (type) {
    case DocumentModel::Scxml::NullDataModel:
        dataModel = new QScxmlNullDataModel;
        break;
    case DocumentModel::Scxml::JSDataModel:
        dataModel = new QScxmlEcmaScriptDataModel;
        break;
    case DocumentModel::Scxml::CppDataModel:
        break;
    default:
        Q_UNREACHABLE();
    }

    return dataModel;
}

/*!
 * \fn QScxmlDataModel::setup(const QVariantMap &initialDataValues)
 *
 * Initializes the data model with the initial values specified by
 * \a initialDataValues.
 *
 * Returns \c false if parse errors occur or if any of the initialization steps
 * fail. Returns \c true otherwise.
 */

/*!
 * \fn QScxmlDataModel::setScxmlEvent(const QScxmlEvent &event)
 *
 * Sets the \a event to use in the subsequent executable content execution.
 */

/*!
 * \fn QScxmlDataModel::scxmlProperty(const QString &name) const
 *
 * Returns the value of the property \a name.
 */

/*!
 * \fn QScxmlDataModel::hasScxmlProperty(const QString &name) const
 *
 * Returns \c true if a property with the given \a name exists, \c false
 * otherwise.
 */

/*!
 * \fn QScxmlDataModel::setScxmlProperty(const QString &name,
 *                                       const QVariant &value,
 *                                       const QString &context)
 *
 * Sets a the value \a value for the property \a name.
 *
 * The \a context is a string that is used in error messages to indicate the
 * location in the SCXML file where the error occurred.
 *
 * Returns \c true if successful or \c false if an error occurred.
 */

/*!
 * \fn QScxmlDataModel::evaluateToString(
 *           QScxmlExecutableContent::EvaluatorId id, bool *ok)
 * Evaluates the executable content pointed to by \a id and sets \a ok to
 * \c false if there was an error or to \c true if there was not.
 * Returns the result of the evaluation as a QString.
 */

/*!
 * \fn QScxmlDataModel::evaluateToBool(QScxmlExecutableContent::EvaluatorId id,
 *                                     bool *ok)
 * Evaluates the executable content pointed to by \a id and sets \a ok to
 * \c false if there was an error or to \c true if there was not.
 * Returns the result of the evaluation as a boolean value.
 */

/*!
 * \fn QScxmlDataModel::evaluateToVariant(
 *           QScxmlExecutableContent::EvaluatorId id, bool *ok)
 * Evaluates the executable content pointed to by \a id and sets \a ok to
 * \c false if there was an error or to \c true if there was not.
 * Returns the result of the evaluation as a QVariant.
 */

/*!
 * \fn QScxmlDataModel::evaluateToVoid(QScxmlExecutableContent::EvaluatorId id,
 *                                     bool *ok)
 * Evaluates the executable content pointed to by \a id and sets \a ok to
 * \c false if there was an error or to \c true if there was not.
 * The execution is expected to return no result.
 */

/*!
 * \fn QScxmlDataModel::evaluateAssignment(
 *           QScxmlExecutableContent::EvaluatorId id, bool *ok)
 * Evaluates the assignment pointed to by \a id and sets \a ok to
 * \c false if there was an error or to \c true if there was not.
 */

/*!
 * \fn QScxmlDataModel::evaluateInitialization(
 *           QScxmlExecutableContent::EvaluatorId id, bool *ok)
 * Evaluates the initialization pointed to by \a id and sets \a ok to
 * \c false if there was an error or to \c true if there was not.
 */

/*!
 * \fn QScxmlDataModel::evaluateForeach(
 *           QScxmlExecutableContent::EvaluatorId id, bool *ok,
 *           ForeachLoopBody *body)
 * Evaluates the foreach loop pointed to by \a id and sets \a ok to
 * \c false if there was an error or to \c true if there was not. The
 * \a body is executed on each iteration.
 */

QT_END_NAMESPACE
