find_package(${QT_PACKAGE} COMPONENTS Core Test REQUIRED)

add_executable(test_rpc "test_rpc.cpp")

add_test(test_rpc test_rpc)

set_target_properties(test_rpc PROPERTIES AUTOMOC ON AUTORCC ON)

if(WIN32)
    set_property(TARGET test_rpc PROPERTY WIN32_EXECUTABLE ON)
endif(WIN32)

target_link_libraries(test_rpc PUBLIC
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Test
    QtRpc::QtRpc
    )
