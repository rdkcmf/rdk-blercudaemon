
HEADERS += \
	$$PWD/logging.h \
	$$PWD/bleuuid.h \
	$$PWD/bleaddress.h \
	$$PWD/bleconnectionparameters.h \
	$$PWD/promise.h \
	$$PWD/future.h \
	$$PWD/futureaggregator.h \
	$$PWD/filedescriptor.h \
	$$PWD/unixpipenotifier.h \
	$$PWD/unixpipesplicer.h \
	$$PWD/unixsignalnotifier.h \
	$$PWD/unixsignalnotifier_p.h \
	$$PWD/statemachine.h \
	$$PWD/voicecodec.h \
	$$PWD/adpcmcodec.h \
	$$PWD/edid.h \
	$$PWD/crc32.h \
	$$PWD/fwimagefile.h \
	$$PWD/linuxinputdevice.h \
	$$PWD/linuxinputdeviceinfo.h \
	$$PWD/inputdevicemanager.h \
	$$PWD/inputdeviceinfo.h

SOURCES += \
	$$PWD/logging.cpp \
	$$PWD/bleuuid.cpp \
	$$PWD/bleaddress.cpp \
	$$PWD/bleconnectionparameters.cpp \
	$$PWD/promise.cpp \
	$$PWD/future.cpp \
	$$PWD/futureaggregator.cpp \
	$$PWD/filedescriptor.cpp \
	$$PWD/unixpipenotifier.cpp \
	$$PWD/unixpipesplicer.cpp \
	$$PWD/unixsignalnotifier.cpp \
	$$PWD/statemachine.cpp \
	$$PWD/adpcmcodec.cpp \
	$$PWD/edid.cpp \
	$$PWD/crc32.cpp \
	$$PWD/fwimagefile.cpp \
	$$PWD/linuxinputdevice.cpp \
	$$PWD/linuxinputdeviceinfo.cpp \
	$$PWD/inputdeviceinfo.cpp


OTHER_FILES += \
	$$PWD/CMakeLists.txt

include( $$PWD/linux/utils_linux.pri )

mac: {

QMAKE_CFLAGS += \
	-D'TEMP_FAILURE_RETRY(x)=x'
QMAKE_CXXFLAGS += \
	-D'TEMP_FAILURE_RETRY(x)=x'

}
