
add_library(mariadbprotocol OBJECT
  detect_special_query.cc
  diskspace.cc
  local_client.cc
  mariadb_backend.cc
  mariadb_client.cc
  mariadb_common.cc
  mariadb_maxscale.cc
  mariadbparser.cc
  packet_parser.cc
  protocol_module.cc
  resultset.cc
  rwbackend.cc
  sqlite_strlike.cc
  user_data.cc
  )
set_target_properties(mariadbprotocol PROPERTIES VERSION "2.0.0" LINK_FLAGS -Wl,-z,defs)
target_link_libraries(mariadbprotocol maxbase maxsql maxsimd)

if (BUILD_TESTS)
  add_subdirectory(test)
endif()
