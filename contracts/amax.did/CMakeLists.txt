add_contract(amax.did amax.did ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.did.cpp)

target_include_directories(amax.did
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.did
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.did.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.did.contracts.md @ONLY )

target_compile_options( amax.did PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
