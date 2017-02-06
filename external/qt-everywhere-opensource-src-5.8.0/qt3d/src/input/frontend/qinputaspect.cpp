/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qinputaspect.h"
#include "qinputaspect_p.h"
#include "inputhandler_p.h"
#include "buttonaxisinput_p.h"
#include "keyboarddevice_p.h"
#include "keyboardhandler_p.h"
#include "mousedevice_p.h"
#include "mousehandler_p.h"
#include <Qt3DInput/qkeyboarddevice.h>
#include <Qt3DInput/qkeyboardhandler.h>
#include <Qt3DInput/qmousedevice.h>
#include <Qt3DInput/qmousehandler.h>
#include <Qt3DInput/private/qinputdeviceintegration_p.h>
#include <Qt3DInput/qinputsettings.h>
#include <Qt3DInput/private/qgenericinputdevice_p.h>
#include <Qt3DInput/private/qinputdeviceintegrationfactory_p.h>
#include <Qt3DCore/private/qservicelocator_p.h>
#include <Qt3DCore/private/qeventfilterservice_p.h>
#include <QDir>
#include <QLibrary>
#include <QLibraryInfo>
#include <QPluginLoader>

#include <Qt3DInput/qaxis.h>
#include <Qt3DInput/qaxisaccumulator.h>
#include <Qt3DInput/qaction.h>
#include <Qt3DInput/qaxissetting.h>
#include <Qt3DInput/qactioninput.h>
#include <Qt3DInput/qanalogaxisinput.h>
#include <Qt3DInput/qbuttonaxisinput.h>
#include <Qt3DInput/qinputchord.h>
#include <Qt3DInput/qinputsequence.h>
#include <Qt3DInput/qlogicaldevice.h>
#include <Qt3DInput/qabstractphysicaldevice.h>
#include <Qt3DInput/private/qabstractphysicaldeviceproxy_p.h>
#include <Qt3DInput/private/axis_p.h>
#include <Qt3DInput/private/axisaccumulator_p.h>
#include <Qt3DInput/private/action_p.h>
#include <Qt3DInput/private/axissetting_p.h>
#include <Qt3DInput/private/actioninput_p.h>
#include <Qt3DInput/private/inputchord_p.h>
#include <Qt3DInput/private/inputsequence_p.h>
#include <Qt3DInput/private/logicaldevice_p.h>
#include <Qt3DInput/private/inputbackendnodefunctor_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/private/updateaxisactionjob_p.h>
#include <Qt3DInput/private/keyboardmousegenericdeviceintegration_p.h>
#include <Qt3DInput/private/genericdevicebackendnode_p.h>
#include <Qt3DInput/private/inputsettings_p.h>
#include <Qt3DInput/private/eventsourcesetterhelper_p.h>
#include <Qt3DInput/private/loadproxydevicejob_p.h>
#include <Qt3DInput/private/axisaccumulatorjob_p.h>

