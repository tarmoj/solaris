QT = websockets

TARGET = solarisserver
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    main.cpp \
    solarisserver.cpp

HEADERS += \
    solarisserver.h

EXAMPLE_FILES += sslechoclient.html

RESOURCES += securesocketclient.qrc

