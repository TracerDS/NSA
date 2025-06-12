local PROJECT_NAME = 'Core'

project ( PROJECT_NAME )
    kind 'SharedLib'
    targetdir (ROOT_PATH_JOIN('bin/'..COMMON_PATH))
    objdir (ROOT_PATH_JOIN('!build/obj/$(ProjectName)/'..COMMON_PATH))
    
    includedirs { 'hpp' }
    files {
        'hpp/**.hpp',
        'src/**.cpp'
    }

    vpaths {
        ["Header Files"] = { 'hpp/**.hpp' },
        ["Source Files"] = { 'src/**.cpp' }
    }