#ifdef HAVE_QGAMEPAD
# include <Qt3DInput/private/qgamepadinput_p.h>
#endif

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DInput {

QInputAspectPrivate::QInputAspectPrivate()
    : QAbstractAspectPrivate()
    , m_inputHandler(new Input::InputHandler())
    , m_keyboardMouseIntegration(new Input::KeyboardMouseGenericDeviceIntegration(m_inputHandler.data()))
    , m_time(0)
{
}

/*!
    \class Qt3DInput::QInputAspect
    \inherits Qt3DCore::QAbstractAspect
    \inmodule Qt3DInput
    \brief Responsible for creating physical devices and handling associated jobs.
    \since 5.5
    \brief Handles mapping between front and backend nodes

    QInputAspect is responsible for creating physical devices.
    It is also the object responsible establishing the jobs to run at a particular time from the current input setup.
*/

/*!
 * Constructs a new QInputAspect with \a parent.
 */
QInputAspect::QInputAspect(QObject *parent)
    : QInputAspect(*new QInputAspectPrivate, parent)
{
}

/*! \internal */
QInputAspect::QInputAspect(QInputAspectPrivate &dd, QObject *parent)
    : QAbstractAspect(dd, parent)
{
    setObjectName(QStringLiteral("Input Aspect"));

    qRegisterMetaType<Qt3DInput::QAbstractPhysicalDevice*>();

    registerBackendType<QKeyboardDevice>(QBackendNodeMapperPtr(new Input::KeyboardDeviceFunctor(this, d_func()->m_inputHandler.data())));
    registerBackendType<QKeyboardHandler>(QBackendNodeMapperPtr(new Input::KeyboardHandlerFunctor(d_func()->m_inputHandler.data())));
    registerBackendType<QMouseDevice>(QBackendNodeMapperPtr(new Input::MouseDeviceFunctor(this, d_func()->m_inputHandler.data())));
    registerBackendType<QMouseHandler>(QBackendNodeMapperPtr(new Input::MouseHandlerFunctor(d_func()->m_inputHandler.data())));
    registerBackendType<QAxis>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::Axis, Input::AxisManager>(d_func()->m_inputHandler->axisManager())));
    registerBackendType<QAxisAccumulator>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::AxisAccumulator, Input::AxisAccumulatorManager>(d_func()->m_inputHandler->axisAccumulatorManager())));
    registerBackendType<QAnalogAxisInput>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::AnalogAxisInput, Input::AnalogAxisInputManager>(d_func()->m_inputHandler->analogAxisInputManager())));
    registerBackendType<QButtonAxisInput>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::ButtonAxisInput, Input::ButtonAxisInputManager>(d_func()->m_inputHandler->buttonAxisInputManager())));
    registerBackendType<QAxisSetting>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::AxisSetting, Input::AxisSettingManager>(d_func()->m_inputHandler->axisSettingManager())));
    registerBackendType<Qt3DInput::QAction>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::Action, Input::ActionManager>(d_func()->m_inputHandler->actionManager())));
    registerBackendType<QActionInput>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::ActionInput, Input::ActionInputManager>(d_func()->m_inputHandler->actionInputManager())));
    registerBackendType<QInputChord>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::InputChord, Input::InputChordManager>(d_func()->m_inputHandler->inputChordManager())));
    registerBackendType<QInputSequence>(QBackendNodeMapperPtr(new Input::InputNodeFunctor<Input::InputSequence, Input::InputSequenceManager>(d_func()->m_inputHandler->inputSequenceManager())));
    registerBackendType<QLogicalDevice>(QBackendNodeMapperPtr(new Input::LogicalDeviceNodeFunctor(d_func()->m_inputHandler->logicalDeviceManager())));
    registerBackendType<QGenericInputDevice>(QBackendNodeMapperPtr(new Input::GenericDeviceBackendFunctor(this, d_func()->m_inputHandler.data())));
    registerBackendType<QInputSettings>(QBackendNodeMapperPtr(new Input::InputSettingsFunctor(d_func()->m_inputHandler.data())));
    registerBackendType<QAbstractPhysicalDeviceProxy>(QBackendNodeMapperPtr(new Input::PhysicalDeviceProxyNodeFunctor(d_func()->m_inputHandler->physicalDeviceProxyManager())));

#ifdef HAVE_QGAMEPAD
    registerBackendType<QGamepadInput>(QBackendNodeMapperPtr(new Input::GenericDeviceBackendFunctor(this, d_func()->m_inputHandler.data())));
#endif

    Q_D(QInputAspect);
    // Plugins are QInputDeviceIntegration instances
    d->loadInputDevicePlugins();

    // KeyboardDevice and MouseDevice also provide their own QInputDeviceIntegration
    d->m_inputHandler->addInputDeviceIntegration(d->m_keyboardMouseIntegration.data());
}

/*! \internal */
QInputAspect::~QInputAspect()
{
}

/*!
   Create each of the detected input device integrations through the Integration Factory
 */
void QInputAspectPrivate::loadInputDevicePlugins()
{
    const QStringList keys = QInputDeviceIntegrationFactory::keys();
    for (const QString &key : keys) {
        Qt3DInput::QInputDeviceIntegration *integration = QInputDeviceIntegrationFactory::create(key, QStringList());
        if (integration != nullptr) {
            m_inputHandler->addInputDeviceIntegration(integration);
            // Initialize will allow the InputDeviceIntegration to
            // register their frontend / backend types,
            // create their managers
            // launch a thread to listen to the actual physical device....
            integration->initialize(q_func());
        }
    }
}

