import qbs

StaticLibrary {
    name: "QtRpc"

    Depends { name: "cpp" }
    Depends { name: "Qt.core" }
    Depends { name: "Qt.network" }
    Depends { name: "msgpack" }
    Depends { name: "qtpromise" }
    cpp.cxxLanguageVersion: "c++17"

    files: [
        "*.cpp",
        "*.h",
    ]

    // Export include and library information
    Export {
        Depends { name: "cpp" }
        Depends { name: "qtpromise" }
        cpp.includePaths: [product.sourceDirectory]
    }
}
