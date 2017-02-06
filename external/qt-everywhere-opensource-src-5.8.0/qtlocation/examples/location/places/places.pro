TARGET = qml_location_places
TEMPLATE = app

QT += qml quick network positioning location
SOURCES += main.cpp

RESOURCES += \
    places.qrc

OTHER_FILES += \
    places.qml \
    helper.js \
    items/MainMenu.qml \
    items/SearchBar.qml \
    items/MapComponent.qml \
    forms/Message.qml \
    forms/MessageForm.ui.qml \
    forms/SearchCenter.qml \
    forms/SearchCenterForm.ui.qml \
    forms/SearchBoundingBox.qml \
    forms/SearchBoundingBoxForm.ui.qml \
    forms/SearchBoundingCircle.qml \
    forms/SearchBoundingCircleForm.ui.qml \
    forms/PlaceDetails.qml \
    forms/PlaceDetailsForm.ui.qml \
    forms/SearchOptions.qml \
    forms/SearchOptionsForm.ui.qml \
    views/SuggestionView.qml \
    views/RatingView.qml \
    views/CategoryView.qml \
    views/CategoryDelegate.qml \
    views/SearchResultDelegate.qml \
    views/SearchResultView.qml \
    views/EditorialView.qml \
    views/EditorialDelegate.qml \
    views/EditorialPage.qml \
    views/ReviewView.qml \
    views/ReviewDelegate.qml \
    views/ReviewPage.qml \
    views/ImageView.qml

target.path = $$[QT_INSTALL_EXAMPLES]/location/places
INSTALLS += target

