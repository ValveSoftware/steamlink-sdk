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
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoRectangle>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRectangleNode>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQml/qqmlinfo.h>
#include <cmath>

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
    to use it. The map is centered over Oslo, Norway, with zoom level 10.

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

static const qreal EARTH_MEAN_RADIUS = 6371007.2;

QDeclarativeGeoMap::QDeclarativeGeoMap(QQuickItem *parent)
        : QQuickItem(parent),
        m_plugin(0),
        m_serviceProvider(0),
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
        m_validRegion(false)
{
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlags(QQuickItem::ItemHasContents | QQuickItem::ItemClipsChildrenToShape);
    setFiltersChildMouseEvents(true);

    connect(this, SIGNAL(childrenChanged()), this, SLOT(onMapChildrenChanged()), Qt::QueuedConnection);

    m_activeMapType = new QDeclarativeGeoMapType(QGeoMapType(QGeoMapType::NoMap,
                                                             tr("No Map"),
                                                             tr("No Map"), false, false, 0), this);
    m_cameraData.setCenter(QGeoCoordinate(51.5073,-0.1277)); //London city center
    m_cameraData.setZoomLevel(8.0);
}

QDeclarativeGeoMap::~QDeclarativeGeoMap()
{
    if (!m_mapViews.isEmpty())
        qDeleteAll(m_mapViews);
    // remove any map items associations
    for (int i = 0; i < m_mapItems.count(); ++i) {
        if (m_mapItems.at(i))
            m_mapItems.at(i).data()->setMap(0,0);
    }
    m_mapItems.clear();

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
            copyrights = m_copyrights.data();
            connect(m_map, SIGNAL(copyrightsChanged(QImage)),
                    copyrights, SLOT(copyrightsChanged(QImage)));
            connect(m_map, SIGNAL(copyrightsChanged(QString)),
                    copyrights, SLOT(copyrightsChanged(QString)));
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

void QDeclarativeGeoMap::initialize()
{
    // try to keep center change signal in the end
    bool centerHasChanged = false;

    setMinimumZoomLevel(m_map->minimumZoomAtViewportSize(width(), height()));

    // set latitude bundary check
    m_maximumViewportLatitude = m_map->maximumCenterLatitudeAtZoom(m_cameraData.zoomLevel());
    QGeoCoordinate center = m_cameraData.center();
    center.setLatitude(qBound(-m_maximumViewportLatitude, center.latitude(), m_maximumViewportLatitude));

    if (center != m_cameraData.center()) {
        centerHasChanged = true;
        m_cameraData.setCenter(center);
    }

    m_map->setCameraData(m_cameraData);

    m_initialized = true;

    if (centerHasChanged)
        emit centerChanged(m_cameraData.center());
}

/*!
    \internal
*/
void QDeclarativeGeoMap::pluginReady()
{
    m_serviceProvider = m_plugin->sharedGeoServiceProvider();
    m_mappingManager = m_serviceProvider->mappingManager();

    if (m_serviceProvider->error() != QGeoServiceProvider::NoError) {
        setError(m_serviceProvider->error(), m_serviceProvider->errorString());
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
        }
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
        qmlInfo(this) << QStringLiteral("Plugin is a write-once property, and cannot be set again.");
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
    this function will only be ever called once
*/
void QDeclarativeGeoMap::mappingManagerInitialized()
{
    m_map = m_mappingManager->createMap(this);

    if (!m_map)
        return;

    m_gestureArea->setMap(m_map);

    QList<QGeoMapType> types = m_mappingManager->supportedMapTypes();
    for (int i = 0; i < types.size(); ++i) {
        QDeclarativeGeoMapType *type = new QDeclarativeGeoMapType(types[i], this);
        m_supportedMapTypes.append(type);
    }

    if (!m_supportedMapTypes.isEmpty()) {
        QDeclarativeGeoMapType *type = m_supportedMapTypes.at(0);
        m_activeMapType = type;
        m_map->setActiveMapType(type->mapType());
    } else {
        m_map->setActiveMapType(m_activeMapType->mapType());
    }

    //The zoom level limits are only restricted by the plugins values, if the user has set a more
    //strict zoom level limit before initialization nothing is done here.
    //minimum zoom level might be changed to limit gray bundaries

    if (m_gestureArea->maximumZoomLevel() < 0
            || m_mappingManager->cameraCapabilities().maximumZoomLevel() < m_gestureArea->maximumZoomLevel())
        setMaximumZoomLevel(m_mappingManager->cameraCapabilities().maximumZoomLevel());

    if (m_mappingManager->cameraCapabilities().minimumZoomLevel() > m_gestureArea->minimumZoomLevel())
        setMinimumZoomLevel(m_mappingManager->cameraCapabilities().minimumZoomLevel());


    // Map tiles are built in this call. m_map->minimumZoom() becomes operational
    // after this has been called at least once, after creation.

    if (!m_initialized && width() > 0 && height() > 0) {
        m_map->setViewportSize(QSize(width(), height()));
        initialize();
    }

    m_copyrights = new QDeclarativeGeoMapCopyrightNotice(this);
    connect(m_map, SIGNAL(copyrightsChanged(QImage)),
            m_copyrights.data(), SLOT(copyrightsChanged(QImage)));
    connect(m_map, SIGNAL(copyrightsChanged(QString)),
            m_copyrights.data(), SLOT(copyrightsChanged(QString)));
    connect(m_copyrights.data(), SIGNAL(linkActivated(QString)),
            this, SIGNAL(copyrightLinkActivated(QString)));
    connect(m_map, &QGeoMap::sgNodeChanged, this, &QQuickItem::update);

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
    foreach (const QPointer<QDeclarativeGeoMapItemBase> &item, m_mapItems) {
        if (item)
            item.data()->setMap(this, m_map);
    }
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
void QDeclarativeGeoMap::setMinimumZoomLevel(qreal minimumZoomLevel)
{

    if (minimumZoomLevel >= 0) {
        qreal oldMinimumZoomLevel = this->minimumZoomLevel();

        if (m_map) {
            minimumZoomLevel = qBound(qreal(m_map->cameraCapabilities().minimumZoomLevel()), minimumZoomLevel, maximumZoomLevel());
            double minimumViewportZoomLevel = m_map->minimumZoomAtViewportSize(width(),height());
            if (minimumZoomLevel < minimumViewportZoomLevel)
                minimumZoomLevel = minimumViewportZoomLevel;
        }

        m_gestureArea->setMinimumZoomLevel(minimumZoomLevel);

        if (zoomLevel() < minimumZoomLevel)
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

    If a plugin supporting mapping is not set, -1.0 is returned.
*/

qreal QDeclarativeGeoMap::minimumZoomLevel() const
{
    if (m_gestureArea->minimumZoomLevel() != -1)
        return m_gestureArea->minimumZoomLevel();
    else if (m_map)
        return m_map->cameraCapabilities().minimumZoomLevel();
    else
        return -1.0;
}

/*!
    \internal
    Sets the gesture areas maximum zoom level. If the camera capabilities
    has been set this method honors the boundaries set by it.
*/
void QDeclarativeGeoMap::setMaximumZoomLevel(qreal maximumZoomLevel)
{
    if (maximumZoomLevel >= 0) {
        qreal oldMaximumZoomLevel = this->maximumZoomLevel();

        if (m_map)
            maximumZoomLevel = qBound(minimumZoomLevel(), maximumZoomLevel, qreal(m_map->cameraCapabilities().maximumZoomLevel()));

        m_gestureArea->setMaximumZoomLevel(maximumZoomLevel);

        if (zoomLevel() > maximumZoomLevel)
            setZoomLevel(maximumZoomLevel);

        if (oldMaximumZoomLevel != maximumZoomLevel)
            emit maximumZoomLevelChanged();
    }
}

/*!
    \qmlproperty real QtLocation::Map::maximumZoomLevel

    This property holds the maximum valid zoom level for the map.

    The maximum zoom level is defined by the \l plugin used.
    If a plugin supporting mapping is not set, -1.0 is returned.
*/

qreal QDeclarativeGeoMap::maximumZoomLevel() const
{
    if (m_gestureArea->maximumZoomLevel() != -1)
        return m_gestureArea->maximumZoomLevel();
    else if (m_map)
        return m_map->cameraCapabilities().maximumZoomLevel();
    else
        return -1.0;
}

/*!
    \qmlproperty real QtLocation::Map::zoomLevel

    This property holds the zoom level for the map.

    Larger values for the zoom level provide more detail. Zoom levels
    are always non-negative. The default value is 8.0.
*/
void QDeclarativeGeoMap::setZoomLevel(qreal zoomLevel)
{
    if (m_cameraData.zoomLevel() == zoomLevel || zoomLevel < 0)
        return;

    //small optiomatization to avoid double setCameraData
    bool centerHasChanged = false;

    if (m_initialized) {
        m_cameraData.setZoomLevel(qBound(minimumZoomLevel(), zoomLevel, maximumZoomLevel()));
        m_maximumViewportLatitude = m_map->maximumCenterLatitudeAtZoom(m_cameraData.zoomLevel());
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

    m_validRegion = false;

    if (centerHasChanged)
        emit centerChanged(m_cameraData.center());
    emit zoomLevelChanged(m_cameraData.zoomLevel());
}

qreal QDeclarativeGeoMap::zoomLevel() const
{
    return m_cameraData.zoomLevel();
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

    m_validRegion = false;
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
    if (shape == m_region && m_validRegion)
        return;

    m_region = shape;
    if (!shape.isValid()) {
        // shape invalidated -> nothing to fit anymore
        m_pendingFitViewport = false;
        return;
    }

    if (!width() || !height()) {
        m_pendingFitViewport = true;
        return;
    }

    fitViewportToGeoShape();
}

QGeoShape QDeclarativeGeoMap::visibleRegion() const
{
    if (!m_map || !width() || !height())
        return m_region;

    QGeoCoordinate tl = m_map->itemPositionToCoordinate(QDoubleVector2D(0, 0));
    QGeoCoordinate br = m_map->itemPositionToCoordinate(QDoubleVector2D(width(), height()));

    return QGeoRectangle(tl, br);
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

void QDeclarativeGeoMap::fitViewportToGeoShape()
{
    int margins  = 10;
    if (!m_map || width() <= margins || height() <= margins)
        return;

    QGeoCoordinate topLeft;
    QGeoCoordinate bottomRight;

    switch (m_region.type()) {
    case QGeoShape::RectangleType:
    {
        QGeoRectangle rect = m_region;
        topLeft = rect.topLeft();
        bottomRight = rect.bottomRight();
        if (bottomRight.longitude() < topLeft.longitude())
            bottomRight.setLongitude(bottomRight.longitude() + 360.0);

        break;
    }
    case QGeoShape::CircleType:
    {
        const double pi = M_PI;
        QGeoCircle circle = m_region;
        QGeoCoordinate centerCoordinate = circle.center();

        // calculate geo bounding box of the circle
        // circle tangential points with meridians and the north pole create
        // spherical triangle, we use spherical law of sines
        // sin(lon_delta_in_rad)/sin(r_in_rad) =
        // sin(alpha_in_rad)/sin(pi/2 - lat_in_rad), where:
        // * lon_delta_in_rad - delta of longitudes of circle center
        //   and tangential points
        // * r_in_rad - angular radius of the circle
        // * lat_in_rad - latitude of circle center
        // * alpha_in_rad - angle between meridian and radius to the circle =>
        //   this is tangential point => sin(alpha) = 1
        // * lat_delta_in_rad - delta of latitudes of circle center and
        //   latitude of points where great circle (going through circle
        //   center) crosses circle and the pole

        double r_in_rad = circle.radius() / EARTH_MEAN_RADIUS; // angular r
        double lat_delta_in_deg = r_in_rad * 180 / pi;
        double lon_delta_in_deg = std::asin(std::sin(r_in_rad) /
               std::cos(centerCoordinate.latitude() * pi / 180)) * 180 / pi;

        topLeft.setLatitude(centerCoordinate.latitude() + lat_delta_in_deg);
        topLeft.setLongitude(centerCoordinate.longitude() - lon_delta_in_deg);
        bottomRight.setLatitude(centerCoordinate.latitude()
                                - lat_delta_in_deg);
        bottomRight.setLongitude(centerCoordinate.longitude()
                                 + lon_delta_in_deg);

        // adjust if circle reaches poles => cross all meridians and
        // fit into Mercator projection bounds
        if (topLeft.latitude() > 90 || bottomRight.latitude() < -90) {
            topLeft.setLatitude(qMin(topLeft.latitude(), 85.05113));
            topLeft.setLongitude(-180.0);
            bottomRight.setLatitude(qMax(bottomRight.latitude(),
                                                 -85.05113));
            bottomRight.setLongitude(180.0);
        }
        break;
    }
    case QGeoShape::UnknownType:
        //Fallthrough to default
    default:
        return;
    }

    // adjust zoom, use reference world to keep things simple
    // otherwise we would need to do the error prone longitudes
    // wrapping
    QDoubleVector2D topLeftPoint =
            m_map->referenceCoordinateToItemPosition(topLeft);
    QDoubleVector2D bottomRightPoint =
            m_map->referenceCoordinateToItemPosition(bottomRight);

    double bboxWidth = bottomRightPoint.x() - topLeftPoint.x();
    double bboxHeight = bottomRightPoint.y() - topLeftPoint.y();

    // find center of the bounding box
    QGeoCoordinate centerCoordinate =
            m_map->referenceItemPositionToCoordinate(
                (topLeftPoint + bottomRightPoint)/2);

    // position camera to the center of bounding box
    setCenter(centerCoordinate);

    // if the shape is empty we just change center position, not zoom
    if (bboxHeight == 0 && bboxWidth == 0)
        return;

    double zoomRatio = qMax(bboxWidth / (width() - margins),
                            bboxHeight / (height() - margins));
    // fixme: use log2 with c++11
    zoomRatio = std::log(zoomRatio) / std::log(2.0);
    double newZoom = qMax<double>(minimumZoomLevel(), zoomLevel() - zoomRatio);
    setZoomLevel(newZoom);
    m_validRegion = true;
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
        return m_map->itemPositionToCoordinate(QDoubleVector2D(position), clipToViewPort);
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
        return m_map->coordinateToItemPosition(coordinate, clipToViewPort).toPointF();
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
    QGeoCoordinate coord = m_map->itemPositionToCoordinate(
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
    item->setParentItem(this);
    if (m_map)
        item->setMap(this, m_map);
    m_mapItems.append(item);
    emit mapItemsChanged();
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
    QPointer<QDeclarativeGeoMapItemBase> item(ptr);
    if (!m_mapItems.contains(item))
        return;
    item.data()->setParentItem(0);
    item.data()->setMap(0, 0);
    // these can be optimized for perf, as we already check the 'contains' above
    m_mapItems.removeOne(item);
    emit mapItemsChanged();
}

/*!
    \qmlmethod void QtLocation::Map::clearMapItems()

    Removes all items from the map.

    \sa mapItems, addMapItem, removeMapItem
*/
void QDeclarativeGeoMap::clearMapItems()
{
    if (m_mapItems.isEmpty())
        return;
    for (int i = 0; i < m_mapItems.count(); ++i) {
        if (m_mapItems.at(i)) {
            m_mapItems.at(i).data()->setParentItem(0);
            m_mapItems.at(i).data()->setMap(0, 0);
        }
    }
    m_mapItems.clear();
    emit mapItemsChanged();
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
        m_activeMapType = mapType;
        if (m_map)
            m_map->setActiveMapType(mapType->mapType());
        emit activeMapTypeChanged();
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

    if (!m_map || !newGeometry.size().isValid())
        return;

    m_map->setViewportSize(newGeometry.size().toSize());

    if (!m_initialized)
        initialize();
    else
        setMinimumZoomLevel(m_map->minimumZoomAtViewportSize(newGeometry.width(), newGeometry.height()));

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
    allows all map items to be visible on screen

*/
void QDeclarativeGeoMap::fitViewportToMapItems()
{
    fitViewportToMapItemsRefine(true);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::fitViewportToMapItemsRefine(bool refine)
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
        if (!item)
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
            fitViewportToMapItemsRefine(false);
        return;
    }
    double bboxWidth = maxX - minX;
    double bboxHeight = maxY - minY;
    double bboxCenterX = minX + (bboxWidth / 2.0);
    double bboxCenterY = minY + (bboxHeight / 2.0);

    // position camera to the center of bounding box
    QGeoCoordinate coordinate;
    coordinate = m_map->itemPositionToCoordinate(QDoubleVector2D(bboxCenterX, bboxCenterY), false);
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
        fitViewportToMapItemsRefine(false);
}

bool QDeclarativeGeoMap::sendMouseEvent(QMouseEvent *event)
{
    QPointF localPos = mapFromScene(event->windowPos());
    QQuickWindow *win = window();
    QQuickItem *grabber = win ? win->mouseGrabberItem() : 0;
    bool stealEvent = m_gestureArea->isActive();

    if ((stealEvent || contains(localPos)) && (!grabber || !grabber->keepMouseGrab())) {
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

        if (grabber && stealEvent && !grabber->keepMouseGrab() && grabber != this)
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
    const QQuickPointerDevice *touchDevice = QQuickPointerDevice::touchDevice(event->device());
    const QTouchEvent::TouchPoint &point = event->touchPoints().first();

    auto touchPointGrabberItem = [touchDevice](const QTouchEvent::TouchPoint &point) -> QQuickItem* {
        if (QQuickEventPoint *eventPointer = touchDevice->pointerEvent()->pointById(point.id()))
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

#include "moc_qdeclarativegeomap_p.cpp"

QT_END_NAMESPACE
