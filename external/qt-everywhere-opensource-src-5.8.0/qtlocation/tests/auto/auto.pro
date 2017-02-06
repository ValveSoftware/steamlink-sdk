TEMPLATE = subdirs

qtHaveModule(location) {

    #Place unit tests
    SUBDIRS += qplace \
           qplaceattribute \
           qplacecategory \
           qplacecontactdetail \
           qplacecontentrequest \
           qplacedetailsreply \
           qplaceeditorial \
           qplacematchreply \
           qplacematchrequest \
           qplaceimage \
           qplaceratings \
           qplaceresult \
           qproposedsearchresult \
           qplacereply \
           qplacereview \
           qplacesearchrequest \
           qplacesupplier \
           qplacesearchresult \
           qplacesearchreply \
           qplacesearchsuggestionreply \
           qplaceuser \
           qplacemanager \
           qplacemanager_nokia \
           qplacemanager_unsupported \
           placesplugin_unsupported

    #misc tests
    SUBDIRS +=  qmlinterface \
           cmake \
           doublevectors

    #Map and Navigation tests
    SUBDIRS += geotestplugin \
           qgeocodingmanagerplugins \
           qgeocameracapabilities\
           qgeocameradata \
           qgeocodereply \
           qgeocodingmanager \
           qgeomaneuver \
           qgeotiledmapscene \
           qgeoroute \
           qgeoroutereply \
           qgeorouterequest \
           qgeoroutesegment \
           qgeoroutingmanager \
           qgeoroutingmanagerplugins \
           qgeoserviceprovider \
           qgeotiledmap \
           qgeotilespec \
           qgeoroutexmlparser \
           maptype \
           nokia_services \
           qgeocameratiles

    qtHaveModule(quick) {
        SUBDIRS += declarative_core \
                declarative_geoshape

        !mac: SUBDIRS += declarative_ui
    }
}


SUBDIRS += \
           positionplugin \
           positionplugintest \
           qgeoaddress \
           qgeoareamonitor \
           qgeoshape \
           qgeorectangle \
           qgeocircle \
           qgeocoordinate \
           qgeolocation \
           qgeopositioninfo \
           qgeopositioninfosource \
           qgeosatelliteinfo \
           qgeosatelliteinfosource \
           qnmeapositioninfosource
