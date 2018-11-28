# RapidJSON_FOUND - true if library and headers were found
# RapidJSON_INCLUDE_DIR - include directories

find_path(RapidJSON_INCLUDE document.h PATH_SUFFIXES rapidjson)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RapidJSON DEFAULT_MSG RapidJSON_INCLUDE)

get_filename_component(RapidJSON_PARENTDIR_INCLUDE ${RapidJSON_INCLUDE} PATH)
set(RapidJSON_INCLUDE_DIR ${RapidJSON_PARENTDIR_INCLUDE} CACHE PATH "RapidJSON include directory")
