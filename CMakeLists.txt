cmake_minimum_required(VERSION 3.21)

project(torapp)

set(CMAKE_CXX_COMPILER "clang++-13")
set(CMAKE_CXX_STANDARD "17")
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CXX_FLAGS "-pipe -pedantic -Wall -Wextra -Wshadow -Wdouble-promotion -Wcast-align -Wconversion -Wfloat-equal -Wunreachable-code")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -fsanitize=undefined,address -fno-omit-frame-pointer ${CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG ${CXX_FLAGS}")

set(CMAKE_AUTOMOC on)

set(SOURCES
         src/main.cxx
         src/main_window.cxx
         src/network_manager.cxx
         src/url_input_widget.cxx
         src/download_tracker.cxx
)

set(MOC_INCLUDES
         include/main_window.hxx
         include/url_input_widget.hxx
         include/network_manager.hxx
         include/download_tracker.hxx
)

add_executable(${PROJECT_NAME} ${SOURCES} ${MOC_INCLUDES})

include_directories(
         "include"
)

find_package(Qt6 COMPONENTS Core Network Widgets REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
         Qt6::Core
         Qt6::Network
         Qt6::Widgets
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
         [["QApplication"]]
         [["QMainWindow"]]
         [["QWidget"]]
         [["QNetworkAccessManager"]]
         [["QDebug"]]
         [["QString"]]
         [["QObject"]]
)