
INCLUDEPATH += \
	$$PWD/

HEADERS += \
	$$PWD/blegattprofile.h \
	$$PWD/blegattservice.h \
	$$PWD/blegattcharacteristic.h \
	$$PWD/blegattdescriptor.h \
	$$PWD/blercucontroller.h \
	$$PWD/blercucontroller_p.h \
	$$PWD/blercuanalytics.h \
	$$PWD/blercuadapter.h

HEADERS += \
	$$PWD/blercuerror.h \
	$$PWD/blercudevice.h \
	$$PWD/blercupairingstatemachine.h \
	$$PWD/blercuscannerstatemachine.h

SOURCES += \
	$$PWD/blercuerror.cpp \
	$$PWD/blercupairingstatemachine.cpp \
	$$PWD/blercuscannerstatemachine.cpp \
	$$PWD/blercucontroller.cpp \
	$$PWD/blercuanalytics.cpp

OTHER_FILES += \
	$$PWD/CMakeLists.txt


include( services/services.pri )
include( bluez/bluez.pri )


