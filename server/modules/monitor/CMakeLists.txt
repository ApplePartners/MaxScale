add_subdirectory(galeramon)
add_subdirectory(mariadbmon)
add_subdirectory(xpandmon)
if (BUILD_POSTGRES)
    add_subdirectory(pgmon)
endif()
