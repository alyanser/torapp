cmake_minimum_required(VERSION 3.18)

project(torapp)

set(CMAKE_CXX_STANDARD "17")
set(CMAKE_CXX_STANDARD_REQUIRED true)

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
         src/torrent_properties_displayer.cxx
         src/tcp_socket.cxx
         src/file_allocator.cxx
         src/util.cxx
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
         include/file_allocator.hxx
         include/torrent_properties_displayer.hxx
         src/resources.qrc
)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)

add_executable(${PROJECT_NAME} ${SOURCES} ${MOC_INCLUDES})

if (CMAKE_BUILD_TYPE STREQUAL "release")
         add_compile_definitions(QT_NO_DEBUG_OUTPUT)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
         "include"
)

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
         "ext"
)

find_package(Qt6 COMPONENTS Network Widgets REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
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
         [["QGridLayout"]]
         [["QHBoxLayout"]]
         [["QVBoxLayout"]]
         [["QAbstractSocket"]]
         [["QTableWidget"]]
         [["QFrame"]]
         [["QUdpSocket"]]
         [["QTimer"]]
         [["QTime"]]
         [["QByteArray"]]
         [["QBitArray"]]
         [["QAbstractSocket"]]
         [["QTcpSocket"]]
         [["QUdpSocket"]]
         [["QPointer"]]
         [["QFile"]]
         [["QSettings"]]
         [["QFileInfo"]]
         [["QCryptographicHash"]]
         [["QMessageBox"]]
         [["QList"]]
         [["bencode_parser.hxx"]]
)