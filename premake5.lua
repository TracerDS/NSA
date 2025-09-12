function ROOT_PATH_JOIN(str)
    return path.normalize(path.join(_MAIN_SCRIPT_DIR, str))
end

COMMON_PATH = '$(PlatformTarget)/$(Configuration)'

workspace "NSA"
    configurations { "Debug", "Release" }
    platforms { "x64" }
    language "C++"
    cppdialect "C++latest"
    characterset "MBCS"

    includedirs { '.', 'Shared', 'Modules', 'Shared/include' }
    libdirs { 'Shared/lib' }
    links {
        'ws2_32.lib',
        'bcrypt.lib',
        'secur32.lib',
        'ole32.lib',
        'shell32.lib',

        'avcodec.lib',
        'avformat.lib',
        'avutil.lib',
        'swresample.lib',
        'swscale.lib',
    }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        runtime "Debug"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Speed"
        runtime "Release"

    filter "platforms:x64"
        systemversion "latest"
        architecture "x64"

    filter "system:windows"
        defines { "_WIN32", "WIN32_LEAN_AND_MEAN", "NOMINMAX" }

    filter {}

    location "build"
    startproject "Loader"
    flags { "MultiProcessorCompile" }

    include 'Loader'
    include 'Core'
    include 'Shared'
    group 'Modules'
        include 'Modules/OS'
        include 'Modules/FFmpeg'