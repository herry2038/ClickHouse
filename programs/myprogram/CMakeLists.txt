add_executable(processor_dag
    processor_dag.cpp
)

#set (LINK_RESOURCE_LIB INTERFACE "-Wl,${WHOLE_ARCHIVE} $<TARGET_FILE:clickhouse_server_configs> -Wl,${NO_WHOLE_ARCHIVE}")

target_link_libraries(processor_dag
    PRIVATE
        clickhouse_aggregate_functions
        clickhouse_common_config
        clickhouse_common_io
        clickhouse_common_zookeeper
        clickhouse_functions
        clickhouse_parsers
        clickhouse_storages_system
        clickhouse_table_functions
        string_utils
        jemalloc

    ${LINK_RESOURCE_LIB}

    PUBLIC
        daemon
)


