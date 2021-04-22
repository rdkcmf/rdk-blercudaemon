

HEADERS += \
	$$PWD/blercuservices.h \
	$$PWD/blercuservicesfactory.h \
	$$PWD/blercuaudioservice.h \
	$$PWD/blercubatteryservice.h \
	$$PWD/blercudeviceinfoservice.h \
	$$PWD/blercufindmeservice.h \
	$$PWD/blercuinfraredservice.h \
	$$PWD/blercutouchservice.h \
	$$PWD/blercuupgradeservice.h \
	$$PWD/blercuremotecontrolservice.h

HEADERS += \
	$$PWD/blercuservicesfactory.h

SOURCES += \
	$$PWD/blercuservicesfactory.cpp

OTHER_FILES += \
	$$PWD/CMakeLists.txt


include( $$PWD/gatt/gatt_services.pri )

