add_contract(did.admin did.admin ${CMAKE_CURRENT_SOURCE_DIR}/src/did.admin.cpp)

target_include_directories(did.admin
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(did.admin
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/did.admin.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/did.admin.contracts.md @ONLY )

target_compile_options( did.admin PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
