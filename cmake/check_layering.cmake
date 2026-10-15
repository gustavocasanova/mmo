# Fails when a source file includes a module from a layer it must not depend on.
# Run by CTest as LayeringTest: cmake -DSOURCE_DIR=<repo> -P cmake/check_layering.cmake

set(engine_dirs core world animation assets equipment character scene editor renderer platform math)
set(game_allowed game core world math)

set(violations "")

function(check_dir dir forbidden_regex)
    file(GLOB_RECURSE sources "${SOURCE_DIR}/src/${dir}/*.hpp" "${SOURCE_DIR}/src/${dir}/*.cpp")
    foreach(source IN LISTS sources)
        file(STRINGS "${source}" includes REGEX "^[ \t]*#[ \t]*include")
        foreach(line IN LISTS includes)
            if(line MATCHES "${forbidden_regex}")
                file(RELATIVE_PATH relative "${SOURCE_DIR}" "${source}")
                list(APPEND violations "${relative}: ${line}")
            endif()
        endforeach()
    endforeach()
    set(violations "${violations}" PARENT_SCOPE)
endfunction()

# Engine never knows the game.
foreach(dir IN LISTS engine_dirs)
    check_dir(${dir} "[\"<](game|presentation|apps)/")
endforeach()

# Game rules stay server-safe: no graphics, window, assets, skeletons or client code.
check_dir(game "[\"<](renderer|platform|scene|assets|animation|equipment|character|editor|presentation|apps)/|<(GLFW|glad|imgui|fastgltf)")

# Client presentation may use engine and game, but not the application or the window.
check_dir(presentation "[\"<](apps|platform)/")

if(violations)
    list(JOIN violations "\n  " message)
    message(FATAL_ERROR "Layering violations:\n  ${message}")
endif()
message(STATUS "Layering OK")
