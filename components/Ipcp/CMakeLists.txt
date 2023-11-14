if("$ENV{TARGET_TYPE}" STREQUAL "freertos_idf")
    set(Manager_INCLUDES
        "${COMMON_INCLUDES}"
        "${CMAKE_SOURCE_DIR}/components/Ipcp/include"
        "${CMAKE_SOURCE_DIR}/components/Ipcp/Modules/include")

    idf_component_register(
        SRCS
        "IPCP_ops.c"
        "IPCP_task.c"
        "Modules/connection.c"
        "Modules/delim.c"
        "Modules/dtp.c"
        "Modules/du.c"
        "Modules/efcp.c"
        "Modules/enrollment.c"
        "Modules/flowAllocator.c"
        "Modules/pci.c"
        "Modules/rib.c"
        "Modules/ribd.c"
        "Modules/rmt.c"
        "Modules/serdesMsg.c"

        INCLUDE_DIRS ${Manager_INCLUDES}
        REQUIRES CdapProto Portability Rina-manager FreeRTOS-Plus-POSIX Rina-api)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/${TARGET_TYPE}/CMakeLists.txt)
endif()