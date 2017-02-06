/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire (paul.lemire350@gmail.com)
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

#include "qeventfilterservice_p.h"
#include "qabstractserviceprovider_p.h"
#include <QMap>
#include <QObject>
#include <QVector>

QT_BEGIN_NAMESPACE

namespace {
    struct FilterPriorityPair
    {
        QObject *filter;
        int priority;
    };

    bool operator <(const FilterPriorityPair &a, const FilterPriorityPair &b)
    {
        return a.priority < b.priority;
    }
}

Q_DECLARE_TYPEINFO(FilterPriorityPair, Q_PRIMITIVE_TYPE);

namespace Qt3DCore {

namespace {
    class InternalEventListener;
}

class QEventFilterServicePrivate : public QAbstractServiceProviderPrivate
{
public:
    QEventFilterServicePrivate()
        : QAbstractServiceProviderPrivate(QServiceLocator::EventFilterService, QStringLiteral("Default event filter service implementation"))
    {}

    void registerEventFilter(QObject *eventFilter, int priority)
    {
        for (int i = 0, m = m_eventFilters.size(); i < m; ++i)
            if (m_eventFilters.at(i).priority == priority)
                return;

        FilterPriorityPair fpPair;
        fpPair.filter = eventFilter;
        fpPair.priority = priority;
        m_eventFilters.push_back(fpPair);
        std::sort(m_eventFilters.begin(), m_eventFilters.end());
    }

    void unregisterEventFilter(QObject *eventFilter)
    {
        QVector<FilterPriorityPair>::iterator it = m_eventFilters.begin();
        const QVector<FilterPriorityPair>::iterator end = m_eventFilters.end();
        while (it != end) {
            if (it->filter == eventFilter) {
                m_eventFilters.erase(it);
                return;
            }
            ++it;
        }
    }

    QScopedPointer<InternalEventListener> m_eventDispatcher;
    QVector<FilterPriorityPair> m_eventFilters;
};

namespace {

class InternalEventListener : public QObject
{
    Q_OBJECT
public:
    explicit InternalEventListener(QEventFilterServicePrivate *filterService, QObject *parent = nullptr)
        : QObject(parent)
        , m_filterService(filterService)
    {
    }

    bool eventFilter(QObject *obj, QEvent *e) Q_DECL_FINAL
    {
        for (int i = m_filterService->m_eventFilters.size() - 1; i >= 0; --i) {
            const FilterPriorityPair &fPPair = m_filterService->m_eventFilters.at(i);
            if (fPPair.filter->eventFilter(obj, e))
                return true;
        }
        return false;
    }

    QEventFilterServicePrivate* m_filterService;
};

} // anonymous


/* !\internal
    \class Qt3DCore::QEventFilterService
    \inmodule Qt3DCore

    \brief Allows to register event filters with a priority.

    The QEventFilterService class allows registering prioritized event filters.
    Events are dispatched to the event filter with the highest priority first,
    and then propagates to lower priority event filters if the event wasn't
    accepted.
 */

QEventFilterService::QEventFilterService()
    : QAbstractServiceProvider(*new QEventFilterServicePrivate())
{
}

QEventFilterService::~QEventFilterService()
{
}

// Note: event filters can only be registered to QObject in the same thread
void QEventFilterService::initialize(QObject *eventSource)
{
    Q_D(QEventFilterService);
    if (eventSource == nullptr) {
        d->m_eventDispatcher.reset();
    } else {
        d->m_eventDispatcher.reset(new InternalEventListener(d));
        eventSource->installEventFilter(d->m_eventDispatcher.data());
    }
}

void QEventFilterService::shutdown(QObject *eventSource)
{
    Q_D(QEventFilterService);
    if (eventSource && d->m_eventDispatcher.data())
        eventSource->removeEventFilter(d->m_eventDispatcher.data());
}

void QEventFilterService::registerEventFilter(QObject *eventFilter, int priority)
{
    Q_D(QEventFilterService);
    d->registerEventFilter(eventFilter, priority);
}

void QEventFilterService::unregisterEventFilter(QObject *eventFilter)
{
    Q_D(QEventFilterService);
    d->unregisterEventFilter(eventFilter);
}

} // Qt3DCore

QT_END_NAMESPACE

#include "qeventfilterservice.moc"
