cmake_minimum_required (VERSION 3.18)

project (oabnew)

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set (SRC_LIST
    ${PROJECT_SOURCE_DIR}/src/httpd.c
    ${PROJECT_SOURCE_DIR}/src/oabnew.c
    ${PROJECT_SOURCE_DIR}/src/utils.c
)

include_directories (
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/libs/iniparser/inc
    ${PROJECT_SOURCE_DIR}/libs/slog/inc
)

add_executable (oabnew ${SRC_LIST})

target_link_libraries (oabnew
    ${PROJECT_SOURCE_DIR}/libs/iniparser/lib/libiniparser.a
    ${PROJECT_SOURCE_DIR}/libs/slog/lib/libslog.a
)
set_property(TARGET oabnew PROPERTY C_STANDARD 99)

file(COPY ${PROJECT_SOURCE_DIR}/oabnew.ini DESTINATION ${PROJECT_SOURCE_DIR}/bin)
file(COPY ${PROJECT_SOURCE_DIR}/www DESTINATION ${PROJECT_SOURCE_DIR}/bin)