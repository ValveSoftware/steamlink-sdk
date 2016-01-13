/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef WLDATADEVICEMANAGER_H
#define WLDATADEVICEMANAGER_H

#include <private/qwlcompositor_p.h>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtGui/QClipboard>
#include <QtCore/QMimeData>

#include <QtCompositor/private/qwayland-server-wayland.h>

QT_BEGIN_NAMESPACE

class QSocketNotifier;

namespace QtWayland {

class Compositor;

class DataDevice;
class DataSource;

class DataDeviceManager : public QObject, public QtWaylandServer::wl_data_device_manager
{
    Q_OBJECT

public:
    DataDeviceManager(Compositor *compositor);

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

private slots:
    void readFromClient(int fd);

private:
    void retain();
    void finishReadFromClient(bool exhausted = false);

    Compositor *m_compositor;
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
