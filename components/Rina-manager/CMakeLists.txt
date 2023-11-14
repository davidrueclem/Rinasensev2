if("$ENV{TARGET_TYPE}" STREQUAL "freertos_idf")
    set(Manager_INCLUDES
        "${COMMON_INCLUDES}"
        "${CMAKE_SOURCE_DIR}/components/Rina-manager/Common/include"
        "${CMAKE_SOURCE_DIR}/components/Rina-manager/configSensor/include"
        "${CMAKE_SOURCE_DIR}/components/Rina-manager/configRINA/include"
        "${CMAKE_SOURCE_DIR}/components/Rina-manager/BufferManagement/include"
        "${CMAKE_SOURCE_DIR}/components/Rina-manager/include")

    idf_component_register(
        SRCS
        "Common/list.c"
        "Common/mac.c"
        "Common/num_mgr.c"
        "Common/rina_gpha.c"
        "Common/rina_ids.c"
        "Common/rina_name.c"
        "Common/rsrc.c"
        "Common/common.c"
        "Common/rina_timer.c"
        "IPCP_manager.c"
        "BufferManagement/BufferManagement.c"

        INCLUDE_DIRS ${Manager_INCLUDES}
        REQUIRES Portability Shim Rina-api)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/${TARGET_TYPE}/CMakeLists.txt)
endif()