#
# Filename: xcite.pro
#
# XCITE is a secure platform utilizing the XTRABYTES Proof of Signature
# blockchain protocol to host decentralized applications
#
# Copyright (c) 2017-2018 Zoltan Szabo & XTRABYTES developers
#
# This file is part of an XTRABYTES Ltd. project.
#

VERSION_MAJOR=0
VERSION_MINOR=3
VERSION_BUILD=0

VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}.$${VERSION_BUILD}

QT	+= core gui xml quick svg charts sql
CONFIG  += c++11 qzxing_multimedia qzxing_qml
CONFIG += resources_big
CONFIG += staticlib
CONFIG += static

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" \
    "VERSION_MINOR=$$VERSION_MINOR" \
    "VERSION_BUILD=$$VERSION_BUILD"

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = xcite
TEMPLATE = app

CONFIG(debug, debug|release) {
    DESTDIR = build/debug
}
CONFIG(release, debug|release) {
    DESTDIR = build/release
}

OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.qrc
UI_DIR = $$DESTDIR/.u

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

include(backend/support/QZXing/QZXing.pri)
include(frontend/support/SortFilterProxyModel/SortFilterProxyModel.pri)

INCLUDEPATH += $$PWD/3rdparty
INCLUDEPATH += $$PWD/3rdparty/include

SOURCES += main/main.cpp \
	    backend/xchat/xchat.cpp \
	    backend/xchat/xchataiml.cpp \
	    backend/staticnet/staticnet.cpp \
	    backend/xutility/xutility.cpp \
	    backend/xutility/crypto/ctools.cpp \
	    backend/xutility/transaction/transaction.cpp \
            backend/p2p/p2p.cpp \
            backend/xchat/xchatconversationmodel.cpp \
            backend/XCITE/nodes/nodetransaction.cpp \
            backend/testnet/testnet.cpp \
            backend/testnet/transactionmodel.cpp \
            backend/addressbook/addressbookmodel.cpp \
            backend/support/ClipboardProxy.cpp \
            backend/support/globaleventfilter.cpp \
            backend/testnet/xchattestnetclient.cpp \
            backend/integrations/MarketValue.cpp \
            backend/integrations/Explorer.cpp \
            backend/support/ReleaseChecker.cpp \
            backend/support/FileDownloader.cpp \
            backend/support/Settings.cpp \
            backend/support/qaesencryption.cpp \
    backend/integrations/xutility_integration.cpp \
    backend/integrations/staticnet_integration.cpp

RESOURCES += resources/resources.qrc
RESOURCES += frontend/frontend.qrc

HEADERS  += backend/xchat/xchat.hpp \
	    backend/xchat/xchataiml.hpp \
	    backend/xutility/crypto/allocators.h \
	    backend/xutility/crypto/ctools.h \
	    backend/xutility/crypto/numbers.h \
	    backend/xutility/transaction/transaction.h \
	    backend/xutility/transaction/serialize.h \
            backend/p2p/p2p.hpp \
            backend/xchat/xchatconversationmodel.hpp \
            backend/XCITE/nodes/nodetransaction.h \
	    backend/staticnet/staticnet.hpp \
	    backend/xutility/xutility.hpp \
            backend/testnet/testnet.hpp \
            backend/testnet/transactionmodel.hpp \
            backend/addressbook/addressbookmodel.hpp \
            backend/support/ClipboardProxy.hpp \
            backend/support/globaleventfilter.hpp \
            backend/testnet/xchattestnetclient.hpp \
            backend/integrations/MarketValue.hpp \
            backend/support/ReleaseChecker.hpp \
            backend/support/FileDownloader.hpp \
            backend/support/Settings.hpp \
            backend/integrations/MarketValue.hpp \
            backend/integrations/Explorer.hpp \
            backend/support/qaesencryption.h \
    backend/integrations/xutility_integration.hpp \
    backend/integrations/staticnet_integration.hpp

DISTFILES += \
    xcite.ico \
    packages/global.xtrabytes.xcite/meta/package.xml \
    config/config.xml \
    packages/global.xtrabytes.xcite/meta/installscript.qs \
    packages/global.xtrabytes.xcite/meta/license.txt \
    config/banner.png \
    config/logo.png

RC_ICONS = xcite.ico
CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/build/debug
} else {
    DESTDIR = $$PWD/build/release
}

win32 {
    # Copy OpenSSL DDLs into the build dir on Windows
    DESTDIR_WIN = $${DESTDIR}
    DESTDIR_WIN ~= s,/,\\,g
    PWD_WIN = $${PWD}
    PWD_WIN ~= s,/,\\,g
    QMAKE_POST_LINK += $$quote(cmd /c copy /y \"$${PWD_WIN}\\support\\*.dll\" \"$${DESTDIR_WIN}\")
}

mac {
    ICON = $$PWD/resources/ios/xcite.icns

    mac!ios {
    }

    ios {
        QT += multimedia
        xcode_product_bundle_identifier_setting.value = "global.xtrabytes.xcite"
        QMAKE_INFO_PLIST = resources/ios/Info.plist
        app_launch_images.files = resources/ios/LaunchScreen.storyboard resources/backgrounds/launchscreen-logo.png
        QMAKE_BUNDLE_DATA += app_launch_images
    }
}

linux:!android {
  LIBS += -lssl -lcrypto -lev
  LIBS += -lboost_system
  LIBS += -lPocoFoundation -lPocoUtil -lPocoNet
  LIBS += -lamqpcpp 
}

android {
    QT += multimedia
}

FORMS += \
    packages/global.xtrabytes.xcite/meta/feedbackpage.ui
