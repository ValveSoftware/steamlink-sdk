TARGET = qml_location_places
TEMPLATE = app

QT += qml quick network
SOURCES += qmlplaceswrapper.cpp

RESOURCES += \
    placeswrapper.qrc

qmlcontent.files += \
    places.qml
OTHER_FILES += $$qmlcontent.files

qmlcontentplaces.files += \
    content/places/PlacesUtils.js \
    content/places/Group.qml \
    content/places/SearchBox.qml \
    content/places/CategoryDelegate.qml \
    content/places/SearchResultDelegate.qml \
    content/places/PlaceDelegate.qml \
    content/places/RatingView.qml \
    content/places/SearchResultView.qml \
    content/places/PlaceDialog.qml \
    content/places/CategoryDialog.qml \
    content/places/PlaceEditorials.qml \
    content/places/EditorialDelegate.qml \
    content/places/EditorialPage.qml \
    content/places/PlaceReviews.qml \
    content/places/ReviewDelegate.qml \
    content/places/ReviewPage.qml \
    content/places/PlaceImages.qml \
    content/places/MapComponent.qml \
    content/places/OptionsDialog.qml \
    content/places/CategoryView.qml

OTHER_FILES += $$qmlcontentplaces.files

include(../common/common.pri)

target.path = $$[QT_INSTALL_EXAMPLES]/location/places
additional.files = ../common
additional.path = $$[QT_INSTALL_EXAMPLES]/location/common
INSTALLS += target additional

