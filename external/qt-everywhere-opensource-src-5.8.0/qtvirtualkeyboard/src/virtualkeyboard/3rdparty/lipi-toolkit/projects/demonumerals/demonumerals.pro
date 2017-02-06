TEMPLATE = aux

cfg.files = config
cfg.path = $$[QT_INSTALL_DATA]/qtvirtualkeyboard/lipi_toolkit/projects/demonumerals
INSTALLS += cfg

!prefix_build: COPIES += cfg
