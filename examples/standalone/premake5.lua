
-- Standalone example
project "standalone"
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

    filter "action:vs*"
        debugdir(bindir .. "%{cfg.name}")

    filter {}

    UseMetriSCA()
