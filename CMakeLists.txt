message(STATUS "CMake version: ${CMAKE_VERSION}")
cmake_minimum_required(VERSION 3.16)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.24.0)
  # We need the timestamps in the files to be the same as they are in the archive, otherwise the build starts
  # failing as automake thinks the files are out-of-date and need to be reconfigured.
  # https://cmake.org/cmake/help/latest/policy/CMP0135.html
  cmake_policy(SET CMP0135 OLD)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/macros.cmake)
enable_testing()

# Packaging builds install to /usr and other builds to /usr/local
if(PACKAGE)
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Prefix prepended to install directories.")
endif()

# Set default values for cache entries and set the MaxScale version
include(cmake/defaults.cmake)

include(VERSION.cmake)
if (MAXSCALE_MATURITY EQUAL "Develop")
  add_definitions(-DMAXSCALE_DEVELOP=1)
endif()

include(ExternalProject)
include(FetchContent)

set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

project(MaxScale)

# Set the installation layout
include(${CMAKE_SOURCE_DIR}/cmake/install_layout.cmake)

# Configure RPATH
# (it has to be before any executable generation)
include(cmake/rpath.cmake)

# Do the platform check
include(cmake/CheckPlatform.cmake)

check_dirs()
find_package(OpenSSL)
find_package(Git)
find_package(LibUUID)
find_package(GSSAPI)
find_package(SQLite)
find_package(CURL)
find_package(PAM)
find_package(GnuTLS REQUIRED)
find_package(Libatomic REQUIRED)

if (WITH_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_res OUTPUT ipo_out)
  if(ipo_res)
    # IPO supported, enable it on all targets.
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "Link-time optimization enabled")
  else()
    message(FATAL_ERROR "Link-time optimization is not supported: ${ipo_out}")
  endif()
endif()

# Build PCRE2 so we always know the version
# Read BuildPCRE2 for details about how to add pcre2 as a dependency to a target
include(cmake/BuildPCRE2.cmake)

include_directories(BEFORE ${PCRE2_INCLUDE_DIRS})

# Always build Connector-C from a known good commit
include(cmake/BuildMariaDBConnector.cmake)
include_directories(BEFORE ${MARIADB_CONNECTOR_INCLUDE_DIR})

include(cmake/BuildJansson.cmake)
include(cmake/BuildMicroHttpd.cmake)
include(cmake/BuildJwtCpp.cmake)

include_directories(${JANSSON_INCLUDE_DIR})

# Both of these modules require librdkafka
if (BUILD_KAFKACDC OR BUILD_KAFKAIMPORTER OR BUILD_MIRROR OR BUILD_SYSTEM_TESTS)
  include(${CMAKE_SOURCE_DIR}/cmake/BuildRdkafka.cmake)
  include_directories(${RDKAFKA_INCLUDE_DIR})
endif()

if(NOT OPENSSL_FOUND AND NOT BUILD_SYSTEM_TESTS)
  message(FATAL_ERROR "Failed to locate dependency: OpenSSL")
else()
  if(OPENSSL_VERSION VERSION_LESS 1 AND NOT FORCE_OPENSSL100)
    add_definitions("-DOPENSSL_0_9")
  elseif(OPENSSL_VERSION VERSION_LESS 1.1)
    add_definitions("-DOPENSSL_1_0")
  else()
    add_definitions("-DOPENSSL_1_1")
  endif()
endif()

if(GIT_FOUND)
  message(STATUS "Found git ${GIT_VERSION_STRING}")
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --max-count=1 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    ERROR_VARIABLE GIT_ERROR
    RESULT_VARIABLE GIT_RVAL)
  if(${GIT_RVAL} EQUAL 0)
    string(REPLACE "\n" "" MAXSCALE_COMMIT ${GIT_COMMIT})
    message(STATUS "Commit ID: ${MAXSCALE_COMMIT}")
  else()
    message(STATUS "Git exited with non-zero value: ${GIT_ERROR}")
    message(STATUS "Could not find repository in source folder, MaxScale commit ID will not be resolved. Will use 'source-build' for commit ID.")
    set(MAXSCALE_COMMIT "source-build")
  endif()
else()
  message(WARNING "Could not find git, MaxScale commit ID will not be resolved. Will use 'source-build' for commit ID.")
  set(MAXSCALE_COMMIT "source-build")
endif()

# Copy cmake_flags, JENKINS_BUILD_TAG, source and value evironmental variables
# into cmake variables. These are used by the build system to store information
# about the packages being built.
set(MAXSCALE_SOURCE "$ENV{source} $ENV{value}")
set(MAXSCALE_CMAKE_FLAGS "$ENV{cmake_flags}")
set(MAXSCALE_JENKINS_BUILD_TAG "$ENV{BUILD_TAG}")

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/server/include)
configure_file(${CMAKE_SOURCE_DIR}/server/test/maxscale_test.h.in ${CMAKE_BINARY_DIR}/include/maxscale/maxscale_test.h @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/etc/postinst.in ${CMAKE_BINARY_DIR}/postinst @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/etc/prerm.in ${CMAKE_BINARY_DIR}/prerm @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/etc/upstart/maxscale.conf.in ${CMAKE_BINARY_DIR}/upstart/maxscale.conf @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/test/maxscale_test.cnf ${CMAKE_BINARY_DIR}/maxscale.cnf @ONLY)

