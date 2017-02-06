TEMPLATE = aux

cfg.files += lipiengine.cfg
cfg.path = $$[QT_INSTALL_DATA]/qtvirtualkeyboard/lipi_toolkit/projects
INSTALLS += cfg

!prefix_build: COPIES += cfg
