cmake_minimum_required(VERSION 3.16)
project(BTE VERSION 0.1 LANGUAGES CXX)

# Use vcpkg toolchain
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain file")
endif()

# Force CMake to use the vcpkg-installed Qt
set(CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows")
set(Qt6_DIR "C:/vcpkg/installed/x64-windows/share/Qt6")
set(Qt6Charts_DIR "C:/vcpkg/installed/x64-windows/share/Qt6Charts")

# Find Qt6 (including Charts)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Charts Concurrent HINTS "${CMAKE_PREFIX_PATH}/share/Qt6")

# Qt6 setup
qt_standard_project_setup()

if(NOT DEFINED QT_VERSION_MAJOR)
    set(QT_VERSION_MAJOR 6)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find other vcpkg-installed packages
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    authenticate.h
    authenticate.cpp
    fetch_data.cpp
    fetch_data.h
    manage_db.cpp
    manage_db.h
    menu.cpp
    menu.h
    menu.ui
    charting.cpp
    charting.h

)

qt_add_executable(BTE
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    chartingpage.h chartingpage.cpp chartingpage.ui
    backtest_engine.h backtest_engine.cpp backtest_engine.ui
    backtest.h backtest.cpp
)

target_link_libraries(BTE PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
    Qt6::Concurrent
)

target_link_libraries(BTE PRIVATE
    unofficial::sqlite3::sqlite3
    CURL::libcurl
    ZLIB::ZLIB
    OpenSSL::Crypto OpenSSL::SSL
    JsonCpp::JsonCpp
)

set_target_properties(BTE PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS BTE
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(BTE)
endif()
