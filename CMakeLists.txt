cmake_minimum_required(VERSION 3.20)
project(usim)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(LPAC_DYNAMIC_LIBEUICC ON)
set(LPAC_BUILD_LIBRARY ON)
add_subdirectory(lpac)

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml)
pkg_check_modules(gbinder REQUIRED libgbinder)
pkg_check_modules(libcurl REQUIRED libcurl)
pkg_check_modules(GLIB2 REQUIRED glib-2.0 gio-2.0)

add_executable(usim main.cpp lpacworker.cpp lpacworker.h gbinderapdu.cpp curl.cpp usim.cpp types.h qml.qrc)
target_link_libraries(usim PUBLIC euicc Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml ${gbinder_LIBRARIES} ${libcurl_LIBRARIES} ${GLIB2_LIBRARIES})
target_include_directories(usim PRIVATE ${gbinder_INCLUDE_DIRS} ${libcurl_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS})

configure_file(manifest.json.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.json)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/manifest.json DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${CMAKE_SOURCE_DIR}/usim.apparmor DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${CMAKE_SOURCE_DIR}/usim.desktop DESTINATION ${CMAKE_INSTALL_PREFIX})