/*!
    Create a physical device identified by \a name using the input device integrations present
    returns a Q_NULLPTR if it is not found.
    \note caller is responsible for ownership
*/
// Note: caller is responsible for ownership
QAbstractPhysicalDevice *QInputAspect::createPhysicalDevice(const QString &name)
{
    Q_D(QInputAspect);
    return d->m_inputHandler->createPhysicalDevice(name);
}

/*!
    \return a list of all available physical devices.
 */
QStringList QInputAspect::availablePhysicalDevices() const
{
    Q_D(const QInputAspect);
    QStringList deviceNamesList;
    const auto deviceIntegrations = d->m_inputHandler->inputDeviceIntegrations();
    for (const QInputDeviceIntegration *integration : deviceIntegrations)
        deviceNamesList += integration->deviceNames();
    return deviceNamesList;
}

/*!
    \internal
 */
QVector<QAspectJobPtr> QInputAspect::jobsToExecute(qint64 time)
{
    Q_D(QInputAspect);
    const qint64 deltaTime = time - d->m_time;
    const float dt = static_cast<const float>(deltaTime) / 1.0e9;
    d->m_time = time;

    QVector<QAspectJobPtr> jobs;

    d->m_inputHandler->updateEventSource();

    jobs.append(d->m_inputHandler->keyboardJobs());
    jobs.append(d->m_inputHandler->mouseJobs());

    const auto integrations = d->m_inputHandler->inputDeviceIntegrations();
    for (QInputDeviceIntegration *integration : integrations)
        jobs += integration->jobsToExecute(time);

    const QVector<Qt3DCore::QNodeId> proxiesToLoad = d->m_inputHandler->physicalDeviceProxyManager()->takePendingProxiesToLoad();
    if (!proxiesToLoad.isEmpty()) {
        // Since loading wrappers occurs quite rarely, no point in keeping the job in a
        // member variable
        auto loadWrappersJob = Input::LoadProxyDeviceJobPtr::create();
        loadWrappersJob->setProxiesToLoad(std::move(proxiesToLoad));
        loadWrappersJob->setInputHandler(d->m_inputHandler.data());
        jobs.push_back(loadWrappersJob);
    }

    // All the jobs added up until this point are independents
    // but the axis action jobs will be dependent on these
    const QVector<QAspectJobPtr> dependsOnJobs = jobs;

    // Jobs that update Axis/Action (store combined axis/action value)
    const auto devHandles = d->m_inputHandler->logicalDeviceManager()->activeDevices();
    QVector<QAspectJobPtr> axisActionJobs;
    axisActionJobs.reserve(devHandles.size());
    for (Input::HLogicalDevice devHandle : devHandles) {
        const auto device = d->m_inputHandler->logicalDeviceManager()->data(devHandle);
        if (!device->isEnabled())
            continue;

        QAspectJobPtr updateAxisActionJob(new Input::UpdateAxisActionJob(time, d->m_inputHandler.data(), devHandle));
        jobs += updateAxisActionJob;
        axisActionJobs.push_back(updateAxisActionJob);
        for (const QAspectJobPtr &job : dependsOnJobs)
            updateAxisActionJob->addDependency(job);
    }

    // Once all the axes have been updated we can step the integrations on
    // the AxisAccumulators
    auto accumulateJob = Input::AxisAccumulatorJobPtr::create(d->m_inputHandler->axisAccumulatorManager(),
                                                              d->m_inputHandler->axisManager());
    accumulateJob->setDeltaTime(dt);
    for (const QAspectJobPtr &job : qAsConst(axisActionJobs))
        accumulateJob->addDependency(job);
    jobs.push_back(accumulateJob);

    return jobs;
}

/*!
    \internal
 */
void QInputAspect::onRegistered()
{
    Q_D(QInputAspect);
    Qt3DCore::QEventFilterService *eventService = d->services()->eventFilterService();
    Q_ASSERT(eventService);

    // Set it on the input handler which will also handle its lifetime
    d->m_inputHandler->eventSourceHelper()->setEventFilterService(eventService);
}

/*!
    \internal
 */
void QInputAspect::onUnregistered()
{
    Q_D(QInputAspect);
    // At this point it is too late to call removeEventFilter as the eventSource (Window)
    // may already be destroyed
    d->m_inputHandler.reset(nullptr);
}

} // namespace Qt3DInput

QT_END_NAMESPACE

QT3D_REGISTER_NAMESPACED_ASPECT("input", QT_PREPEND_NAMESPACE(Qt3DInput), QInputAspect)
