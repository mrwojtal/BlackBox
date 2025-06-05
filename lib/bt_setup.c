#include "bt_setup.h"

#define DEBUG_BT

#define SPP_SERVICE_UUID 0x1101
#define QUERY_DURATION 5
#define DATA_PACKET_MAX_SIZE 64

#define LED_CONNECTED_PERIOD 100
#define LED_NOT_CONNECTED_PERIOD 500

/*=================STATIC FUNCTIONS DECLARATIONS=================*/
static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_start_sdp_client_query(void * context);
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_context_callback_registration_t handle_sdp_client_query_request;
static void led_toggle_handler(struct btstack_timer_source* ts);
static void led_toggle_setup(void);
/*=================INQUIRY STATES ANONYMOUS ENUM=================*/
enum {
    START_INQUIRY,
    STOP_INQUIRY,
    START_SDP_QUERY,
    SDP_CHANNEL_QUERY,
    CONNECTED
};
static uint8_t state;

/*===============ELM327 FLOW CONTROL ANONYMOUS ENUM==============*/
enum {
    IDLE,
    CAN_SEND_NOW,
    RESPONSE_RECEIVED
};
static uint8_t communication_flow_state = IDLE;

static bd_addr_t peer_addr;
static bd_addr_t vlink_addr = {0x10, 0x21, 0x3E, 0x48, 0x49, 0x43};

static uint8_t spp_server_channel = 0;
static uint16_t rfcomm_cid;
static uint16_t rfcomm_mtu;

static btstack_timer_source_t led_toggle;

uint8_t data_packet[DATA_PACKET_MAX_SIZE];
static uint16_t data_packet_size;
// bool partial_response = false;
bool transmission_end = false;
uint8_t data_iterator = 0;

const uint16_t max_timeout = 2000;

/// @brief Function used to check current connection state with OBDII diagnostic interface.
/// @return 0 if connected to device, 1 otherwise
uint8_t bt_check_connection(void) {
    if (state == CONNECTED) {
        return 0;
    }
    else {
        return 1;
    }
}

/// @brief Function used to initialize bt_stack and connect to OBDII diagnostic interface.
/// @return 0 if connected to device, 1 otherwise.
uint8_t bt_start(void) {

    btstack_memory_init();

    l2cap_init();

    rfcomm_init();

    sdp_init();

    handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;
    sdp_client_register_query_callback(&handle_sdp_client_query_request);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    led_toggle_setup();

    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("Pico2W");
    
    gap_discoverable_control(0);
    gap_connectable_control(0);
    
    // turn on!
	hci_power_control(HCI_POWER_ON);

    //wait until connection with VLINK is setup properly
    while(state != CONNECTED) {
        busy_wait_ms(50);
    }

    return (state==CONNECTED ? 0 : 1);
}

/// @brief Function used to transfer data over Bluetooth with Serial Port Profile.
/// @param data Array of data bytes to be send.
/// @param dataSize Ammount of bite to be send.
/// @param response Buffer for response handling.
/// @return 0 if data was send and response received, 1 otherwise.
uint8_t bt_send_command(const uint8_t* data, const uint8_t dataSize, uint8_t* response) {
    uint16_t timeout = 0;
    rfcomm_request_can_send_now_event(rfcomm_cid);
    while(communication_flow_state != CAN_SEND_NOW) {
        #ifdef DEBUG_BT
        printf("communication_flow_state != CAN_SEND_NOW\n");
        #endif
        if (timeout >= max_timeout) {
            #ifdef DEBUG_BT
            printf("Sending timeout reached, breaking\n");
            #endif
            break;
        }
        timeout+=100;
        sleep_ms(100);
    }
    if (timeout >= max_timeout) {
        data_iterator = 0;
        communication_flow_state = IDLE;
        return 1;
    }
    rfcomm_send(rfcomm_cid, data, dataSize);
    #ifdef DEBUG_BT
    printf("Sending data to VLINK...\n");
    #endif
    while(communication_flow_state != RESPONSE_RECEIVED) {
        #ifdef DEBUG_BT
        printf("communication_flow_state != RESPONSE_RECEIVED\n");
        #endif
        if (timeout >= max_timeout) {
            #ifdef DEBUG_BT
            printf("Receiving timeout reached, breaking\n");
            #endif
            break;
        }
        timeout+=100;
        sleep_ms(100);
    }
    if (timeout >= max_timeout) {
        data_iterator = 0;
        communication_flow_state = IDLE;
        return 1;
    }
    #ifdef DEBUG_BT
    printf("Received response from VLINK!\n");
    #endif
    uint8_t i = 0;
    while(true) {
        #ifdef DEBUG_BT
        printf("[%02X] ", data_packet[i]);
        #endif
        response[i] = data_packet[i];
        if (data_packet[i] == 0x3E) {
            break;
        }
        i++;
    }
    communication_flow_state = IDLE;
    return 0;
}

static void led_toggle_handler(struct btstack_timer_source* ts) {
    switch(state) {
        case CONNECTED: {
            if (cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN)) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
            }
            else {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
            }
            btstack_run_loop_set_timer(ts, LED_CONNECTED_PERIOD);
            btstack_run_loop_add_timer(ts);
            break;
        }
        default: {
            if (cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN)) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
            }
            else {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
            }
            btstack_run_loop_set_timer(ts, LED_NOT_CONNECTED_PERIOD);
            btstack_run_loop_add_timer(ts);
            break;
        }
    }
}

