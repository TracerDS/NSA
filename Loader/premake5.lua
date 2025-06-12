local PROJECT_NAME = 'Loader'

project ( PROJECT_NAME )
    kind 'ConsoleApp'
    targetdir (ROOT_PATH_JOIN('bin/'..COMMON_PATH))
    objdir (ROOT_PATH_JOIN('!build/obj/$(ProjectName)/'..COMMON_PATH))
    
    targetname 'NSA'

    includedirs { 'hpp' }
    files {
        'hpp/**.hpp',
        'src/**.cpp'
    }

    vpaths {
        ["Header Files"] = { 'hpp/**.hpp' },
        ["Source Files"] = { 'src/**.cpp' }
    }