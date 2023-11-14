set(reqs
  Rina-manager
  Portability
)

idf_component_register(
  SRCS "wifi_IPCP_ethernet.c" "wifi_IPCP.c" "wifi_IPCP_ops.c"
  INCLUDE_DIRS "include"
  REQUIRES ${reqs})
