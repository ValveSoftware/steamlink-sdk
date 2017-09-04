/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qdeclarativegeomap_p.h"
#include "qdeclarativegeomapquickitem_p.h"
#include "qdeclarativegeomapcopyrightsnotice_p.h"
#include "qdeclarativegeoserviceprovider_p.h"
#include "qdeclarativegeomaptype_p.h"
#include "qgeomappingmanager_p.h"
#include "qgeocameracapabilities_p.h"
#include "qgeomap_p.h"
#include "qdeclarativegeomapparameter_p.h"
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoPath>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRectangleNode>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQml/qqmlinfo.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.141592653589793238463
#endif


QT_BEGIN_NAMESPACE

/*!
    \qmltype Map
    \instantiates QDeclarativeGeoMap
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.0

    \brief The Map type displays a map.

    The Map type is used to display a map or image of the Earth, with
    the capability to also display interactive objects tied to the map's
    surface.

    There are a variety of different ways to visualize the Earth's surface
    in a 2-dimensional manner, but all of them involve some kind of projection:
    a mathematical relationship between the 3D coordinates (latitude, longitude
    and altitude) and 2D coordinates (X and Y in pixels) on the screen.

    Different sources of map data can use different projections, and from the
    point of view of the Map type, we treat these as one replaceable unit:
    the Map plugin. A Map plugin consists of a data source, as well as all other
    details needed to display its data on-screen.

    The current Map plugin in use is contained in the \l plugin property of
    the Map item. In order to display any image in a Map item, you will need
    to set this property. See the \l Plugin type for a description of how
    to retrieve an appropriate plugin for use.

    The geographic region displayed in the Map item is referred to as its
    viewport, and this is defined by the properties \l center, and
    \l zoomLevel. The \l center property contains a \l {coordinate}
    specifying the center of the viewport, while \l zoomLevel controls the scale of the
    map. See each of these properties for further details about their values.

    When the map is displayed, each possible geographic coordinate that is
    visible will map to some pixel X and Y coordinate on the screen. To perform
    conversions between these two, Map provides the \l toCoordinate and
    \l fromCoordinate functions, which are of general utility.

    \section2 Map Objects

    Map related objects can be declared within the body of a Map object in Qt Quick and will
    automatically appear on the Map. To add objects programmatically, first be
    sure they are created with the Map as their parent (for example in an argument to
    Component::createObject), and then call the \l addMapItem method on the Map.
    A corresponding \l removeMapItem method also exists to do the opposite and
    remove an object from the Map.

    Moving Map objects around, resizing them or changing their shape normally
    does not involve any special interaction with Map itself -- changing these
    details about a map object will automatically update the display.

    \section2 Interaction

    The Map type includes support for pinch and flick gestures to control
    zooming and panning. These are enabled by default, and available at any
    time by using the \l gesture object. The actual GestureArea is constructed
    specially at startup and cannot be replaced or destroyed. Its properties
    can be altered, however, to control its behavior.

    \section2 Performance

    Maps are rendered using OpenGL (ES) and the Qt Scene Graph stack, and as
    a result perform quite well where GL accelerated hardware is available.

    For "online" Map plugins, network bandwidth and latency can be major
    contributors to the user's perception of performance. Extensive caching is
    performed to mitigate this, but such mitigation is not always perfect. For
    "offline" plugins, the time spent retrieving the stored geographic data
    and rendering the basic map features can often play a dominant role. Some
    offline plugins may use hardware acceleration themselves to (partially)
    avert this.

    In general, large and complex Map items such as polygons and polylines with
    large numbers of vertices can have an adverse effect on UI performance.
    Further, more detailed notes on this are in the documentation for each
    map item type.

    \section2 Example Usage

    The following snippet shows a simple Map and the necessary Plugin type
    to use it. The map is centered over Oslo, Norway, with zoom level 14.

    \quotefromfile minimal_map/main.qml
    \skipto import
    \printuntil }
    \printline }
    \skipto Map
    \printuntil }
    \printline }

    \image minimal_map.png
*/

/*!
    \qmlsignal QtLocation::Map::copyrightLinkActivated(string link)

    This signal is emitted when the user clicks on a \a link in the copyright notice. The
    application should open the link in a browser or display its contents to the user.
*/

QDeclarativeGeoMap::QDeclarativeGeoMap(QQuickItem *parent)
        : QQuickItem(parent),
        m_plugin(0),
        m_mappingManager(0),
        m_activeMapType(0),
        m_gestureArea(new QQuickGeoMapGestureArea(this)),
        m_map(0),
        m_error(QGeoServiceProvider::NoError),
        m_color(QColor::fromRgbF(0.9, 0.9, 0.9)),
        m_componentCompleted(false),
        m_pendingFitViewport(false),
        m_copyrightsVisible(true),
        m_maximumViewportLatitude(0.0),
        m_initialized(false),
        m_userMinimumZoomLevel(qQNaN()),
        m_userMaximumZoomLevel(qQNaN()),
        m_userMinimumTilt(qQNaN()),
        m_userMaximumTilt(qQNaN()),
        m_userMinimumFieldOfView(qQNaN()),
        m_userMaximumFieldOfView(qQNaN())
{
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlags(QQuickItem::ItemHasContents | QQuickItem::ItemClipsChildrenToShape);
    setFiltersChildMouseEvents(true);

    connect(this, SIGNAL(childrenChanged()), this, SLOT(onMapChildrenChanged()), Qt::QueuedConnection);

    m_activeMapType = new QDeclarativeGeoMapType(QGeoMapType(QGeoMapType::NoMap,
                                                             tr("No Map"),
                                                             tr("No Map"), false, false, 0, QByteArrayLiteral("")), this);
    m_cameraData.setCenter(QGeoCoordinate(51.5073,-0.1277)); //London city center
    m_cameraData.setZoomLevel(8.0);

    m_cameraCapabilities.setTileSize(256);
    m_cameraCapabilities.setSupportsBearing(true);
    m_cameraCapabilities.setSupportsTilting(true);
    m_cameraCapabilities.setMinimumZoomLevel(0);
    m_cameraCapabilities.setMaximumZoomLevel(30);
    m_cameraCapabilities.setMinimumTilt(0);
    m_cameraCapabilities.setMaximumTilt(89.5);
    m_cameraCapabilities.setMinimumFieldOfView(1);
    m_cameraCapabilities.setMaximumFieldOfView(179);

    m_minimumTilt = m_cameraCapabilities.minimumTilt();
    m_maximumTilt = m_cameraCapabilities.maximumTilt();
    m_minimumFieldOfView = m_cameraCapabilities.minimumFieldOfView();
    m_maximumFieldOfView = m_cameraCapabilities.maximumFieldOfView();
}

QDeclarativeGeoMap::~QDeclarativeGeoMap()
{
    // Removing map parameters and map items from m_map
    if (m_map) {
        m_map->clearParameters();
        m_map->clearMapItems();
    }

    if (!m_mapViews.isEmpty())
        qDeleteAll(m_mapViews);
    // remove any map items associations
    for (int i = 0; i < m_mapItems.count(); ++i) {
        if (m_mapItems.at(i))
            m_mapItems.at(i).data()->setMap(0,0);
    }
    m_mapItems.clear();

    for (auto g: qAsConst(m_mapItemGroups)) {
        if (!g)
            continue;
        const QList<QQuickItem *> quickKids = g->childItems();
        for (auto c: quickKids) {
            QDeclarativeGeoMapItemBase *itemBase = qobject_cast<QDeclarativeGeoMapItemBase *>(c);
            if (itemBase)
                itemBase->setMap(0,0);
        }
    }
    m_mapItemGroups.clear();

    delete m_copyrights.data();
    m_copyrights.clear();
}

/*!
    \internal
*/
void QDeclarativeGeoMap::onMapChildrenChanged()
{
    if (!m_componentCompleted || !m_map)
        return;

    int maxChildZ = 0;
    QObjectList kids = children();
    bool foundCopyrights = false;

    for (int i = 0; i < kids.size(); ++i) {
        QDeclarativeGeoMapCopyrightNotice *copyrights = qobject_cast<QDeclarativeGeoMapCopyrightNotice *>(kids.at(i));
        if (copyrights) {
            foundCopyrights = true;
        } else {
            QDeclarativeGeoMapItemBase *mapItem = qobject_cast<QDeclarativeGeoMapItemBase *>(kids.at(i));
            if (mapItem) {
                if (mapItem->z() > maxChildZ)
                    maxChildZ = mapItem->z();
            }
        }
    }

    QDeclarativeGeoMapCopyrightNotice *copyrights = m_copyrights.data();
    // if copyrights object not found within the map's children
    if (!foundCopyrights) {
        // if copyrights object was deleted!
        if (!copyrights) {
            // create a new one and set its parent, re-assign it to the weak pointer, then connect the copyrights-change signal
            m_copyrights = new QDeclarativeGeoMapCopyrightNotice(this);
            m_copyrights->onCopyrightsStyleSheetChanged(m_map->copyrightsStyleSheet());

            copyrights = m_copyrights.data();

            connect(m_map.data(), SIGNAL(copyrightsChanged(QImage)),
                    copyrights, SLOT(copyrightsChanged(QImage)));
            connect(m_map.data(), SIGNAL(copyrightsChanged(QImage)),
                    this,  SIGNAL(copyrightsChanged(QImage)));

            connect(m_map.data(), SIGNAL(copyrightsChanged(QString)),
                    copyrights, SLOT(copyrightsChanged(QString)));
            connect(m_map.data(), SIGNAL(copyrightsChanged(QString)),
                    this,  SIGNAL(copyrightsChanged(QString)));

            connect(m_map.data(), SIGNAL(copyrightsStyleSheetChanged(QString)),
                    copyrights, SLOT(onCopyrightsStyleSheetChanged(QString)));

            connect(copyrights, SIGNAL(linkActivated(QString)),
                    this, SIGNAL(copyrightLinkActivated(QString)));

            // set visibility of copyright notice
            copyrights->setCopyrightsVisible(m_copyrightsVisible);

        } else {
            // just re-set its parent.
            copyrights->setParent(this);
        }
    }

    // put the copyrights notice object at the highest z order
    copyrights->setCopyrightsZ(maxChildZ + 1);
}

