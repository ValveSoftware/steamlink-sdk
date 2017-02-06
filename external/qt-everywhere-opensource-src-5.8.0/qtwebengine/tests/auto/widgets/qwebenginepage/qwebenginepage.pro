include(../tests.pri)
QT *= core-private gui-private

contains(WEBENGINE_CONFIG, enable_pdf): DEFINES+=QWEBENGINEPAGE_PDFPRINTINGENABLED
