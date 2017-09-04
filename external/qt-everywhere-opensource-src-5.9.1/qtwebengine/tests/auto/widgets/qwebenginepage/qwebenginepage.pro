include(../tests.pri)
QT *= core-private

contains(WEBENGINE_CONFIG, use_pdf): DEFINES+=QWEBENGINEPAGE_PDFPRINTINGENABLED
