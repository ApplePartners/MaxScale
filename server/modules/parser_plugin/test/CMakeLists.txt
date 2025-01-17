add_executable(compare compare.cc testreader.cc)
target_link_libraries(compare maxscale-common)

add_executable(pp_concurrency pp_concurrency.cc)
target_link_libraries(pp_concurrency maxscale-common)

add_executable(pp_cache pp_cache.cc)
target_link_libraries(pp_cache maxscale-common)

add_executable(crash_pp_sqlite crash_pp_sqlite.cc)
target_link_libraries(crash_pp_sqlite maxscale-common)

add_executable(test_pp_sqlite test_pp_sqlite.cc)
target_link_libraries(test_pp_sqlite maxscale-common)

add_test(test_crash_pp_sqlite crash_pp_sqlite)
add_test(test_pp_sqlite test_pp_sqlite)

if (BUILD_PP_MYSQLEMBEDDED)

  add_test(test_pp_comparecreate compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/create.test)
  add_test(test_pp_comparedelete compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/delete.test)
  add_test(test_pp_compareinsert compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/insert.test)
  add_test(test_pp_comparejoin compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/join.test)
  add_test(test_pp_compareselect compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/select.test)
  add_test(test_pp_compareset compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/set.test)
  add_test(test_pp_compareupdate compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/update.test)
  add_test(test_pp_comparemaxscale compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/maxscale.test)
  add_test(test_pp_comparewhitespace compare -v 2 -S -s "select user from mysql.user; ")

  add_test(test_pp_cte_simple       compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/cte_simple.test)
  add_test(test_pp_cte_grant        compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/cte_grant.test)
  add_test(test_pp_cte_nonrecursive compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/cte_nonrecursive.test)
  add_test(test_pp_cte_recursive    compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/cte_recursive.test)

  add_test(test_pp_win                  compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win.test)
  add_test(test_pp_win_avg              compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_avg.test)
  add_test(test_pp_win_big-mdev-10092   compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_big-mdev-10092.test)
  add_test(test_pp_win_big-mdev-11697   compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_big-mdev-11697.test)
  add_test(test_pp_win_big              compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_big.test)
  add_test(test_pp_win_bit              compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_bit.test)
  add_test(test_pp_win_empty_over       compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_empty_over.test)
  add_test(test_pp_win_first_last_value compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_first_last_value.test)
  add_test(test_pp_win_i_s              compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_i_s.test)
  add_test(test_pp_win_lead_lag         compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_lead_lag.test)
  add_test(test_pp_win_min_max          compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_min_max.test)
  add_test(test_pp_win_nth_value        compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_nth_value.test)
  add_test(test_pp_win_ntile            compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_ntile.test)
  add_test(test_pp_win_orderby          compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_orderby.test)
  add_test(test_pp_win_percent_cume     compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_percent_cume.test)
  add_test(test_pp_win_rank             compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_rank.test)
  add_test(test_pp_win_std              compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_std.test)
  add_test(test_pp_win_sum              compare -v2 ${CMAKE_CURRENT_SOURCE_DIR}/win_sum.test)

  add_test(test_pp_oracle-binlog_stm_ps     compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/binlog_stm_ps.test)
  add_test(test_pp_oracle-binlog_stm_sp     compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/binlog_stm_sp.test)
  add_test(test_pp_oracle-exception         compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/exception.test)
  add_test(test_pp_oracle-func_case         compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/func_case.test)
  add_test(test_pp_oracle-func_concat       compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/func_concat.test)
  add_test(test_pp_oracle-func_decode       compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/func_decode.test)
  add_test(test_pp_oracle-func_length       compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/func_length.test)
  add_test(test_pp_oracle-func_misc         compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/func_misc.test)
  add_test(test_pp_oracle-misc              compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/misc.test)
  add_test(test_pp_oracle-ps                compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/ps.test)
  add_test(test_pp_oracle-sequence          compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sequence.test)
  add_test(test_pp_oracle-sp-anonymous      compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-anonymous.test)
  add_test(test_pp_oracle-sp-code           compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-code.test)
  add_test(test_pp_oracle-sp-cursor-decl    compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-cursor-decl.test)
  add_test(test_pp_oracle-sp-cursor-rowtype compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-cursor-rowtype.test)
  add_test(test_pp_oracle-sp-cursor         compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-cursor.test)
  add_test(test_pp_oracle-sp-goto           compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-goto.test)
  add_test(test_pp_oracle-sp-param_inc      compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-param.inc)
  add_test(test_pp_oracle-sp-param          compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-param.test)
  add_test(test_pp_oracle-sp-row            compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-row.test)
  add_test(test_pp_oracle-sp-row-vs-var_inc compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-row-vs-var.inc)
  add_test(test_pp_oracle-sp-security       compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp-security.test)
  add_test(test_pp_oracle-sp                compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/sp.test)
  add_test(test_pp_oracle-trigger           compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/trigger.test)
  add_test(test_pp_oracle-truncate          compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/truncate.test)
  add_test(test_pp_oracle-type_blob         compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/type_blob.test)
  add_test(test_pp_oracle-type_clob         compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/type_clob.test)
  add_test(test_pp_oracle-type_date         compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/type_date.test)
  add_test(test_pp_oracle-type_number       compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/type_number.test)
  add_test(test_pp_oracle-type_raw          compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/type_raw.test)
  add_test(test_pp_oracle-type_varchar      compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/type_varchar.test)
  add_test(test_pp_oracle-type_varchar2     compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/type_varchar2.test)
  add_test(test_pp_oracle-type_variables    compare -v 2 ${CMAKE_CURRENT_SOURCE_DIR}/oracle/variables.test)
endif()

if (BUILD_POSTGRES)
  target_link_libraries(compare postgresprotocol)
  target_compile_definitions(compare PRIVATE -DBUILD_POSTGRES=1)

  file(GLOB POSTGRES_TEST_FILES ${CMAKE_CURRENT_SOURCE_DIR}/postgres/*.sql)

  # From all files in the directory, pick statements that match the regex "^ALTER TABLE "
  # and check that the output for "operation" matches the regex "OP_ALTER_TABLE".
  add_test(pg_alter_table compare -v 3 -0 pp_pg_query -E postgres -x "^ALTER TABLE " -p operation -c "ALTER_TABLE" ${POSTGRES_TEST_FILES})
  add_test(pg_alter compare -v 3 -0 pp_pg_query -E postgres -x "^ALTER" -p operation -c "ALTER" ${POSTGRES_TEST_FILES})

  add_test(pg_create_table compare -v 3 -0 pp_pg_query -E postgres -x "^CREATE TABLE " -p operation -c "CREATE_TABLE" ${POSTGRES_TEST_FILES})
  add_test(pg_create compare -v 3 -0 pp_pg_query -E postgres -x "^CREATE" -p operation -c "CREATE" ${POSTGRES_TEST_FILES})

  add_test(pg_drop_table compare -v 3 -0 pp_pg_query -E postgres -x "^DROP TABLE " -p operation -c "DROP_TABLE" ${POSTGRES_TEST_FILES})
  add_test(pg_drop compare -v 3 -0 pp_pg_query -E postgres -x "^DROP" -p operation -c "DROP" ${POSTGRES_TEST_FILES})

endif()
