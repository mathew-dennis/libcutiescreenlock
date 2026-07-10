cmake_minimum_required(VERSION 3.16)
project(cutiescreenlock VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml DBus)

find_library(PAM_LIBRARIES pam REQUIRED)
find_path(PAM_INCLUDE_DIR security/pam_appl.h REQUIRED)

qt_standard_project_setup()

# ── Core library ──────────────────────────────────────────────────────────
# Contains only the real PAM/hashing engine (CutieScreenLockAuthority). This
# is what cutie-panel links directly, to construct the single system-wide
# instance in main.cpp. It does NOT contain CutieScreenLock (the D-Bus
# client) - that lives solely in the QML plugin below, so there is only
# ever one compiled copy of it and no risk of cutie-panel accidentally
# using a different CutieScreenLock than the one QML sees.
add_library(cutiescreenlock SHARED
	src/cutiescreenlockauth.h
	src/cutiescreenlockauth.cpp
)

target_include_directories(cutiescreenlock
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	       $<INSTALL_INTERFACE:include/cutiescreenlock>
	PRIVATE ${PAM_INCLUDE_DIR}
)

target_link_libraries(cutiescreenlock PUBLIC Qt::Core Qt::DBus PRIVATE ${PAM_LIBRARIES})

set_target_properties(cutiescreenlock PROPERTIES VERSION 1.0 SOVERSION 1)

install(TARGETS cutiescreenlock LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES src/cutiescreenlockauth.h
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/cutiescreenlock")

# ── QML plugin ────────────────────────────────────────────────────────────
# CutieScreenLock registers itself via the QML_ELEMENT macro in
# cutiescreenlockclient.h (Qt6 automatic registration) - no manual
# plugin.cpp/registerTypes needed. PinPad/PatternLock are plain QML files
# bundled alongside it, laid out under src/qml/Cutie/ScreenLock to mirror
# the eventual install path - same convention as libcutiewaydroid's
# src/qml/Cutie/Waydroid.
qt_add_qml_module(qmlcutiescreenlockplugin
	URI Cutie.ScreenLock
	VERSION 1.0
	PLUGIN_TARGET qmlcutiescreenlockplugin
	SOURCES
		src/cutiescreenlockclient.h
		src/cutiescreenlockclient.cpp
	QML_FILES
		src/qml/Cutie/ScreenLock/PinPad.qml
		src/qml/Cutie/ScreenLock/PatternLock.qml
)

target_link_libraries(qmlcutiescreenlockplugin PRIVATE Qt::Core Qt::Qml Qt::DBus)

install(TARGETS qmlcutiescreenlockplugin
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/qt6/qml/Cutie/ScreenLock")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qmldir"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/qt6/qml/Cutie/ScreenLock")
