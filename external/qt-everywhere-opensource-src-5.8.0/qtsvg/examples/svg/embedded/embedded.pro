TEMPLATE = subdirs
qtHaveModule(widgets): SUBDIRS += desktopservices fluidlauncher weatherinfo

# Disable platforms without process support
uikit|winrt: SUBDIRS -= fluidlauncher
