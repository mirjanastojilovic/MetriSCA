
-- Global variables
basedir = os.getcwd() .. "/"
builddir = basedir .. "build/"
bindir = basedir .. "bin/"
intdir = basedir .. "obj/"
toolsdir = basedir .. "tools/"
vendordir = basedir .. "vendor/"

-- Project workspace
workspace "metrisca"
    
    architecture "x86_64"
    location(builddir)
    configurations { 
        "Debug", 
        "Release"
    }

    flags {
        "MultiProcessorCompile"
    }

    targetdir(bindir .. "%{cfg.buildcfg}")
    objdir(intdir)

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "On"

    filter {}

    newaction {
        trigger     = "clean",
        description = "Clean the build and output directories",
        execute = function ()
            print("Cleaning build directory...")
            os.rmdir(builddir)

            print("Cleaning output directories...")
            os.rmdir(bindir)
            os.rmdir(intdir)
            
            print("Done!")
        end
    }

-- Load 3rd party libraries
include(vendordir)

-- Side-Channel Analysis library
project "metrisca"
    location (builddir .. "%{prj.name}")
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "On"
    pic "On"

    files {
        "src/**.cpp",
        "src/**.hpp",
        "includes/**.hpp"
    }

    includedirs {
        "src",
        "includes"
    }

    UseSpanLite()

    filter "action:vs*"
        vpaths {
            ["src/*"] = {"includes/**.hpp", "src/**.hpp", "src/**.cpp"}
        }

    filter {}

-- This function can be called by any project that wants to include 
-- the metrisca library
function UseMetriSCA()
    includedirs {
        basedir .. "includes"
    }
    links {
        "metrisca"
    }
    UseSpanLite()
end

-- Build additional tools
include(toolsdir)
