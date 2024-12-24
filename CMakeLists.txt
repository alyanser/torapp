cmake_minimum_required(VERSION 3.18)

project(torapp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD "23")
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(SOURCES
         src/main.cc
         src/main_window.cc
         src/network_manager.cc
         src/url_input_dialog.cc
         src/download_tracker.cc
         src/torrent_metadata_dialog.cc
         src/udp_torrent_client.cc
         src/peer_wire_client.cc
         src/udp_socket.cc
         src/torrent_properties_displayer.cc
         src/tcp_socket.cc
         src/file_allocator.cc
         src/util.cc
)

set(MOC_INCLUDES
         include/main_window.h
         include/url_input_dialog.h
         include/network_manager.h
         include/download_tracker.h
         include/torrent_metadata_dialog.h
         include/peer_wire_client.h
         include/udp_torrent_client.h
         include/udp_socket.h
         include/tcp_socket.h
         include/file_allocator.h
         include/torrent_properties_displayer.h
         src/resources.qrc
)

include(FetchContent)

FetchContent_Declare(
         bencode-parser
         GIT_REPOSITORY "https://github.com/alyanser/bencode-parser.git"
)

FetchContent_MakeAvailable(bencode-parser)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)

add_executable(${PROJECT_NAME} ${SOURCES} ${MOC_INCLUDES})

if (CMAKE_BUILD_TYPE STREQUAL "release")
         add_compile_definitions(QT_NO_DEBUG_OUTPUT)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
         "include"
         "${bencode-parser_SOURCE_DIR}/include"
)

find_package(Qt6 COMPONENTS Network Widgets REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
         Qt6::Network
         Qt6::Widgets
)