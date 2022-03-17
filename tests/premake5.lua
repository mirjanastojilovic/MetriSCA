-- MetriSCA test suite
project "metrisca-tests"
    location (builddir .. "%{prj.name}")
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "On"

    files {
        "src/**.cpp",
        "src/**.hpp"
    }

    includedirs {
        "src"
    }

    UseMetriSCA()
    UseCatch2()
