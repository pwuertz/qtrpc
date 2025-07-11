find_package(${QT_PACKAGE} COMPONENTS Core Network REQUIRED)
find_package(msgpack-cxx REQUIRED)


add_library(QtRpc)
add_library(QtRpc::QtRpc ALIAS QtRpc)

include(GenerateExportHeader)
generate_export_header(QtRpc
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/QtRpc_export.hpp"
    )

set_target_properties(QtRpc PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
    AUTOMOC ON
    )

target_include_directories(QtRpc PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
    )

target_link_libraries(QtRpc PUBLIC
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Network
    msgpack-cxx
    qtpromise::qtpromise
    )

target_sources(QtRpc PRIVATE
    "include/QRpcPeer.hpp"
    "include/QRpcService.hpp"
    "MsgpackRpcProtocol.hpp"
    "QtMsgpackAdaptor.hpp"
    "QRpcPeer.cpp"
    "QRpcService.cpp"
    )
