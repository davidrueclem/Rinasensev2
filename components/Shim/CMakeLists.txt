if("$ENV{TARGET_TYPE}" STREQUAL "freertos_idf")
    set(Manager_INCLUDES
        "${COMMON_INCLUDES}"
        "${CMAKE_SOURCE_DIR}/components/Shim/WiFi/include"
        "${CMAKE_SOURCE_DIR}/components/Shim/ieee802154/include"
        "${CMAKE_SOURCE_DIR}/components/Shim/NetworkInterface/include"
        "${CMAKE_SOURCE_DIR}/components/Shim/Arp826/include"
        "${CMAKE_SOURCE_DIR}/components/Shim/include")

    idf_component_register(
        SRCS

        "WiFi/wifi_IPCP_ops.c"
        "WiFi/wifi_IPCP_ethernet.c"
        "ieee802154/ieee802154_IPCP_ops.c"
        "ieee802154/ieee802154_NetworkInterface.c"
        "ieee802154/ieee802154_IPCP_frame.c"
        "NetworkInterface/esp32/NetworkInterface.c"
        "Arp826/Arp826.c"
        "shim.c"
        "shim_IPCP_flow.c"

        INCLUDE_DIRS ${Manager_INCLUDES}
        REQUIRES Portability Rina-manager FreeRTOS-Plus-POSIX Ipcp esp_wifi esp_netif ieee802154)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/${TARGET_TYPE}/CMakeLists.txt)
endif()