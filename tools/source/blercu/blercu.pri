

OTHER_FILES += \
	$$PWD/blercu.pri	

INCLUDEPATH += \
	$$PWD/

HEADERS += \
	$$PWD/blercucontroller1_interface.h \
	$$PWD/blercudevice1_interface.h \
	$$PWD/blercuinfrared1_interface.h \
	$$PWD/blercudebug1_interface.h \
	$$PWD/blercuupgrade1_interface.h \
	$$PWD/blercuhcicapture1_interface.h

SOURCES += \
	$$PWD/blercucontroller1_interface.cpp \
	$$PWD/blercudevice1_interface.cpp \
	$$PWD/blercuinfrared1_interface.cpp \
	$$PWD/blercudebug1_interface.cpp \
	$$PWD/blercuupgrade1_interface.cpp \
	$$PWD/blercuhcicapture1_interface.cpp