set(FLAGS "-fno-omit-frame-pointer -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-variable -Wno-unused-function -Wno-unused-result -Wno-unused-but-set-variable -Werror=format-security -Werror -fPIC" CACHE STRING "Compilation flags")

# clang complains if a compile flag is not used
add_link_options("-rdynamic")

if(GCOV)
  set(FLAGS "${FLAGS} --coverage -O0 -g" CACHE STRING "Compilation flags"  FORCE)
endif()

if(PROFILE)
  message(STATUS "Profiling executables")
  set(FLAGS "${FLAGS} -pg " CACHE STRING "Compilation flags" FORCE)
endif()

if (WITH_ASAN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  # Only relevant when MaxScale is linked with -fsanitize=address
  set(SERVICE_FILE_ASAN_OPTIONS "Environment=ASAN_OPTIONS=abort_on_error=1:unmap_shadow_on_exit=1")
  add_definitions(-DMXS_WITH_ASAN=1)
endif()
if (WITH_TSAN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()
if (WITH_UBSAN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fno-sanitize=vptr -fno-sanitize-recover")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-sanitize=vptr -fno-sanitize-recover")
  set(SERVICE_FILE_UBSAN_OPTIONS "Environment=UBSAN_OPTIONS=abort_on_error=1:print_stacktrace=1")
endif()

# disable warnings emanating from sqlite
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(FLAGS "${FLAGS} -Wno-tautological-constant-out-of-range-compare")
  set(FLAGS "${FLAGS} -Wno-implicit-const-int-float-conversion")
  set(FLAGS "${FLAGS} -Wno-unqualified-std-cast-call")
else() # GCC
  if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER 7.1.0)
    add_compile_options(-Wshadow=local)
  endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -D_GNU_SOURCE=1 ${FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb -DSS_DEBUG -DLOG_ASSERT")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -ggdb")

# CRTP in config2.hh subclasses redefining the base class virtual to_json()
# causes a warning. Calling the virtual version with such a subclass would
# be a compilation error, so the warning is not very important.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overloaded-virtual")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${FLAGS} ${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wsuggest-override")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -DSS_DEBUG -DLOG_ASSERT")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ggdb")

include_directories(include)
include_directories(server/modules/include)
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CURL_INCLUDE_DIRS})
include_directories(maxutils/maxbase/include)
include_directories(maxutils/maxsql/include)
include_directories(maxutils/maxsimd/include)

add_subdirectory(maxutils)
add_subdirectory(server)
add_subdirectory(include/maxscale)
add_subdirectory(maxctrl)
add_subdirectory(connectors)
add_subdirectory(maxgui)

if(BUILD_TESTS)
  add_subdirectory(examples)
endif()

if(BUILD_SYSTEM_TESTS)
  add_subdirectory(system-test)
endif()

install_file(Documentation/Changelog.md core)
install_file(etc/maxscale.cnf.template core)
install_script(script/maxscale_generate_support_info.py core)

# Install the template into /etc
if(WITH_MAXSCALE_CNF AND (NOT TARGET_COMPONENT OR "core" STREQUAL "${TARGET_COMPONENT}"))
  install_custom_file(etc/maxscale.cnf.template ${MAXSCALE_CONFDIR} core)
endif()

install_file(${CMAKE_SOURCE_DIR}/COPYRIGHT core)
install_file(${CMAKE_SOURCE_DIR}/README.md core)
install_file(${CMAKE_SOURCE_DIR}/LICENSE.TXT core)
install_file(${CMAKE_SOURCE_DIR}/LICENSE2308.TXT core)
install_file(${CMAKE_SOURCE_DIR}/LICENSE-THIRDPARTY.TXT core)
install_manual(Documentation/maxscale.1 1 core)
install_file(${CMAKE_SOURCE_DIR}/etc/logrotate.d/maxscale_logrotate core)

# For devel package, these files are put to the base folder
install_header(${CMAKE_SOURCE_DIR}/COPYRIGHT devel)
install_header(${CMAKE_SOURCE_DIR}/LICENSE.TXT devel)

# Install startup scripts and other system configuration files
if(WITH_SCRIPTS)
  include(cmake/init_scripts.cmake)

  # Prevents prelink from corrupting maxctrl
  install_custom_file(etc/prelink.conf.d/maxscale.conf /etc/prelink.conf.d/ core)
endif()

if(PACKAGE)
  # Configure packaging
  include(cmake/package.cmake)

  # Install the files copied by the postinst script into the share folder
  install_program(${CMAKE_BINARY_DIR}/maxscale core)
  install_file(${CMAKE_BINARY_DIR}/maxscale.conf core)
  install_program(${CMAKE_BINARY_DIR}/postinst core)
  install_program(${CMAKE_BINARY_DIR}/prerm core)

  # The inclusion of CPack needs to be the last effective packaging related command. All
  # configurations to packaging done after the call will be ignored.
  include(CPack)
endif()

#
# Custom targets for MaxScale
#

# uninstall target
# see http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# NOTE: If you make changes here, ensure they are compatible with the
#       situation in paths.h.in.
if (NOT PACKAGE)
  install(DIRECTORY DESTINATION var/cache/maxscale)
  install(DIRECTORY DESTINATION var/log/maxscale)
  install(DIRECTORY DESTINATION var/run/maxscale)
  install(DIRECTORY DESTINATION var/lib/maxscale)
  install(DIRECTORY DESTINATION ${DEFAULT_MODULE_CONFIG_SUBPATH})
endif()
