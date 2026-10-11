# Local fixes to the upstream submodules, kept as patch files in
# lib-patches/<submodule>/ and applied to the submodule's working tree at
# configure time.
#
# The applied set is recorded (by a hash of the patch files) in the
# submodule's git directory. When the set changes (a pull added or edited a
# patch), the submodule's tracked files are reset and every patch is applied
# again in order. Testing each patch on its own isn't reliable: once a later
# patch changes lines next to an earlier one, the earlier one no longer
# reverse-applies cleanly and would look unapplied.
find_package(Git REQUIRED)

function(dinotale_apply_lib_patches submodule)
    set(dir "${CMAKE_SOURCE_DIR}/lib/${submodule}")
    file(GLOB patches "${CMAKE_SOURCE_DIR}/lib-patches/${submodule}/*.patch")
    list(SORT patches)

    set(series "")
    foreach(patch IN LISTS patches)
        file(SHA256 "${patch}" patch_hash)
        string(APPEND series "${patch_hash}\n")
    endforeach()
    string(SHA256 series_hash "${series}")

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --absolute-git-dir
        WORKING_DIRECTORY "${dir}"
        OUTPUT_VARIABLE git_dir OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE result)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "lib/${submodule} isn't a git checkout; run: git submodule update --init --recursive")
    endif()
    set(stamp "${git_dir}/dinotale-patches")

    if (EXISTS "${stamp}")
        file(READ "${stamp}" applied_hash)
        string(STRIP "${applied_hash}" applied_hash)
        if (applied_hash STREQUAL series_hash)
            return()
        endif()
    endif()

    # Start from the submodule's own files, then apply the whole series.
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" checkout -- .
        WORKING_DIRECTORY "${dir}"
        RESULT_VARIABLE result ERROR_VARIABLE error)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Couldn't reset lib/${submodule}:\n${error}")
    endif()
    foreach(patch IN LISTS patches)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${patch}"
            WORKING_DIRECTORY "${dir}"
            RESULT_VARIABLE result ERROR_VARIABLE error)
        if (NOT result EQUAL 0)
            file(REMOVE "${stamp}")
            message(FATAL_ERROR "Couldn't apply ${patch} to lib/${submodule}:\n${error}")
        endif()
        message(STATUS "Applied ${patch} to lib/${submodule}")
    endforeach()
    file(WRITE "${stamp}" "${series_hash}\n")
endfunction()
