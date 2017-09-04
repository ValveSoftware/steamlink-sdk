TEMPLATE = subdirs
SUBDIRS = samegame \
            calqlatr \
            clocks \
            tweetsearch \
            maroon \
            photosurface \
            stocqt

qtHaveModule(xmlpatterns): SUBDIRS += rssnews photoviewer

