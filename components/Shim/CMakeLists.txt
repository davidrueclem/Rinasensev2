if("$ENV{TARGET_TYPE}" STREQUAL "freertos_idf")
    set(Manager_INCLUDES
        "${COMMON_INCLUDES}"
        "${CMAKE_SOURCE_DIR}/components/Shim/WiFi/include"
        "${CMAKE_SOURCE_DIR}/components/Shim/NetworkInterface/include"
        "${CMAKE_SOURCE_DIR}/components/Shim/Arp826/include"
        "${CMAKE_SOURCE_DIR}/components/Shim/include")

    idf_component_register(
        SRCS
        "shim.c"
        "WiFi/wifi_IPCP_ops.c"
        "WiFi/wifi_IPCP.c"
        "WiFi/wifi_IPCP_ethernet.c"
        "NetworkInterface/esp32/NetworkInterface.c"
        "Arp826/Arp826.c"

        INCLUDE_DIRS ${Manager_INCLUDES}
        REQUIRES Portability Rina-manager FreeRTOS-Plus-POSIX Ipcp)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/${TARGET_TYPE}/CMakeLists.txt)
endif()