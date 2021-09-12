cmake_minimum_required(VERSION 3.21)

project(torapp)

set(CMAKE_CXX_COMPILER "clang++-13")
set(CMAKE_CXX_STANDARD "17")
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CXX_FLAGS "-stdlib=libc++ -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Winit-self -Wmissing-declarations -Wold-style-cast  -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wswitch-default -Wundef")

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -fsanitize=undefined,address ${CXX_FLAGS}")

set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CXX_FLAGS}")

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTOUIC on)
set(CMAKE_AURORCC on)

set(SOURCES
         src/main.cxx
         src/main_window.cxx
         src/network_manager.cxx
)

set(MOC_INCLUDES
         include/main_window.hxx
         include/custom_download_widget.hxx
         include/network_manager.hxx
)

add_executable(${PROJECT_NAME} ${SOURCES} ${MOC_INCLUDES})

include_directories(
         "include"
         "ext"
)

find_package(Qt6 COMPONENTS Core Gui Network Widgets REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
         Qt6::Core
         Qt6::Gui
         Qt6::Network
         Qt6::Widgets
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
         [["QApplication"]]
         [["QMainWindow"]]
         [["QWidget"]]
         [["QNetworkAccessManager"]]
)