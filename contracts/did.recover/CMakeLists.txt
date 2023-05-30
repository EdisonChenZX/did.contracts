add_contract(did.recover did.recover ${CMAKE_CURRENT_SOURCE_DIR}/src/did.recover.cpp)

target_include_directories(did.recover
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../amax.bootdao/include)

set_target_properties(did.recover
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/did.recover.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/did.recover.contracts.md @ONLY )

target_compile_options( did.recover PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
