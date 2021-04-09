

OTHER_FILES += \
	$$PWD/utils.pri	

INCLUDEPATH += \
	$$PWD/

HEADERS += \
	$$PWD/unixsignalnotifier.h \
	$$PWD/unixsignalnotifier_p.h \
	$$PWD/bleaddress.h \
	$$PWD/audiowavfile.h


SOURCES += \
	$$PWD/unixsignalnotifier.cpp \
	$$PWD/bleaddress.cpp \
	$$PWD/audiowavfile.cpp

mac: {

	QMAKE_CFLAGS += \
		-D'TEMP_FAILURE_RETRY(x)=x'
	QMAKE_CXXFLAGS += \
		-D'TEMP_FAILURE_RETRY(x)=x'
	
}
