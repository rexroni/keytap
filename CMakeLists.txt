cmake_minimum_required(VERSION 3.1.0)
project(sdiol)

set(sources
    config.c
    devices.c
    names.c
    networking.c
    resolver.c
    sdiol.c
    server.c
    time_util.c
    permissions.c
)
add_executable(sdiol ${sources})

# lua dependency
find_path(LUA_INCLUDE NAMES lua.h PATH_SUFFIXES lua lua5.3)
find_library(LUA_LIBRARY NAMES lua lua5.3)

# systemd dependency
find_path(SYSTEMD_INCLUDE NAMES systemd/sd-daemon.h)
find_library(SYSTEMD_LIBRARY NAMES systemd)

target_include_directories(sdiol PRIVATE "${LUA_INCLUDE}" "${SYSTEMD_INCLUDE}")
target_link_libraries(sdiol "${LUA_LIBRARY}" "${SYSTEMD_LIBRARY}")

# complier flags
target_compile_options(sdiol PRIVATE -Wall -Wno-unused-result)

# enable color output from compilier, unless it is explicitly disabled
if("${COLORIZE_OUTPUT}" STREQUAL "")
    set(COLORIZE_OUTPUT "yes")
endif()
if("${COLORIZE_OUTPUT}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -fdiagnostics-color=always")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "debug")
    target_compile_options(sdiol PRIVATE -O2)
else()
    target_compile_options(sdiol PRIVATE -g)
endif()

# install files
install(TARGETS sdiol RUNTIME DESTINATION bin)
install(FILES sdiol.service DESTINATION /etc/systemd/system)
