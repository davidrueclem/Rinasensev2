#ifndef CONFIG_RINA_H
#define CONFIG_RINA_H

#define TESTING (0)
/**************SHIM 802154 CONFIGURATION ***********/
#define SHIM_802154_MODULE (0)

#define SHIM_INTERFACE_802154 "Nodo2" //"Coordinator"

#define ieee802154_COORDINATOR (1) // Zero if coordinator
#define ieee802154_PANID_SOURCE 0x4242
#define ieee802154_PANID_DESTINATION 0x4242
#define ieee802154_CHANNEL 20
#define ieee802154_SHORT_ADDRESS 0x0002 // 0x0002
#define PAN_BROADCAST 0xFFFF
#define SHORT_BROADCAST 0xFFFF
#define ieee802154_MTU (256)

/************* SHIM WIFI CONFIGURATION ***********/
#define SHIM_WIFI_MODULE (1) // Zero if not shim WiFi modules is required.

#define SHIM_PROCESS_NAME "wlan0.ue"
#define SHIM_PROCESS_INSTANCE "1"
#define SHIM_ENTITY_NAME ""
#define SHIM_ENTITY_INSTANCE ""

#define SHIM_DIF_NAME "Irati"

#define SHIM_INTERFACE "ESP_WIFI_MODE_STA"

#define SIZE_SDU_QUEUE (200)

/************ SHIM DIF CONFIGURATION **************/
#define ESP_WIFI_SSID "irati"     //"WiFiTerminet" //"WS02" //irati
#define ESP_WIFI_PASS "irati2017" //"20TrmnT22"    //"Esdla2025" //"irati2017"

/*********** NORMAL CONFIGURATION ****************/

#define NORMAL_PROCESS_NAME "ue1.mobile" //"st4.slice1"
#define NORMAL_PROCESS_INSTANCE "1"
#define NORMAL_ENTITY_NAME ""
#define NORMAL_ENTITY_INSTANCE ""

#define NORMAL_DIF_NAME "mobile.DIF" //"slice1.DIF" //

/*********** NORMAL IPCP CONFIGURATION ****************/
/**** Known IPCProcess Address *****/
#define LOCAL_ADDRESS (1) // 1
#define LOCAL_ADDRESS_AP_INSTANCE "1"
#define LOCAL_ADDRESS_AP_NAME "ue1.mobile" //"st4.slice1"

#define REMOTE_ADDRESS (3) // 3
#define REMOTE_ADDRESS_AP_INSTANCE "1"
#define REMOTE_ADDRESS_AP_NAME "ar1.mobile" // "gw1.slice1" // ar1.mobile

/**** QoS CUBES ****/
#define QoS_CUBE_NAME "unreliable"
#define QoS_CUBE_ID (3)
#define QoS_CUBE_PARTIAL_DELIVERY false
#define QoS_CUBE_ORDERED_DELIVERY true

/**** EFCP POLICIES ****/
/* DTP POLICY SET */
#define DTP_POLICY_SET_NAME "default"
#define DTP_POLICY_SET_VERSION "0"

#define DTP_INITIAL_A_TIMER (300)
#define DTP_DTCP_PRESENT false

/* Linux NetworkInterface options. */

/* Decides if the TAP NetworkInterface will create the tap device itself. */
#define LINUX_TAP_CREATE false

/* Decides if the TAP NetworkInterface will put UP, or DOWN, the
 * virtual device. */
#define LINUX_TAP_MANAGE false

/* */
#define LINUX_TAP_DEVICE "rina00"

#endif