static QDeclarativeGeoMapType *findMapType(const QList<QDeclarativeGeoMapType *> &types, const QGeoMapType &type)
{
    for (int i = 0; i < types.size(); ++i)
        if (types[i]->mapType() == type)
            return types[i];
    return Q_NULLPTR;
}

void QDeclarativeGeoMap::onSupportedMapTypesChanged()
{
    QList<QDeclarativeGeoMapType *> supportedMapTypes;
    QList<QGeoMapType> types = m_mappingManager->supportedMapTypes();
    for (int i = 0; i < types.size(); ++i) {
        // types that are present and get removed will be deleted at QObject destruction
        QDeclarativeGeoMapType *type = findMapType(m_supportedMapTypes, types[i]);
        if (!type)
            type = new QDeclarativeGeoMapType(types[i], this);
        supportedMapTypes.append(type);
    }
    m_supportedMapTypes.swap(supportedMapTypes);
    if (m_supportedMapTypes.isEmpty()) {
        m_map->setActiveMapType(QGeoMapType()); // no supported map types: setting an invalid one
    } else {
        bool hasMapType = false;
        foreach (QDeclarativeGeoMapType *declarativeType, m_supportedMapTypes) {
            if (declarativeType->mapType() == m_map->activeMapType())
                hasMapType = true;
        }
        if (!hasMapType) {
            QDeclarativeGeoMapType *type = m_supportedMapTypes.at(0);
            m_activeMapType = type;
            m_map->setActiveMapType(type->mapType());
        }
    }

    emit supportedMapTypesChanged();
}

void QDeclarativeGeoMap::setError(QGeoServiceProvider::Error error, const QString &errorString)
{
    if (m_error == error && m_errorString == errorString)
        return;
    m_error = error;
    m_errorString = errorString;
    emit errorChanged();
}

/*!
    \internal
    Called when the mapping manager is initialized AND the declarative element has a valid size > 0
*/
void QDeclarativeGeoMap::initialize()
{
    // try to keep change signals in the end
    bool centerHasChanged = false;
    bool bearingHasChanged = false;
    bool tiltHasChanged = false;
    bool fovHasChanged = false;

    QGeoCoordinate center = m_cameraData.center();

    if (qIsNaN(m_userMinimumZoomLevel))
        setMinimumZoomLevel(m_map->minimumZoom(), false);
    else
        setMinimumZoomLevel(qMax<qreal>(m_map->minimumZoom(), m_userMinimumZoomLevel), false);

    double bearing = m_cameraData.bearing();
    double tilt = m_cameraData.tilt();
    double fov = m_cameraData.fieldOfView(); // Must be 45.0

    if (!m_cameraCapabilities.supportsBearing() && bearing != 0.0) {
        m_cameraData.setBearing(0);
        bearingHasChanged = true;
    }
    if (!m_cameraCapabilities.supportsTilting() && tilt != 0.0) {
        m_cameraData.setTilt(0);
        tiltHasChanged = true;
    }

    m_cameraData.setFieldOfView(qBound(m_cameraCapabilities.minimumFieldOfView(),
                                       fov,
                                       m_cameraCapabilities.maximumFieldOfView()));
    if (fov != m_cameraData.fieldOfView())
        fovHasChanged  = true;

    // set latitude boundary check
    m_maximumViewportLatitude = m_map->maximumCenterLatitudeAtZoom(m_cameraData);

    center.setLatitude(qBound(-m_maximumViewportLatitude, center.latitude(), m_maximumViewportLatitude));

    if (center != m_cameraData.center()) {
        centerHasChanged = true;
        m_cameraData.setCenter(center);
    }

    m_map->setCameraData(m_cameraData);

    m_initialized = true;

    if (centerHasChanged)
        emit centerChanged(m_cameraData.center());

    if (bearingHasChanged)
        emit bearingChanged(m_cameraData.bearing());

    if (tiltHasChanged)
        emit tiltChanged(m_cameraData.tilt());

    if (fovHasChanged)
        emit fieldOfViewChanged(m_cameraData.fieldOfView());

    emit mapReadyChanged(true);

    if (m_copyrights)
         update();
}

/*!
    \internal
*/
void QDeclarativeGeoMap::pluginReady()
{
    QGeoServiceProvider *provider = m_plugin->sharedGeoServiceProvider();
    m_mappingManager = provider->mappingManager();

    if (provider->error() != QGeoServiceProvider::NoError) {
        setError(provider->error(), provider->errorString());
        return;
    }

    if (!m_mappingManager) {
        //TODO Should really be EngineNotSetError (see QML GeoCodeModel)
        setError(QGeoServiceProvider::NotSupportedError, tr("Plugin does not support mapping."));
        return;
    }

    if (!m_mappingManager->isInitialized())
        connect(m_mappingManager, SIGNAL(initialized()), this, SLOT(mappingManagerInitialized()));
    else
        mappingManagerInitialized();

    // make sure this is only called once
    disconnect(this, SLOT(pluginReady()));
}

