# CompilerWarnings.cmake
#
# Centralizes the mandatory warning set from
# AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 13.1, so every target in
# this project (platform, bsw_*, rte_*, asw_*, the two ECU executables, and
# later test binaries) is built with the same "warnings as errors" policy
# instead of repeating the flag list at every add_library/add_executable
# call site.

function(autosar_apply_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endfunction()
