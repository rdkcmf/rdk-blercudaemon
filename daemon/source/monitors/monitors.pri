

OTHER_FILES += \
	$$PWD/monitor.pri	

INCLUDEPATH += \
	$$PWD/

HEADERS += \
	$$PWD/ringbuffer.h \
	$$PWD/hcimonitor.h \
	$$PWD/hcimonitor_p.h \
	$$PWD/hidmonitor.h

SOURCES += \
	$$PWD/ringbuffer.cpp \
	$$PWD/hcimonitor.cpp \
	$$PWD/hidmonitor.cpp