/*!
    \internal
*/
void QDeclarativeGeoMap::componentComplete()
{
    m_componentCompleted = true;
    populateParameters();
    populateMap();
    QQuickItem::componentComplete();
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mousePressEvent(QMouseEvent *event)
{
    if (isInteractive())
        m_gestureArea->handleMousePressEvent(event);
    else
        QQuickItem::mousePressEvent(event);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mouseMoveEvent(QMouseEvent *event)
{
    if (isInteractive())
        m_gestureArea->handleMouseMoveEvent(event);
    else
        QQuickItem::mouseMoveEvent(event);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mouseReleaseEvent(QMouseEvent *event)
{
    if (isInteractive()) {
        m_gestureArea->handleMouseReleaseEvent(event);
    } else {
        QQuickItem::mouseReleaseEvent(event);
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mouseUngrabEvent()
{
    if (isInteractive())
        m_gestureArea->handleMouseUngrabEvent();
    else
        QQuickItem::mouseUngrabEvent();
}

void QDeclarativeGeoMap::touchUngrabEvent()
{
    if (isInteractive())
        m_gestureArea->handleTouchUngrabEvent();
    else
        QQuickItem::touchUngrabEvent();
}

/*!
    \qmlproperty MapGestureArea QtLocation::Map::gesture

    Contains the MapGestureArea created with the Map. This covers pan, flick and pinch gestures.
    Use \c{gesture.enabled: true} to enable basic gestures, or see \l{MapGestureArea} for
    further details.
*/

QQuickGeoMapGestureArea *QDeclarativeGeoMap::gesture()
{
    return m_gestureArea;
}

/*!
    \internal

    This may happen before mappingManagerInitialized()
*/
void QDeclarativeGeoMap::populateMap()
{
    QObjectList kids = children();
    QList<QQuickItem *> quickKids = childItems();
    for (int i=0; i < quickKids.count(); ++i)
        kids.append(quickKids.at(i));

    for (int i = 0; i < kids.size(); ++i) {
        // dispatch items appropriately
        QDeclarativeGeoMapItemView *mapView = qobject_cast<QDeclarativeGeoMapItemView *>(kids.at(i));
        if (mapView) {
            m_mapViews.append(mapView);
            setupMapView(mapView);
            continue;
        }
        QDeclarativeGeoMapItemBase *mapItem = qobject_cast<QDeclarativeGeoMapItemBase *>(kids.at(i));
        if (mapItem) {
            addMapItem(mapItem);
            continue;
        }
        // Allow to add to the map Map items contained inside a parent QQuickItem, but only those at one level of nesting.
        QDeclarativeGeoMapItemGroup *itemGroup = qobject_cast<QDeclarativeGeoMapItemGroup *>(kids.at(i));
        if (itemGroup) {
            addMapItemGroup(itemGroup);
            continue;
        }
    }
}

void QDeclarativeGeoMap::populateParameters()
{
    QObjectList kids = children();
    QList<QQuickItem *> quickKids = childItems();
    for (int i = 0; i < quickKids.count(); ++i)
        kids.append(quickKids.at(i));
    for (int i = 0; i < kids.size(); ++i) {
        QDeclarativeGeoMapParameter *mapParameter = qobject_cast<QDeclarativeGeoMapParameter *>(kids.at(i));
        if (mapParameter)
            addMapParameter(mapParameter);
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMap::setupMapView(QDeclarativeGeoMapItemView *view)
{
    Q_UNUSED(view)
    view->setMap(this);
    view->repopulate();
}

/*!
 * \internal
 */
QSGNode *QDeclarativeGeoMap::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_map) {
        delete oldNode;
        return 0;
    }

    QSGRectangleNode *root = static_cast<QSGRectangleNode *>(oldNode);
    if (!root)
        root = window()->createRectangleNode();

    root->setRect(boundingRect());
    root->setColor(m_color);

    QSGNode *content = root->childCount() ? root->firstChild() : 0;
    content = m_map->updateSceneGraph(content, window());
    if (content && root->childCount() == 0)
        root->appendChildNode(content);

    return root;
}

/*!
    \qmlproperty Plugin QtLocation::Map::plugin

    This property holds the plugin which provides the mapping functionality.

    This is a write-once property. Once the map has a plugin associated with
    it, any attempted modifications of the plugin will be ignored.
*/

void QDeclarativeGeoMap::setPlugin(QDeclarativeGeoServiceProvider *plugin)
{
    if (m_plugin) {
        qmlWarning(this) << QStringLiteral("Plugin is a write-once property, and cannot be set again.");
        return;
    }
    m_plugin = plugin;
    emit pluginChanged(m_plugin);

    if (m_plugin->isAttached()) {
        pluginReady();
    } else {
        connect(m_plugin, SIGNAL(attached()),
                this, SLOT(pluginReady()));
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMap::onCameraCapabilitiesChanged(const QGeoCameraCapabilities &oldCameraCapabilities)
{
    if (m_map->cameraCapabilities() == oldCameraCapabilities)
        return;
    m_cameraCapabilities = m_map->cameraCapabilities();

    //The zoom level limits are only restricted by the plugins values, if the user has set a more
    //strict zoom level limit before initialization nothing is done here.
    //minimum zoom level might be changed to limit gray bundaries
    //This code assumes that plugins' maximum zoom level will never exceed 30.0
    if (m_cameraCapabilities.maximumZoomLevelAt256() < m_gestureArea->maximumZoomLevel()) {
        setMaximumZoomLevel(m_cameraCapabilities.maximumZoomLevelAt256(), false);
    } else if (m_cameraCapabilities.maximumZoomLevelAt256() > m_gestureArea->maximumZoomLevel()) {
        if (qIsNaN(m_userMaximumZoomLevel)) {
            // If the user didn't set anything
            setMaximumZoomLevel(m_cameraCapabilities.maximumZoomLevelAt256(), false);
        } else {  // Try to set what the user requested
            // Else if the user set something larger, but that got clamped by the previous camera caps
            setMaximumZoomLevel(qMin<qreal>(m_cameraCapabilities.maximumZoomLevelAt256(),
                                            m_userMaximumZoomLevel), false);
        }
    }

    if (m_cameraCapabilities.minimumZoomLevelAt256() > m_gestureArea->minimumZoomLevel()) {
        setMinimumZoomLevel(m_cameraCapabilities.minimumZoomLevelAt256(), false);
    } else if (m_cameraCapabilities.minimumZoomLevelAt256() < m_gestureArea->minimumZoomLevel()) {
        if (qIsNaN(m_userMinimumZoomLevel)) {
            // If the user didn't set anything, trying to set the new caps.
            setMinimumZoomLevel(m_cameraCapabilities.minimumZoomLevelAt256(), false);
        } else {  // Try to set what the user requested
            // Else if the user set a minimum, m_gestureArea->minimumZoomLevel() might be larger
            // because of different reasons. Resetting it, as if it ends to be the same,
            // no signal will be emitted.
            setMinimumZoomLevel(qMax<qreal>(m_cameraCapabilities.minimumZoomLevelAt256(),
                                            m_userMinimumZoomLevel), false);
        }
    }

    // Tilt
    if (m_cameraCapabilities.maximumTilt() < m_maximumTilt) {
        setMaximumTilt(m_cameraCapabilities.maximumTilt(), false);
    } else if (m_cameraCapabilities.maximumTilt() > m_maximumTilt) {
        if (qIsNaN(m_userMaximumTilt))
            setMaximumTilt(m_cameraCapabilities.maximumTilt(), false);
        else // Try to set what the user requested
            setMaximumTilt(qMin<qreal>(m_cameraCapabilities.maximumTilt(), m_userMaximumTilt), false);
    }

    if (m_cameraCapabilities.minimumTilt() > m_minimumTilt) {
        setMinimumTilt(m_cameraCapabilities.minimumTilt(), false);
    } else if (m_cameraCapabilities.minimumTilt() < m_minimumTilt) {
        if (qIsNaN(m_userMinimumTilt))
            setMinimumTilt(m_cameraCapabilities.minimumTilt(), false);
        else // Try to set what the user requested
            setMinimumTilt(qMax<qreal>(m_cameraCapabilities.minimumTilt(), m_userMinimumTilt), false);
    }

    // FoV
    if (m_cameraCapabilities.maximumFieldOfView() < m_maximumFieldOfView) {
        setMaximumFieldOfView(m_cameraCapabilities.maximumFieldOfView(), false);
    } else if (m_cameraCapabilities.maximumFieldOfView() > m_maximumFieldOfView) {
        if (qIsNaN(m_userMaximumFieldOfView))
            setMaximumFieldOfView(m_cameraCapabilities.maximumFieldOfView(), false);
        else // Try to set what the user requested
            setMaximumFieldOfView(qMin<qreal>(m_cameraCapabilities.maximumFieldOfView(), m_userMaximumFieldOfView), false);
    }

    if (m_cameraCapabilities.minimumFieldOfView() > m_minimumFieldOfView) {
        setMinimumFieldOfView(m_cameraCapabilities.minimumFieldOfView(), false);
    } else if (m_cameraCapabilities.minimumFieldOfView() < m_minimumFieldOfView) {
        if (qIsNaN(m_userMinimumFieldOfView))
            setMinimumFieldOfView(m_cameraCapabilities.minimumFieldOfView(), false);
        else // Try to set what the user requested
            setMinimumFieldOfView(qMax<qreal>(m_cameraCapabilities.minimumFieldOfView(), m_userMinimumFieldOfView), false);
    }
}


/*!
    \internal
    this function will only be ever called once
*/
void QDeclarativeGeoMap::mappingManagerInitialized()
{
    m_map = QPointer<QGeoMap>(m_mappingManager->createMap(this));

    if (!m_map)
        return;

    m_gestureArea->setMap(m_map);

    QList<QGeoMapType> types = m_mappingManager->supportedMapTypes();
    for (int i = 0; i < types.size(); ++i) {
        QDeclarativeGeoMapType *type = new QDeclarativeGeoMapType(types[i], this);
        m_supportedMapTypes.append(type);
    }

    if (m_activeMapType && m_plugin->name().toLatin1() == m_activeMapType->mapType().pluginName()) {
        m_map->setActiveMapType(m_activeMapType->mapType());
    } else {
        if (m_activeMapType)
            m_activeMapType->deleteLater();

        if (!m_supportedMapTypes.isEmpty()) {
                m_activeMapType = m_supportedMapTypes.at(0);
                m_map->setActiveMapType(m_activeMapType->mapType());
        } else {
            m_activeMapType = new QDeclarativeGeoMapType(QGeoMapType(QGeoMapType::NoMap,
                                                                     tr("No Map"),
                                                                     tr("No Map"), false, false, 0, QByteArrayLiteral("")), this);
        }
    }

    // Update camera capabilities
    onCameraCapabilitiesChanged(m_cameraCapabilities);

    // Map tiles are built in this call. m_map->minimumZoom() becomes operational
    // after this has been called at least once, after creation.
    // However, getting into the following block may fire a copyrightsChanged that would get lost,
    // as the connections are set up after.
    QString copyrightString;
    QImage copyrightImage;
    if (!m_initialized && width() > 0 && height() > 0) {
        QMetaObject::Connection copyrightStringCatcherConnection =
                connect(m_map.data(),
                        QOverload<const QString &>::of(&QGeoMap::copyrightsChanged),
                        [&copyrightString](const QString &copy){ copyrightString = copy; });
        QMetaObject::Connection copyrightImageCatcherConnection =
                connect(m_map.data(),
                        QOverload<const QImage &>::of(&QGeoMap::copyrightsChanged),
                        [&copyrightImage](const QImage &copy){ copyrightImage = copy; });
        m_map->setViewportSize(QSize(width(), height()));
        initialize();
        QObject::disconnect(copyrightStringCatcherConnection);
        QObject::disconnect(copyrightImageCatcherConnection);
    }

    m_copyrights = new QDeclarativeGeoMapCopyrightNotice(this);
    m_copyrights->onCopyrightsStyleSheetChanged(m_map->copyrightsStyleSheet());

    connect(m_map.data(), SIGNAL(copyrightsChanged(QImage)),
            m_copyrights.data(), SLOT(copyrightsChanged(QImage)));
    connect(m_map.data(), SIGNAL(copyrightsChanged(QImage)),
            this,  SIGNAL(copyrightsChanged(QImage)));

    connect(m_map.data(), SIGNAL(copyrightsChanged(QString)),
            m_copyrights.data(), SLOT(copyrightsChanged(QString)));
    connect(m_map.data(), SIGNAL(copyrightsChanged(QString)),
            this,  SIGNAL(copyrightsChanged(QString)));

    if (!copyrightString.isEmpty())
        emit m_map.data()->copyrightsChanged(copyrightString);
    else if (!copyrightImage.isNull())
        emit m_map.data()->copyrightsChanged(copyrightImage);

    connect(m_map.data(), SIGNAL(copyrightsStyleSheetChanged(QString)),
            m_copyrights.data(), SLOT(onCopyrightsStyleSheetChanged(QString)));

    connect(m_copyrights.data(), SIGNAL(linkActivated(QString)),
            this, SIGNAL(copyrightLinkActivated(QString)));
    connect(m_map.data(), &QGeoMap::sgNodeChanged, this, &QQuickItem::update);
    connect(m_map.data(), &QGeoMap::cameraCapabilitiesChanged, this, &QDeclarativeGeoMap::onCameraCapabilitiesChanged);

    // set visibility of copyright notice
    m_copyrights->setCopyrightsVisible(m_copyrightsVisible);

    // This prefetches a buffer around the map
    m_map->prefetchData();

    connect(m_mappingManager, SIGNAL(supportedMapTypesChanged()), this, SLOT(onSupportedMapTypesChanged()));
    emit minimumZoomLevelChanged();
    emit maximumZoomLevelChanged();
    emit supportedMapTypesChanged();
    emit activeMapTypeChanged();

    // Any map items that were added before the plugin was ready
    // need to have setMap called again
    for (const QPointer<QDeclarativeGeoMapItemBase> &item : qAsConst(m_mapItems)) {
        if (item) {
            item->setMap(this, m_map);
            m_map->addMapItem(item.data()); // m_map filters out what is not supported.
        }
    }

    // Any map item groups that were added before the plugin was ready
    // need to have setMap called again on their children map items
    for (auto g: qAsConst(m_mapItemGroups)) {
        const QList<QQuickItem *> quickKids = g->childItems();
        for (auto c: quickKids) {
            QDeclarativeGeoMapItemBase *itemBase = qobject_cast<QDeclarativeGeoMapItemBase *>(c);
            if (itemBase)
                itemBase->setMap(this, m_map);
        }
    }

    // All map parameters that were added before the plugin was ready
    // need to be added to m_map
    for (QDeclarativeGeoMapParameter *p : qAsConst(m_mapParameters))
        m_map->addParameter(p);

    if (m_initialized)
        update();
}

/*!
    \internal
*/
QDeclarativeGeoServiceProvider *QDeclarativeGeoMap::plugin() const
{
    return m_plugin;
}

/*!
    \internal
    Sets the gesture areas minimum zoom level. If the camera capabilities
    has been set this method honors the boundaries set by it.
    The minimum zoom level will also have a lower bound dependent on the size
    of the canvas, effectively preventing to display out of bounds areas.
*/
void QDeclarativeGeoMap::setMinimumZoomLevel(qreal minimumZoomLevel, bool userSet)
{

    if (minimumZoomLevel >= 0) {
        if (userSet)
            m_userMinimumZoomLevel = minimumZoomLevel;
        qreal oldMinimumZoomLevel = this->minimumZoomLevel();

        minimumZoomLevel = qBound(qreal(m_cameraCapabilities.minimumZoomLevelAt256()), minimumZoomLevel, maximumZoomLevel());
        if (m_map)
             minimumZoomLevel = qMax<qreal>(minimumZoomLevel, m_map->minimumZoom());

        m_gestureArea->setMinimumZoomLevel(minimumZoomLevel);

        if (zoomLevel() < minimumZoomLevel && (m_gestureArea->enabled() || !m_cameraCapabilities.overzoomEnabled()))
            setZoomLevel(minimumZoomLevel);

        if (oldMinimumZoomLevel != minimumZoomLevel)
            emit minimumZoomLevelChanged();
    }
}

/*!
    \qmlproperty real QtLocation::Map::minimumZoomLevel

    This property holds the minimum valid zoom level for the map.

    The minimum zoom level defined by the \l plugin used is a lower bound for
    this property. However, the returned value is also canvas-size-dependent, and
    can be higher than the user-specified value, or than the minimum zoom level
    defined by the plugin used, to prevent the map from being smaller than the
    viewport in either dimension.

    If the \l plugin property is not set or the plugin does not support mapping, this property is \c 0.
*/

qreal QDeclarativeGeoMap::minimumZoomLevel() const
{
    return m_gestureArea->minimumZoomLevel();
}

/*!
    \internal
    Sets the gesture areas maximum zoom level. If the camera capabilities
    has been set this method honors the boundaries set by it.
*/
void QDeclarativeGeoMap::setMaximumZoomLevel(qreal maximumZoomLevel, bool userSet)
{
    if (maximumZoomLevel >= 0) {
        if (userSet)
            m_userMaximumZoomLevel = maximumZoomLevel;
        qreal oldMaximumZoomLevel = this->maximumZoomLevel();

        maximumZoomLevel = qBound(minimumZoomLevel(), maximumZoomLevel, qreal(m_cameraCapabilities.maximumZoomLevelAt256()));

        m_gestureArea->setMaximumZoomLevel(maximumZoomLevel);

        if (zoomLevel() > maximumZoomLevel && (m_gestureArea->enabled() || !m_cameraCapabilities.overzoomEnabled()))
            setZoomLevel(maximumZoomLevel);

        if (oldMaximumZoomLevel != maximumZoomLevel)
            emit maximumZoomLevelChanged();
    }
}

/*!
    \qmlproperty real QtLocation::Map::maximumZoomLevel

    This property holds the maximum valid zoom level for the map.

    The maximum zoom level is defined by the \l plugin used.
    If the \l plugin property is not set or the plugin does not support mapping, this property is \c 30.
*/

qreal QDeclarativeGeoMap::maximumZoomLevel() const
{
    return m_gestureArea->maximumZoomLevel();
}

/*!
    \qmlproperty real QtLocation::Map::zoomLevel

    This property holds the zoom level for the map.

    Larger values for the zoom level provide more detail. Zoom levels
    are always non-negative. The default value is 8.0. Depending on the plugin in use,
    values outside the [minimumZoomLevel, maximumZoomLevel] range, which represent the range for which
    tiles are available, may be accepted, or clamped.
*/
void QDeclarativeGeoMap::setZoomLevel(qreal zoomLevel)
{
    return setZoomLevel(zoomLevel, m_cameraCapabilities.overzoomEnabled());
}

/*!
    \internal

    Sets the zoom level.
    Larger values for the zoom level provide more detail. Zoom levels
    are always non-negative. The default value is 8.0. Values outside the
    [minimumZoomLevel, maximumZoomLevel] range, which represent the range for which
    tiles are available, can be accepted or clamped by setting the overzoom argument
    to true or false respectively.
*/
void QDeclarativeGeoMap::setZoomLevel(qreal zoomLevel, bool overzoom)
{
    if (m_cameraData.zoomLevel() == zoomLevel || zoomLevel < 0)
        return;

    //small optimization to avoid double setCameraData
    bool centerHasChanged = false;

    if (m_initialized) {
        m_cameraData.setZoomLevel(qBound<qreal>(overzoom ? m_map->minimumZoom() : minimumZoomLevel(),
                                                zoomLevel,
                                                overzoom ? 30 : maximumZoomLevel()));
        m_maximumViewportLatitude = m_map->maximumCenterLatitudeAtZoom(m_cameraData);
        QGeoCoordinate coord = m_cameraData.center();
        coord.setLatitude(qBound(-m_maximumViewportLatitude, coord.latitude(), m_maximumViewportLatitude));
        if (coord != m_cameraData.center()) {
            centerHasChanged = true;
            m_cameraData.setCenter(coord);
        }
        m_map->setCameraData(m_cameraData);
    } else {
        m_cameraData.setZoomLevel(zoomLevel);
    }

    if (centerHasChanged)
        emit centerChanged(m_cameraData.center());
    emit zoomLevelChanged(m_cameraData.zoomLevel());
}

qreal QDeclarativeGeoMap::zoomLevel() const
{
    return m_cameraData.zoomLevel();
}

/*!
    \qmlproperty real QtLocation::Map::bearing

    This property holds the bearing for the map.
    The default value is 0.
    If the Plugin used for the Map supports bearing, the valid range for this value is between 0 and 360.
    If the Plugin used for the Map does not support bearing, changing this property will have no effect.

    \since Qt Location 5.9
*/
void QDeclarativeGeoMap::setBearing(qreal bearing)
{
    bearing = std::fmod(bearing, qreal(360.0));
    if (bearing < 0.0)
        bearing += 360.0;
    if (m_map && !m_cameraCapabilities.supportsBearing())
        bearing = 0.0;
    if (m_cameraData.bearing() == bearing || bearing < 0.0)
        return;

    m_cameraData.setBearing(bearing);
    if (m_map)
        m_map->setCameraData(m_cameraData);
    emit bearingChanged(bearing);
}

qreal QDeclarativeGeoMap::bearing() const
{
    return m_cameraData.bearing();
}

/*!
    \qmlproperty real QtLocation::Map::tilt

    This property holds the tilt for the map, in degrees.
    The default value is 0.
    The valid range for this value is [ minimumTilt, maximumTilt ].
    If the Plugin used for the Map does not support tilting, changing this property will have no effect.

    \sa minimumTilt, maximumTilt

    \since Qt Location 5.9
*/
void QDeclarativeGeoMap::setTilt(qreal tilt)
{
    tilt = qBound(minimumTilt(), tilt, maximumTilt());
    if (m_cameraData.tilt() == tilt)
        return;

    m_cameraData.setTilt(tilt);
    if (m_map)
        m_map->setCameraData(m_cameraData);
    emit tiltChanged(tilt);
}

qreal QDeclarativeGeoMap::tilt() const
{
    return m_cameraData.tilt();
}

void QDeclarativeGeoMap::setMinimumTilt(qreal minimumTilt, bool userSet)
{
    if (minimumTilt >= 0) {
        if (userSet)
            m_userMinimumTilt = minimumTilt;
        qreal oldMinimumTilt = this->minimumTilt();

        m_minimumTilt = qBound(m_cameraCapabilities.minimumTilt(),
                               minimumTilt,
                               m_cameraCapabilities.maximumTilt());

        if (tilt() < m_minimumTilt)
            setTilt(m_minimumTilt);

        if (oldMinimumTilt != m_minimumTilt)
            emit minimumTiltChanged(m_minimumTilt);
    }
}

/*!
    \qmlproperty real QtLocation::Map::fieldOfView

    This property holds the field of view of the camera used to look at the map, in degrees.
    If the plugin property of the map is not set, or the plugin does not support mapping, the value is 45 degrees.

    Note that changing this value implicitly changes also the distance between the camera and the map,
    so that, at a tilting angle of 0 degrees, the resulting image is identical for any value used for this property.

    For more information about this parameter, consult the Wikipedia articles about \l {https://en.wikipedia.org/wiki/Field_of_view} {Field of view} and
    \l {https://en.wikipedia.org/wiki/Angle_of_view} {Angle of view}.

    \sa minimumFieldOfView, maximumFieldOfView

    \since Qt Location 5.9
*/
void QDeclarativeGeoMap::setFieldOfView(qreal fieldOfView)
{
    fieldOfView = qBound(minimumFieldOfView(), fieldOfView, maximumFieldOfView());
    if (m_cameraData.fieldOfView() == fieldOfView)
        return;

    m_cameraData.setFieldOfView(fieldOfView);
    if (m_map)
        m_map->setCameraData(m_cameraData);
    emit fieldOfViewChanged(fieldOfView);
}

qreal QDeclarativeGeoMap::fieldOfView() const
{
    return m_cameraData.fieldOfView();
}

void QDeclarativeGeoMap::setMinimumFieldOfView(qreal minimumFieldOfView, bool userSet)
{
    if (minimumFieldOfView > 0 && minimumFieldOfView < 180.0) {
        if (userSet)
            m_userMinimumFieldOfView = minimumFieldOfView;
        qreal oldMinimumFoV = this->minimumFieldOfView();

        m_minimumFieldOfView = qBound(m_cameraCapabilities.minimumFieldOfView(),
                                      minimumFieldOfView,
                                      m_cameraCapabilities.maximumFieldOfView());

        if (fieldOfView() < m_minimumFieldOfView)
            setFieldOfView(m_minimumFieldOfView);

        if (oldMinimumFoV != m_minimumFieldOfView)
            emit minimumFieldOfViewChanged(m_minimumFieldOfView);
    }
}

/*!
    \qmlproperty bool QtLocation::Map::minimumFieldOfView

    This property holds the minimum valid field of view for the map, in degrees.

    The minimum tilt field of view by the \l plugin used is a lower bound for
    this property.
    If the \l plugin property is not set or the plugin does not support mapping, this property is \c 1.

    \sa fieldOfView, maximumFieldOfView

    \since Qt Location 5.9
*/
qreal QDeclarativeGeoMap::minimumFieldOfView() const
{
    return m_minimumFieldOfView;
}

void QDeclarativeGeoMap::setMaximumFieldOfView(qreal maximumFieldOfView, bool userSet)
{
    if (maximumFieldOfView > 0 && maximumFieldOfView < 180.0) {
        if (userSet)
            m_userMaximumFieldOfView = maximumFieldOfView;
        qreal oldMaximumFoV = this->maximumFieldOfView();

        m_maximumFieldOfView = qBound(m_cameraCapabilities.minimumFieldOfView(),
                                      maximumFieldOfView,
                                      m_cameraCapabilities.maximumFieldOfView());

        if (fieldOfView() > m_maximumFieldOfView)
            setFieldOfView(m_maximumFieldOfView);

        if (oldMaximumFoV != m_maximumFieldOfView)
            emit maximumFieldOfViewChanged(m_maximumFieldOfView);
    }
}

/*!
    \qmlproperty bool QtLocation::Map::maximumFieldOfView

    This property holds the maximum valid field of view for the map, in degrees.

    The minimum tilt field of view by the \l plugin used is an upper bound for
    this property.
    If the \l plugin property is not set or the plugin does not support mapping, this property is \c 179.

    \sa fieldOfView, minimumFieldOfView

    \since Qt Location 5.9
*/
qreal QDeclarativeGeoMap::maximumFieldOfView() const
{
    return m_maximumFieldOfView;
}

/*!
    \qmlproperty bool QtLocation::Map::minimumTilt

    This property holds the minimum valid tilt for the map, in degrees.

    The minimum tilt defined by the \l plugin used is a lower bound for
    this property.
    If the \l plugin property is not set or the plugin does not support mapping, this property is \c 0.

    \sa tilt, maximumTilt

    \since Qt Location 5.9
*/
qreal QDeclarativeGeoMap::minimumTilt() const
{
    return m_minimumTilt;
}

void QDeclarativeGeoMap::setMaximumTilt(qreal maximumTilt, bool userSet)
{
    if (maximumTilt >= 0) {
        if (userSet)
            m_userMaximumTilt = maximumTilt;
        qreal oldMaximumTilt = this->maximumTilt();

        m_maximumTilt = qBound(m_cameraCapabilities.minimumTilt(),
                               maximumTilt,
                               m_cameraCapabilities.maximumTilt());

        if (tilt() > m_maximumTilt)
            setTilt(m_maximumTilt);

        if (oldMaximumTilt != m_maximumTilt)
            emit maximumTiltChanged(m_maximumTilt);
    }
}

/*!
    \qmlproperty bool QtLocation::Map::maximumTilt

    This property holds the maximum valid tilt for the map, in degrees.

    The maximum tilt defined by the \l plugin used is an upper bound for
    this property.
    If the \l plugin property is not set or the plugin does not support mapping, this property is \c 89.5.

    \sa tilt, minimumTilt

    \since Qt Location 5.9
*/
qreal QDeclarativeGeoMap::maximumTilt() const
{
    return m_maximumTilt;
}

/*!
    \qmlproperty coordinate QtLocation::Map::center

    This property holds the coordinate which occupies the center of the
    mapping viewport. Invalid center coordinates are ignored.

    The default value is an arbitrary valid coordinate.
*/
void QDeclarativeGeoMap::setCenter(const QGeoCoordinate &center)
{
    if (center == m_cameraData.center())
        return;

    if (!center.isValid())
        return;

    if (m_initialized) {
        QGeoCoordinate coord(center);
        coord.setLatitude(qBound(-m_maximumViewportLatitude, center.latitude(), m_maximumViewportLatitude));
        m_cameraData.setCenter(coord);
        m_map->setCameraData(m_cameraData);
    } else {
        m_cameraData.setCenter(center);
    }

    emit centerChanged(m_cameraData.center());
}

QGeoCoordinate QDeclarativeGeoMap::center() const
{
    return m_cameraData.center();
}


/*!
    \qmlproperty geoshape QtLocation::Map::visibleRegion

    This property holds the region which occupies the viewport of
    the map. The camera is positioned in the center of the shape, and
    at the largest integral zoom level possible which allows the
    whole shape to be visible on the screen. This implies that
    reading this property back shortly after having been set the
    returned area is equal or larger than the set area.

    Setting this property implicitly changes the \l center and
    \l zoomLevel of the map. Any previously set value to those
    properties will be overridden.

    This property does not provide any change notifications.

    \since 5.6
*/
void QDeclarativeGeoMap::setVisibleRegion(const QGeoShape &shape)
{
    if (shape.boundingGeoRectangle() == visibleRegion())
        return;

    m_visibleRegion = shape.boundingGeoRectangle();
    if (!m_visibleRegion.isValid()
        || (m_visibleRegion.bottomRight().latitude() >= 85.0) // rect entirely outside web mercator
        || (m_visibleRegion.topLeft().latitude() <= -85.0)) {
        // shape invalidated -> nothing to fit anymore
        m_visibleRegion = QGeoRectangle();
        m_pendingFitViewport = false;
        return;
    }

    if (!m_map || !width() || !height()) {
        m_pendingFitViewport = true;
        return;
    }

    fitViewportToGeoShape();
}

QGeoShape QDeclarativeGeoMap::visibleRegion() const
{
    if (!m_map || !width() || !height())
        return m_visibleRegion;

    const QList<QDoubleVector2D> &visibleRegion = m_map->geoProjection().visibleRegion();
    QGeoPath path;
    for (const QDoubleVector2D &c: visibleRegion)
        path.addCoordinate(m_map->geoProjection().wrappedMapProjectionToGeo(c));

    QGeoRectangle vr = path.boundingGeoRectangle();

    bool empty = vr.topLeft().latitude() == vr.bottomRight().latitude() ||
            qFuzzyCompare(vr.topLeft().longitude(), vr.bottomRight().longitude()); // QTBUG-57690

    if (empty) {
        vr.setTopLeft(QGeoCoordinate(vr.topLeft().latitude(), -180));
        vr.setBottomRight(QGeoCoordinate(vr.bottomRight().latitude(), 180));
    }

    return vr;
}

/*!
    \qmlproperty bool QtLocation::Map::copyrightsVisible

    This property holds the visibility of the copyrights notice. The notice is usually
    displayed in the bottom left corner. By default, this property is set to \c true.

    \note Many map providers require the notice to be visible as part of the terms and conditions.
    Please consult the relevant provider documentation before turning this notice off.

    \since 5.7
*/
void QDeclarativeGeoMap::setCopyrightsVisible(bool visible)
{
    if (m_copyrightsVisible == visible)
        return;

    if (!m_copyrights.isNull())
        m_copyrights->setCopyrightsVisible(visible);

    m_copyrightsVisible = visible;
    emit copyrightsVisibleChanged(visible);
}

bool QDeclarativeGeoMap::copyrightsVisible() const
{
    return m_copyrightsVisible;
}



/*!
    \qmlproperty color QtLocation::Map::color

    This property holds the background color of the map element.

    \since 5.6
*/
void QDeclarativeGeoMap::setColor(const QColor &color)
{
    if (color != m_color) {
        m_color = color;
        update();
        emit colorChanged(m_color);
    }
}

QColor QDeclarativeGeoMap::color() const
{
    return m_color;
}

/*!
    \qmlproperty color QtLocation::Map::mapReady

    This property holds whether the map has been successfully initialized and is ready to be used.
    Some methods, such as \l fromCoordinate and \l toCoordinate, will not work before the map is ready.
    Due to the architecture of the \l Map, it's advised to use the signal emitted for this property
    in place of \l {QtQml::Component::completed()}{Component.onCompleted}, to make sure that everything behaves as expected.

    \since 5.9
*/
bool QDeclarativeGeoMap::mapReady() const
{
    return m_initialized;
}

// TODO: offer the possibility to specify the margins.
void QDeclarativeGeoMap::fitViewportToGeoShape()
{
    const int margins  = 10;
    if (!m_map  || !m_visibleRegion.isValid() || width() <= margins || height() <= margins)
        return;

    QDoubleVector2D topLeftPoint = m_map->geoProjection().geoToMapProjection(m_visibleRegion.topLeft());
    QDoubleVector2D bottomRightPoint = m_map->geoProjection().geoToMapProjection(m_visibleRegion.bottomRight());
    if (bottomRightPoint.x() < topLeftPoint.x()) // crossing the dateline
        bottomRightPoint.setX(bottomRightPoint.x() + 1.0);

    // find center of the bounding box
    QDoubleVector2D center = (topLeftPoint + bottomRightPoint) * 0.5;
    center.setX(center.x() > 1.0 ? center.x() - 1.0 : center.x());
    QGeoCoordinate centerCoordinate = m_map->geoProjection().mapProjectionToGeo(center);

    // position camera to the center of bounding box
    setCenter(centerCoordinate);

    // if the shape is empty we just change center position, not zoom
    double bboxWidth  = (bottomRightPoint.x() - topLeftPoint.x()) * m_map->mapWidth();
    double bboxHeight = (bottomRightPoint.y() - topLeftPoint.y()) * m_map->mapHeight();

    if (bboxHeight == 0.0 && bboxWidth == 0.0)
        return;

    double zoomRatio = qMax(bboxWidth / (width() - margins),
                            bboxHeight / (height() - margins));
    zoomRatio = std::log(zoomRatio) / std::log(2.0);
    double newZoom = qMax<double>(minimumZoomLevel(), zoomLevel() - zoomRatio);
    setZoomLevel(newZoom);
}


/*!
    \qmlproperty list<MapType> QtLocation::Map::supportedMapTypes

    This read-only property holds the set of \l{MapType}{map types} supported by this map.

    \sa activeMapType
*/
QQmlListProperty<QDeclarativeGeoMapType> QDeclarativeGeoMap::supportedMapTypes()
{
    return QQmlListProperty<QDeclarativeGeoMapType>(this, m_supportedMapTypes);
}

/*!
    \qmlmethod coordinate QtLocation::Map::toCoordinate(QPointF position, bool clipToViewPort)

    Returns the coordinate which corresponds to the \a position relative to the map item.

    If \a cliptoViewPort is \c true, or not supplied then returns an invalid coordinate if
    \a position is not within the current viewport.
*/
QGeoCoordinate QDeclarativeGeoMap::toCoordinate(const QPointF &position, bool clipToViewPort) const
{
    if (m_map)
        return m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(position), clipToViewPort);
    else
        return QGeoCoordinate();
}

/*!
    \qmlmethod point QtLocation::Map::fromCoordinate(coordinate coordinate, bool clipToViewPort)

    Returns the position relative to the map item which corresponds to the \a coordinate.

    If \a cliptoViewPort is \c true, or not supplied then returns an invalid QPointF if
    \a coordinate is not within the current viewport.
*/
QPointF QDeclarativeGeoMap::fromCoordinate(const QGeoCoordinate &coordinate, bool clipToViewPort) const
{
    if (m_map)
        return m_map->geoProjection().coordinateToItemPosition(coordinate, clipToViewPort).toPointF();
    else
        return QPointF(qQNaN(), qQNaN());
}

/*!
    \qmlmethod void QtLocation::Map::pan(int dx, int dy)

    Starts panning the map by \a dx pixels along the x-axis and
    by \a dy pixels along the y-axis.

    Positive values for \a dx move the map right, negative values left.
    Positive values for \a dy move the map down, negative values up.

    During panning the \l center, and \l zoomLevel may change.
*/
void QDeclarativeGeoMap::pan(int dx, int dy)
{
    if (!m_map)
        return;
    if (dx == 0 && dy == 0)
        return;

    QGeoCoordinate coord = m_map->geoProjection().itemPositionToCoordinate(
                                QDoubleVector2D(m_map->viewportWidth() / 2 + dx,
                                        m_map->viewportHeight() / 2 + dy));
    setCenter(coord);
}


/*!
    \qmlmethod void QtLocation::Map::prefetchData()

    Optional hint that allows the map to prefetch during this idle period
*/
void QDeclarativeGeoMap::prefetchData()
{
    if (!m_map)
        return;
    m_map->prefetchData();
}

/*!
    \qmlmethod void QtLocation::Map::clearData()

    Clears map data collected by the currently selected plugin.
    \note This method will delete cached files.
    \sa plugin
*/
void QDeclarativeGeoMap::clearData()
{
    m_map->clearData();
}

/*!
    \qmlproperty string QtLocation::Map::errorString

    This read-only property holds the textual presentation of the latest mapping provider error.
    If no error has occurred, an empty string is returned.

    An empty string may also be returned if an error occurred which has no associated
    textual representation.

    \sa QGeoServiceProvider::errorString()
*/

QString QDeclarativeGeoMap::errorString() const
{
    return m_errorString;
}

/*!
    \qmlproperty enumeration QtLocation::Map::error

    This read-only property holds the last occurred mapping service provider error.

    \list
    \li Map.NoError - No error has occurred.
    \li Map.NotSupportedError -The maps plugin property was not set or there is no mapping manager associated with the plugin.
    \li Map.UnknownParameterError -The plugin did not recognize one of the parameters it was given.
    \li Map.MissingRequiredParameterError - The plugin did not find one of the parameters it was expecting.
    \li Map.ConnectionError - The plugin could not connect to its backend service or database.
    \endlist

    \sa QGeoServiceProvider::Error
*/

QGeoServiceProvider::Error QDeclarativeGeoMap::error() const
{
    return m_error;
}

/*!
    \internal
*/
void QDeclarativeGeoMap::touchEvent(QTouchEvent *event)
{
    if (isInteractive()) {
        m_gestureArea->handleTouchEvent(event);
    } else {
        //ignore event so sythesized event is generated;
        QQuickItem::touchEvent(event);
    }
}

#if QT_CONFIG(wheelevent)
/*!
    \internal
*/
void QDeclarativeGeoMap::wheelEvent(QWheelEvent *event)
{
    if (isInteractive())
        m_gestureArea->handleWheelEvent(event);
    else
        QQuickItem::wheelEvent(event);

}
#endif

bool QDeclarativeGeoMap::isInteractive()
{
    return (m_gestureArea->enabled() && m_gestureArea->acceptedGestures()) || m_gestureArea->isActive();
}

/*!
    \internal
*/
bool QDeclarativeGeoMap::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    Q_UNUSED(item)
    if (!isVisible() || !isEnabled() || !isInteractive())
        return QQuickItem::childMouseEventFilter(item, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        return sendMouseEvent(static_cast<QMouseEvent *>(event));
    case QEvent::UngrabMouse: {
        QQuickWindow *win = window();
        if (!win) break;
        if (!win->mouseGrabberItem() ||
                (win->mouseGrabberItem() &&
                 win->mouseGrabberItem() != this)) {
            // child lost grab, we could even lost
            // some events if grab already belongs for example
            // in item in diffrent window , clear up states
            mouseUngrabEvent();
        }
        break;
    }
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        if (static_cast<QTouchEvent *>(event)->touchPoints().count() >= 2) {
            // 1 touch point = handle with MouseEvent (event is always synthesized)
            // let the synthesized mouse event grab the mouse,
            // note there is no mouse grabber at this point since
            // touch event comes first (see Qt::AA_SynthesizeMouseForUnhandledTouchEvents)
            return sendTouchEvent(static_cast<QTouchEvent *>(event));
        }
    default:
        break;
    }
    return QQuickItem::childMouseEventFilter(item, event);
}

/*!
    \qmlmethod void QtLocation::Map::addMapItem(MapItem item)

    Adds the given \a item to the Map (for example MapQuickItem, MapCircle). If the object
    already is on the Map, it will not be added again.

    As an example, consider the case where you have a MapCircle representing your current position:

    \snippet declarative/maps.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/maps.qml Map addMapItem MapCircle at current position

    \note MapItemViews cannot be added with this method.

    \sa mapItems, removeMapItem, clearMapItems
*/

void QDeclarativeGeoMap::addMapItem(QDeclarativeGeoMapItemBase *item)
{
    if (!item || item->quickMap())
        return;
    // If the item comes from a MapItemGroup, do not reparent it.
    if (!qobject_cast<QDeclarativeGeoMapItemGroup *>(item->parentItem()))
        item->setParentItem(this);
    m_mapItems.append(item);
    if (m_map) {
        item->setMap(this, m_map);
        m_map->addMapItem(item);
    }
    emit mapItemsChanged();
}

/*!
    \qmlmethod void QtLocation::Map::addMapParameter(MapParameter parameter)

    Adds a MapParameter object to the map. The effect of this call is dependent
    on the combination of the content of the MapParameter and the type of
    underlying QGeoMap. If a MapParameter that is not supported by the underlying
    QGeoMap gets added, the call has no effect.

    The release of this API with Qt 5.9 is a Technology Preview.

    \sa MapParameter, removeMapParameter, mapParameters, clearMapParameters

    \since 5.9
*/
void QDeclarativeGeoMap::addMapParameter(QDeclarativeGeoMapParameter *parameter)
{
    if (!parameter->isComponentComplete()) {
        connect(parameter, &QDeclarativeGeoMapParameter::completed, this, &QDeclarativeGeoMap::addMapParameter);
        return;
    }

    disconnect(parameter);
    if (m_mapParameters.contains(parameter))
        return;
    parameter->setParent(this);
    m_mapParameters.append(parameter); // parameter now owned by QDeclarativeGeoMap
    if (m_map)
        m_map->addParameter(parameter);
}

/*!
    \qmlmethod void QtLocation::Map::removeMapParameter(MapParameter parameter)

    Removes the given MapParameter object from the map.

    The release of this API with Qt 5.9 is a Technology Preview.

    \sa MapParameter, addMapParameter, mapParameters, clearMapParameters

    \since 5.9
*/
void QDeclarativeGeoMap::removeMapParameter(QDeclarativeGeoMapParameter *parameter)
{
    if (!m_mapParameters.contains(parameter))
        return;
    if (m_map)
        m_map->removeParameter(parameter);
    m_mapParameters.removeOne(parameter);
}

/*!
    \qmlmethod void QtLocation::Map::clearMapParameters()

    Removes all map parameters from the map.

    The release of this API with Qt 5.9 is a Technology Preview.

    \sa MapParameter, mapParameters, addMapParameter, removeMapParameter, clearMapParameters

    \since 5.9
*/
void QDeclarativeGeoMap::clearMapParameters()
{
    if (m_map)
        m_map->clearParameters();
    m_mapParameters.clear();
}

/*!
    \qmlproperty list<MapParameters> QtLocation::Map::mapParameters

    Returns the list of all map parameters in no particular order.
    These items include map parameters that were declared statically as part of
    the type declaration, as well as dynamical map parameters (\l addMapParameter).

    The release of this API with Qt 5.9 is a Technology Preview.

    \sa MapParameter, addMapParameter, removeMapParameter, clearMapParameters

    \since 5.9
*/
QList<QObject *> QDeclarativeGeoMap::mapParameters()
{
    QList<QObject *> ret;
    for (QDeclarativeGeoMapParameter *p : qAsConst(m_mapParameters))
        ret << p;
    return ret;
}

/*!
    \qmlproperty list<MapItem> QtLocation::Map::mapItems

    Returns the list of all map items in no particular order.
    These items include items that were declared statically as part of
    the type declaration, as well as dynamical items (\l addMapItem,
    \l MapItemView).

    \sa addMapItem, removeMapItem, clearMapItems
*/

QList<QObject *> QDeclarativeGeoMap::mapItems()
{
    QList<QObject *> ret;
    foreach (const QPointer<QDeclarativeGeoMapItemBase> &ptr, m_mapItems) {
        if (ptr)
            ret << ptr.data();
    }
    return ret;
}

/*!
    \qmlmethod void QtLocation::Map::removeMapItem(MapItem item)

    Removes the given \a item from the Map (for example MapQuickItem, MapCircle). If
    the MapItem does not exist or was not previously added to the map, the
    method does nothing.

    \sa mapItems, addMapItem, clearMapItems
*/
void QDeclarativeGeoMap::removeMapItem(QDeclarativeGeoMapItemBase *ptr)
{
    if (!ptr || !m_map)
        return;
    m_map->removeMapItem(ptr);
    QPointer<QDeclarativeGeoMapItemBase> item(ptr);
    if (!m_mapItems.contains(item))
        return;
    if (item->parentItem() == this)
        item->setParentItem(0);
    item->setMap(0, 0);
    // these can be optimized for perf, as we already check the 'contains' above
    m_mapItems.removeOne(item);
    emit mapItemsChanged();
}

/*!
    \qmlmethod void QtLocation::Map::clearMapItems()

    Removes all items and item groups from the map.

    \sa mapItems, addMapItem, removeMapItem, addMapItemGroup, removeMapItemGroup
*/
void QDeclarativeGeoMap::clearMapItems()
{
    m_map->clearMapItems();
    if (m_mapItems.isEmpty())
        return;
    for (auto i : qAsConst(m_mapItems)) {
        if (i) {
            i->setMap(0, 0);
            if (i->parentItem() == this)
                i->setParentItem(0);
        }
    }
    m_mapItems.clear();
    m_mapItemGroups.clear();
    emit mapItemsChanged();
}

/*!
    \qmlmethod void QtLocation::Map::addMapItemGroup(MapItemGroup itemGroup)

    Adds the map items contained in the given \a itemGroup to the Map
    (for example MapQuickItem, MapCircle).

    \sa MapItemGroup, removeMapItemGroup

    \since 5.9
*/
void QDeclarativeGeoMap::addMapItemGroup(QDeclarativeGeoMapItemGroup *itemGroup)
{
    if (!itemGroup || itemGroup->quickMap()) // || Already added to some map
        return;

    itemGroup->setQuickMap(this);
    QPointer<QDeclarativeGeoMapItemGroup> g(itemGroup);
    m_mapItemGroups.append(g);
    const QList<QQuickItem *> quickKids = g->childItems();
    for (auto c: quickKids) {
        QDeclarativeGeoMapItemBase *mapItem = qobject_cast<QDeclarativeGeoMapItemBase *>(c);
        if (mapItem)
            addMapItem(mapItem);
    }
    itemGroup->setParentItem(this);
}

/*!
    \qmlmethod void QtLocation::Map::removeMapItemGroup(MapItemGroup itemGroup)

    Removes \a itemGroup and the items contained therein from the Map.

    \sa MapItemGroup, addMapItemGroup

    \since 5.9
*/
void QDeclarativeGeoMap::removeMapItemGroup(QDeclarativeGeoMapItemGroup *itemGroup)
{
    if (!itemGroup || itemGroup->quickMap() != this) // cant remove an itemGroup added to another map
        return;

    QPointer<QDeclarativeGeoMapItemGroup> g(itemGroup);
    if (!m_mapItemGroups.removeOne(g))
        return;

    const QList<QQuickItem *> quickKids = itemGroup->childItems();
    for (auto c: quickKids) {
        QDeclarativeGeoMapItemBase *mapItem = qobject_cast<QDeclarativeGeoMapItemBase *>(c);
        if (mapItem)
            removeMapItem(mapItem);
    }
    itemGroup->setQuickMap(nullptr);
    itemGroup->setParentItem(0);
}

/*!
    \qmlproperty MapType QtLocation::Map::activeMapType

    \brief Access to the currently active \l{MapType}{map type}.

    This property can be set to change the active \l{MapType}{map type}.
    See the \l{Map::supportedMapTypes}{supportedMapTypes} property for possible values.

    \sa MapType
*/
void QDeclarativeGeoMap::setActiveMapType(QDeclarativeGeoMapType *mapType)
{
    if (m_activeMapType->mapType() != mapType->mapType()) {
        if (m_map) {
            if (mapType->mapType().pluginName() == m_plugin->name().toLatin1()) {
                m_map->setActiveMapType(mapType->mapType());
                m_activeMapType = mapType;
                emit activeMapTypeChanged();
            }
        } else {
            m_activeMapType = mapType;
            emit activeMapTypeChanged();
        }
    }
}

QDeclarativeGeoMapType * QDeclarativeGeoMap::activeMapType() const
{
    return m_activeMapType;
}

/*!
    \internal
*/
void QDeclarativeGeoMap::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    m_gestureArea->setSize(newGeometry.size());
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (!m_map || newGeometry.size().isEmpty())
        return;

    m_map->setViewportSize(newGeometry.size().toSize());

    if (!m_initialized) {
        initialize();
    } else {
        setMinimumZoomLevel(m_map->minimumZoom(), false);

        // Update the center latitudinal threshold
        double maximumCenterLatitudeAtZoom = m_map->maximumCenterLatitudeAtZoom(m_cameraData);
        if (maximumCenterLatitudeAtZoom != m_maximumViewportLatitude) {
            m_maximumViewportLatitude = maximumCenterLatitudeAtZoom;
            QGeoCoordinate coord = m_cameraData.center();
            coord.setLatitude(qBound(-m_maximumViewportLatitude, coord.latitude(), m_maximumViewportLatitude));

            if (coord != m_cameraData.center()) {
                m_cameraData.setCenter(coord);
                m_map->setCameraData(m_cameraData);
                emit centerChanged(m_cameraData.center());
            }
        }
    }

    /*!
        The fitViewportTo*() functions depend on a valid map geometry.
        If they were called prior to the first resize they cause
        the zoomlevel to jump to 0 (showing the world). Therefore the
        calls were queued up until now.

        Multiple fitViewportTo*() calls replace each other.
     */
    if (m_pendingFitViewport && width() && height()) {
        fitViewportToGeoShape();
        m_pendingFitViewport = false;
    }

}

/*!
    \qmlmethod void QtLocation::Map::fitViewportToMapItems()

    Fits the current viewport to the boundary of all map items. The camera is positioned
    in the center of the map items, and at the largest integral zoom level possible which
    allows all map items to be visible on screen.

    \sa fitViewportToVisibleMapItems
*/
void QDeclarativeGeoMap::fitViewportToMapItems()
{
    fitViewportToMapItemsRefine(true, false);
}

/*!
    \qmlmethod void QtLocation::Map::fitViewportToVisibleMapItems()

    Fits the current viewport to the boundary of all \b visible map items.
    The camera is positioned in the center of the map items, and at the largest integral
    zoom level possible which allows all map items to be visible on screen.

    \sa fitViewportToMapItems
*/
void QDeclarativeGeoMap::fitViewportToVisibleMapItems()
{
    fitViewportToMapItemsRefine(true, true);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::fitViewportToMapItemsRefine(bool refine, bool onlyVisible)
{
    if (!m_map)
        return;

    if (m_mapItems.size() == 0)
        return;

    double minX = 0;
    double maxX = 0;
    double minY = 0;
    double maxY = 0;
    double topLeftX = 0;
    double topLeftY = 0;
    double bottomRightX = 0;
    double bottomRightY = 0;
    bool haveQuickItem = false;

    // find bounds of all map items
    int itemCount = 0;
    for (int i = 0; i < m_mapItems.count(); ++i) {
        if (!m_mapItems.at(i))
            continue;
        QDeclarativeGeoMapItemBase *item = m_mapItems.at(i).data();
        if (!item || (onlyVisible && (!item->isVisible() || item->mapItemOpacity() <= 0.0)))
            continue;

        // skip quick items in the first pass and refine the fit later
        if (refine) {
            QDeclarativeGeoMapQuickItem *quickItem =
                    qobject_cast<QDeclarativeGeoMapQuickItem*>(item);
            if (quickItem) {
                haveQuickItem = true;
                continue;
            }
        }
        // Force map items to update immediately. Needed to ensure correct item size and positions
        // when recursively calling this function.
        // TODO: See if we really need updatePolish on delegated items, in particular
        // in relation to
        // a) fitViewportToMapItems
        // b) presence of MouseArea
        if (item->isPolishScheduled())
           item->updatePolish();

        topLeftX = item->position().x();
        topLeftY = item->position().y();
        bottomRightX = topLeftX + item->width();
        bottomRightY = topLeftY + item->height();

        if (itemCount == 0) {
            minX = topLeftX;
            maxX = bottomRightX;
            minY = topLeftY;
            maxY = bottomRightY;
        } else {
            minX = qMin(minX, topLeftX);
            maxX = qMax(maxX, bottomRightX);
            minY = qMin(minY, topLeftY);
            maxY = qMax(maxY, bottomRightY);
        }
        ++itemCount;
    }

    if (itemCount == 0) {
        if (haveQuickItem)
            fitViewportToMapItemsRefine(false, onlyVisible);
        return;
    }
    double bboxWidth = maxX - minX;
    double bboxHeight = maxY - minY;
    double bboxCenterX = minX + (bboxWidth / 2.0);
    double bboxCenterY = minY + (bboxHeight / 2.0);

    // position camera to the center of bounding box
    QGeoCoordinate coordinate;
    coordinate = m_map->geoProjection().itemPositionToCoordinate(QDoubleVector2D(bboxCenterX, bboxCenterY), false);
    setProperty("center", QVariant::fromValue(coordinate));

    // adjust zoom
    double bboxWidthRatio = bboxWidth / (bboxWidth + bboxHeight);
    double mapWidthRatio = width() / (width() + height());
    double zoomRatio;

    if (bboxWidthRatio > mapWidthRatio)
        zoomRatio = bboxWidth / width();
    else
        zoomRatio = bboxHeight / height();

    qreal newZoom = std::log10(zoomRatio) / std::log10(0.5);
    newZoom = std::floor(qMax(minimumZoomLevel(), (zoomLevel() + newZoom)));
    setProperty("zoomLevel", QVariant::fromValue(newZoom));

    // as map quick items retain the same screen size after the camera zooms in/out
    // we refine the viewport again to achieve better results
    if (refine)
        fitViewportToMapItemsRefine(false, onlyVisible);
}

bool QDeclarativeGeoMap::sendMouseEvent(QMouseEvent *event)
{
    QPointF localPos = mapFromScene(event->windowPos());
    QQuickWindow *win = window();
    QQuickItem *grabber = win ? win->mouseGrabberItem() : 0;
    bool stealEvent = m_gestureArea->isActive();

    if ((stealEvent || contains(localPos)) && (!grabber || (!grabber->keepMouseGrab() && !grabber->keepTouchGrab()))) {
        QScopedPointer<QMouseEvent> mouseEvent(QQuickWindowPrivate::cloneMouseEvent(event, &localPos));
        mouseEvent->setAccepted(false);

        switch (mouseEvent->type()) {
        case QEvent::MouseMove:
            m_gestureArea->handleMouseMoveEvent(mouseEvent.data());
            break;
        case QEvent::MouseButtonPress:
            m_gestureArea->handleMousePressEvent(mouseEvent.data());
            break;
        case QEvent::MouseButtonRelease:
            m_gestureArea->handleMouseReleaseEvent(mouseEvent.data());
            break;
        default:
            break;
        }

        stealEvent = m_gestureArea->isActive();
        grabber = win ? win->mouseGrabberItem() : 0;

        if (grabber && stealEvent && !grabber->keepMouseGrab() && !grabber->keepTouchGrab() && grabber != this)
            grabMouse();

        if (stealEvent) {
            //do not deliver
            event->setAccepted(true);
            return true;
        } else {
            return false;
        }
    }

    return false;
}

bool QDeclarativeGeoMap::sendTouchEvent(QTouchEvent *event)
{
    QQuickPointerDevice *touchDevice = QQuickPointerDevice::touchDevice(event->device());
    const QTouchEvent::TouchPoint &point = event->touchPoints().first();
    QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(window());

    auto touchPointGrabberItem = [touchDevice, windowPriv](const QTouchEvent::TouchPoint &point) -> QQuickItem* {
        if (QQuickEventPoint *eventPointer = windowPriv->pointerEventInstance(touchDevice)->pointById(point.id()))
            return eventPointer->grabber();
        return nullptr;
    };

    QQuickItem *grabber = touchPointGrabberItem(point);

    bool stealEvent = m_gestureArea->isActive();
    bool containsPoint = contains(mapFromScene(point.scenePos()));

    if ((stealEvent || containsPoint) && (!grabber || !grabber->keepTouchGrab())) {
        QScopedPointer<QTouchEvent> touchEvent(new QTouchEvent(event->type(), event->device(), event->modifiers(), event->touchPointStates(), event->touchPoints()));
        touchEvent->setTimestamp(event->timestamp());
        touchEvent->setAccepted(false);

        m_gestureArea->handleTouchEvent(touchEvent.data());
        stealEvent = m_gestureArea->isActive();
        grabber = touchPointGrabberItem(point);

        if (grabber && stealEvent && !grabber->keepTouchGrab() && grabber != this) {
            QVector<int> ids;
            foreach (const QTouchEvent::TouchPoint &tp, event->touchPoints()) {
                if (!(tp.state() & Qt::TouchPointReleased)) {
                    ids.append(tp.id());
                }
            }
            grabTouchPoints(ids);
        }

        if (stealEvent) {
            //do not deliver
            event->setAccepted(true);
            return true;
        } else {
            return false;
        }
    }

    return false;
}

QT_END_NAMESPACE
