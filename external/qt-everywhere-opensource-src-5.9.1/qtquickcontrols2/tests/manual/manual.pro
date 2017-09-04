TEMPLATE = subdirs
SUBDIRS += \
    buttons \
    gifs \
    fonts \
    screenshots \
    styles \
    testbench

qtHaveModule(widgets): SUBDIRS += viewinqwidget
