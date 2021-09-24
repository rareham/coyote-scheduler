include_directories(".")
include_directories("../include")
include_directories("./integration/folly/include")

add_subdirectory(unit)
#add_subdirectory(unit/atomic_litmus)
add_subdirectory(integration/atomic_operations)
add_subdirectory(coverage)
