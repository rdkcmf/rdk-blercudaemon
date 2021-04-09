
INCLUDEPATH += \
	$$PWD/

HEADERS += \
	$$PWD/irsignalset.h \
	$$PWD/irdatabase.h \
	$$PWD/irdatabase_p.h \
	$$PWD/qtvfs.h

SOURCES += \
	$$PWD/irsignalset.cpp \
	$$PWD/irdatabase.cpp \
	$$PWD/qtvfs.cpp

DEFINES += \
	QTVFS_USE_QRESOURCE

OTHER_FILES += \
	$$PWD/CMakeLists.txt

