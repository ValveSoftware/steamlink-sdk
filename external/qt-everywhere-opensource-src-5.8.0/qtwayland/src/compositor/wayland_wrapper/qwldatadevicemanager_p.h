/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef WLDATADEVICEMANAGER_H
#define WLDATADEVICEMANAGER_H

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

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtGui/QClipboard>
#include <QtCore/QMimeData>

#include <QtWaylandCompositor/QWaylandCompositor>

#include <QtWaylandCompositor/private/qwayland-server-wayland.h>

QT_BEGIN_NAMESPACE

class QSocketNotifier;

namespace QtWayland {

class DataDevice;
class DataSource;

class DataDeviceManager : public QObject, public QtWaylandServer::wl_data_device_manager
{
    Q_OBJECT

public:
    DataDeviceManager(QWaylandCompositor *compositor);

    void setCurrentSelectionSource(DataSource *source);
    DataSource *currentSelectionSource();

    struct wl_display *display() const;

    void sourceDestroyed(DataSource *source);

    void overrideSelection(const QMimeData &mimeData);
    bool offerFromCompositorToClient(wl_resource *clientDataDeviceResource);
    void offerRetainedSelection(wl_resource *clientDataDeviceResource);

protected:
    void data_device_manager_create_data_source(Resource *resource, uint32_t id) Q_DECL_OVERRIDE;
    void data_device_manager_get_data_device(Resource *resource, uint32_t id, struct ::wl_resource *seat) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void readFromClient(int fd);

private:
    void retain();
    void finishReadFromClient(bool exhausted = false);

    QWaylandCompositor *m_compositor;
    QList<DataDevice *> m_data_device_list;

    DataSource *m_current_selection_source;

    QMimeData m_retainedData;
    QSocketNotifier *m_retainedReadNotifier;
    QList<QSocketNotifier *> m_obsoleteRetainedReadNotifiers;
    int m_retainedReadIndex;
    QByteArray m_retainedReadBuf;

    bool m_compositorOwnsSelection;


    static void comp_accept(struct wl_client *client,
                            struct wl_resource *resource,
                            uint32_t time,
                            const char *type);
    static void comp_receive(struct wl_client *client,
                             struct wl_resource *resource,
                             const char *mime_type,
                             int32_t fd);
    static void comp_destroy(struct wl_client *client,
                             struct wl_resource *resource);

    static const struct wl_data_offer_interface compositor_offer_interface;
};

}

QT_END_NAMESPACE

#endif // WLDATADEVICEMANAGER_H
