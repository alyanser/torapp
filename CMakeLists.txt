cmake_minimum_required(VERSION 3.21)

project(torapp CXX)

set(CMAKE_CXX_STANDARD "17")
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(SOURCES
         src/main.cxx
         src/main_window.cxx
         src/network_manager.cxx
         src/url_input_dialog.cxx
         src/download_tracker.cxx
         src/torrent_metadata_dialog.cxx
         src/udp_torrent_client.cxx
         src/peer_wire_client.cxx
         src/udp_socket.cxx
)

set(MOC_INCLUDES
         include/main_window.hxx
         include/url_input_dialog.hxx
         include/network_manager.hxx
         include/download_tracker.hxx
         include/torrent_metadata_dialog.hxx
         include/peer_wire_client.hxx
         include/udp_torrent_client.hxx
         include/udp_socket.hxx
         include/tcp_socket.hxx
         include/file_manager.hxx
)

set(CMAKE_AUTOMOC on)

add_executable(${PROJECT_NAME} ${SOURCES} ${MOC_INCLUDES})

target_compile_options(${PROJECT_NAME} PRIVATE
         -Wall -Wextra -pedantic -Wshadow -Wconversion
)

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -fsanitize=undefined,address -fno-omit-frame-pointer")

target_include_directories(${PROJECT_NAME} PRIVATE
         "include"
         SYSTEM "ext"
)

find_package(Qt6 COMPONENTS Network Widgets REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
         Qt6::Network
         Qt6::Widgets
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
         [["bencode_parser.hxx"]]
         [["QApplication"]]
         [["QMainWindow"]]
         [["QWidget"]]
         [["QNetworkAccessManager"]]
         [["QDebug"]]
         [["QString"]]
         [["QObject"]]
         [["QGridLayout"]]
         [["QAbstractSocket"]]
         [["QUdpSocket"]]
         [["QTimer"]]
         [["QTime"]]
         [["QByteArray"]]
         [["QBitArray"]]
         [["QTcpSocket"]]
         [["QUdpSocket"]]
         [["QAbstractSocket"]]
         [["QPointer"]]
         [["QFile"]]
         [["QSettings"]]
         [["QFileInfo"]]
         [["QCryptographicHash"]]
         [["QMessageBox"]]
)