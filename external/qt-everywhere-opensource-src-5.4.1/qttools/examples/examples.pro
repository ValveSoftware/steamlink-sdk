TEMPLATE = subdirs
qtHaveModule(widgets): SUBDIRS += help designer linguist uitools assistant

winrt: SUBDIRS -= assistant designer
