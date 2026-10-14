# In-tree samples: small programs built with the library. The minimal/
# subdirectory is a separate project that consumes the installed
# package instead; CI builds it against a fresh install.

foreach(sample button sequence layers)
    add_executable(sample_${sample} ${sample}.c)
    target_link_libraries(sample_${sample} PRIVATE maul-tween)
    maul_apply_flags(sample_${sample})
    # Each sample returns 0 when its checks hold: run as a test, so a
    # sample that stops working fails the suite.
    if(MAUL_TWEEN_BUILD_TESTS)
        add_test(NAME sample_${sample} COMMAND sample_${sample})
    endif()
endforeach()
