import qbs

CppApplication {
    consoleApplication: true
    Depends { name: "Qt"; submodules: [ "core", "network", "testlib" ] }
    Depends { name: "QtRpc" }
    cpp.cxxLanguageVersion: "c++17"

    files: [
        "test_rpc.cpp",
    ]
}