static void led_toggle_setup(void) {
    led_toggle.process = &led_toggle_handler;
    btstack_run_loop_set_timer(&led_toggle, LED_NOT_CONNECTED_PERIOD);
    btstack_run_loop_add_timer(&led_toggle);
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            spp_server_channel = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            #ifdef DEBUG_BT
            printf("spp_server_channel = %d\n", spp_server_channel);
            #endif
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (sdp_event_query_complete_get_status(packet)){
                state = START_SDP_QUERY;
                sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, peer_addr, SPP_SERVICE_UUID);
                break;
            } 
            if (spp_server_channel == 0){
                state = START_INQUIRY;
                gap_inquiry_start(QUERY_DURATION);
                break;
            }
            rfcomm_create_channel(packet_handler, peer_addr, spp_server_channel, NULL); 
            break;
        default:
            break;
    }
}

static void handle_start_sdp_client_query(void * context){
    UNUSED(context);
    if (state != START_SDP_QUERY) return;
    state = SDP_CHANNEL_QUERY;
    sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, peer_addr, SPP_SERVICE_UUID);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    UNUSED(channel);
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint32_t class_of_device;
    uint8_t name_len;

    switch (packet_type) {
        case HCI_EVENT_PACKET: {
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE: {
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    state = START_INQUIRY;
                    #ifdef DEBUG_BT
                    printf("Start device scan\n");
                    #endif
                    gap_inquiry_start(QUERY_DURATION);
                    break;
                }
                case GAP_EVENT_INQUIRY_RESULT: {
                    if (state != START_INQUIRY) {
                        break;
                    }
                    gap_event_inquiry_result_get_bd_addr(packet, event_addr);
                    memcpy(peer_addr, event_addr, 6);
                    #ifdef DEBUG_BT
                    printf("Peer found: %s\n", bd_addr_to_str(peer_addr));
                    #endif
                    if (0 == bd_addr_cmp(vlink_addr, peer_addr)) {
                        #ifdef DEBUG_BT
                        printf("VLINK found, can connect to pair.\n");
                        #endif
                        state = STOP_INQUIRY;
                    }
                    else {
                        state = START_INQUIRY;
                    }           
                    break;
                }
                case GAP_EVENT_INQUIRY_COMPLETE: {
                    switch (state) {
                        case START_INQUIRY: {
                            #ifdef DEBUG_BT                   
                            printf("Inquiry complete\n");
                            printf("Peer not found, starting scan again\n");
                            gap_inquiry_start(QUERY_DURATION);
                            #endif
                            break;           
                        }        
                        case STOP_INQUIRY: {
                            #ifdef DEBUG_BT
                            printf("Start to connect and query for SPP service\n");
                            #endif
                            state = START_SDP_QUERY;
                            handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;
                            (void) sdp_client_register_query_callback(&handle_sdp_client_query_request);
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case HCI_EVENT_PIN_CODE_REQUEST: {
                    #ifdef DEBUG_BT
                    printf("Received PIN Code request, sending response 1234.\n");
                    #endif
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "1234");
                    break;
                }
                case HCI_EVENT_USER_CONFIRMATION_REQUEST: {
                    // inform about user confirmation request
                    #ifdef DEBUG_BT
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    #endif
                    break;
                }
                case RFCOMM_EVENT_INCOMING_CONNECTION: {
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    #ifdef DEBUG_BT
                    printf("RFCOMM channel 0x%02x requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    #endif
                    rfcomm_accept_connection(rfcomm_cid);
					break;
                }
				case RFCOMM_EVENT_CHANNEL_OPENED: {
					if (rfcomm_event_channel_opened_get_status(packet)) {
                        #ifdef DEBUG_BT
                        printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                        #endif
                        break;
                    } 
                    else {
                        rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        #ifdef DEBUG_BT
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID 0x%02x, max frame size %u\n", rfcomm_cid, rfcomm_mtu);
                        #endif
                        state = CONNECTED;
                    }
                    break;
                }
                case RFCOMM_EVENT_CHANNEL_CLOSED: {
                    #ifdef DEBUG_BT
                    printf("case RFCOMM_EVENT_CHANNEL_CLOSED\n");
                    printf("RFCOMM channel closed\n");
                    #endif
                    rfcomm_cid = 0;
                    spp_server_channel=0;
                    state = START_INQUIRY;
                    gap_inquiry_start(QUERY_DURATION);
                    break;
                }
                case RFCOMM_EVENT_CAN_SEND_NOW: {
                    #ifdef DEBUG_BT
                    printf("case RFCOMM_EVENT_CAN_SEND_NOW\n");
                    #endif
                    if (rfcomm_cid != 0) {
                        communication_flow_state = CAN_SEND_NOW;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case RFCOMM_DATA_PACKET: {
            #ifdef DEBUG_BT
            printf("case RFCOMM_DATA_PACKET\n");
            printf("packet size: %d\n", size);
            #endif
            transmission_end = false;
            uint8_t i = 0;
            for (i = 0; i < size; i++) {
                #ifdef DEBUG_BT
                printf("[%02X] ", packet[i]);
                #endif
                data_packet[i+data_iterator] = packet[i];
                if (data_packet[i+data_iterator] == 0x3E) {
                    transmission_end = true;
                    // communication_flow_state = RESPONSE_RECEIVED;    
                }
            }
            if (transmission_end == true) {
                #ifdef DEBUG_BT
                printf("transmission end = true\n");
                #endif
                data_iterator = 0;
                communication_flow_state = RESPONSE_RECEIVED;
                #ifdef DEBUG_BT
                printf("\n");
                #endif
            }
            else {
                #ifdef DEBUG_BT
                printf("transmission end = false\n");
                #endif
                data_iterator = i;//+1;
            }
            break;
        }
        default:
            break;
    }
}
