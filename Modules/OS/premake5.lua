local PROJECT_NAME = 'OS'

project ( PROJECT_NAME )
    kind 'SharedLib'
    targetdir (ROOT_PATH_JOIN('bin/'..path.join(COMMON_PATH, 'Modules')))
    objdir (ROOT_PATH_JOIN('!build/obj/$(ProjectName)/'..path.join(COMMON_PATH, 'Modules')))

    includedirs { 'hpp' }
    files {
        'hpp/**.hpp',
        'src/**.cpp'
    }

    vpaths {
        ["Header Files"] = { 'hpp/**.hpp' },
        ["Source Files"] = { 'src/**.cpp' }
    }

    defines {
        'NSA_EXPORT',
        'NSA_OS_EXPORT',
    }
