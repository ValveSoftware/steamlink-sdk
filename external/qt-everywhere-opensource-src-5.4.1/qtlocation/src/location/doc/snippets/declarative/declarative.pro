TEMPLATE = aux

content.files = \
    maps.qml \
    places.qml \
    plugin.qml \
    routing.qml \
    places_loader.qml

OTHER_FILES = \
    $${content.files}

# Put content in INSTALLS so that content.files become part of the project tree in Qt Creator,
# but scoped with false as we don't actually want to install them.  They are documentation
# snippets.
false:INSTALLS += content

