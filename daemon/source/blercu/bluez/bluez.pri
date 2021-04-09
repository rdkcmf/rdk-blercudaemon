
HEADERS += \
	$$PWD/blegattprofile_p.h \
	$$PWD/blegattservice_p.h \
	$$PWD/blegattcharacteristic_p.h \
	$$PWD/blegattdescriptor_p.h \
	$$PWD/blegattnotifypipe.h \
	$$PWD/blercuadapter_p.h \
	$$PWD/blercudevice_p.h \
	$$PWD/blercurecovery.h

HEADERS += \
	$$PWD/interfaces/bluezadapterinterface.h \
	$$PWD/interfaces/bluezdeviceinterface.h \
	$$PWD/interfaces/bluezgattcharacteristicinterface.h \
	$$PWD/interfaces/bluezgattdescriptorinterface.h

SOURCES += \
	$$PWD/blegattmanager.cpp \
	$$PWD/blegattprofile.cpp \
	$$PWD/blegattservice.cpp \
	$$PWD/blegattcharacteristic.cpp \
	$$PWD/blegattdescriptor.cpp \
	$$PWD/blegattnotifypipe.cpp \
	$$PWD/blercuadapter.cpp \
	$$PWD/blercudevice.cpp \
	$$PWD/blercurecovery.cpp

OTHER_FILES += \
	$$PWD/CMakeLists.txt

