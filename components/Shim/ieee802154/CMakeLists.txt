set(reqs
  Rina-manager
  Portability
  ieee802154
)

idf_component_register(
  SRCS "ieee802154_IPCP_frame.c" "ieee802154_IPCP_ops.c" "ieee802154_NetworkInterface.c"
  INCLUDE_DIRS "include"
  REQUIRES ${reqs})
