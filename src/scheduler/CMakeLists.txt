set(src_files
	"ffi.cc"
	"scheduler.cc"
    "errors/error_code.cc"
    "operations/operation.cc")

add_library(coyote SHARED ${src_files})
set_target_properties(coyote PROPERTIES
    OUTPUT_NAME "coyote"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")
target_compile_definitions(coyote PUBLIC COYOTE_SHARED COYOTE_EXPORT)

add_library(coyote_static STATIC ${src_files})
set_target_properties(coyote_static PROPERTIES
    OUTPUT_NAME "coyote_static"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

file(GLOB test_files "test/*.cc")
foreach(test_file ${test_files})
    get_filename_component(test_name ${test_file} NAME_WE)
    message(STATUS "${test_name}")
	add_executable(${test_name} ${test_file})
	target_link_libraries(${test_name} PRIVATE coyote_static)
	add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
