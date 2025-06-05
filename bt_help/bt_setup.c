#include "bt_setup.h"

/*======================DEFINE STATEMENTS========================*/
#define SPP_SERVICE_UUID 0x1101
#define QUERY_DURATION 5

//For debugging purposes
#ifndef DEBUG
#define DEBUG 0
#endif

/*=================STATIC FUNCTIONS DECLARATIONS=================*/
static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_start_sdp_client_query(void * context);
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_context_callback_registration_t handle_sdp_client_query_request;

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

static uint8_t spp_server_channel=0;
static uint16_t rfcomm_cid;
static uint16_t rfcomm_mtu;

static uint8_t* data_packet;
static uint16_t data_packet_size;

static uint16_t max_timeout = 500;

uint8_t bt_start() {
    uint8_t returnStatus = 1;

    l2cap_init();
    printf("l2cap initialization OK!\n");

    rfcomm_init();
    printf("rfcomm initialization OK!\n");

    sdp_init();
    printf("sdp initialization OK!\n");

    handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;
    sdp_client_register_query_callback(&handle_sdp_client_query_request);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("Pico2W");
    
    gap_discoverable_control(0);
    gap_connectable_control(0);
    
    // turn on!
	if (0 == hci_power_control(HCI_POWER_ON)) {
        printf("hci power control turned ON!\n");
    }

    //wait until connection with VLINK is setup properly
    while(state != CONNECTED) {
        sleep_ms(100);
    }

    returnStatus = (state==CONNECTED ? 0 : 1);
    return returnStatus;
}

void send_command_BT(const uint8_t* data, const uint8_t dataSize, uint8_t* response) {
    uint16_t timeout = 0;
    rfcomm_request_can_send_now_event(rfcomm_cid);
    while(communication_flow_state != CAN_SEND_NOW) {
        // printf("communication_flow_state != CAN_SEND_NOW\n");
        if (timeout == max_timeout) {
            printf("sending timeout reached, breaking\n");
            break;
        }
        timeout+=10;
        sleep_ms(10);
    }
    if (timeout != 0) return;
    rfcomm_send(rfcomm_cid, data, dataSize);
    printf("Sending data to VLINK...\n");
    while(communication_flow_state != RESPONSE_RECEIVED) {
        // printf("communication_flow_state != RESPONSE_RECEIVED\n");
        if (timeout == max_timeout) {
            printf("receiving timeout reached, breaking\n");
            break;
        }
        timeout+=10;
        sleep_ms(10);
    }
    if (timeout != 0 ) return;
    printf("Received response from VLINK!\n");
    response = memcpy(response, data_packet, data_packet_size);
    if (data_packet != NULL) {
        free(data_packet);
    }
    if (response == NULL) {
        printf("Response is NULL!\n");
    }
    communication_flow_state = IDLE;
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            spp_server_channel = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            printf("spp_server_channel = %d\n", spp_server_channel);
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
                    printf("Start device scan\n");
                    gap_inquiry_start(QUERY_DURATION);
                    break;
                }
                case GAP_EVENT_INQUIRY_RESULT: {
                    if (state != START_INQUIRY) {
                        break;
                    }
                    gap_event_inquiry_result_get_bd_addr(packet, event_addr);
                    memcpy(peer_addr, event_addr, 6);
                    printf("Peer found: %s\n", bd_addr_to_str(peer_addr));
                    if (0 == bd_addr_cmp(vlink_addr, peer_addr)) {
                        printf("VLINK found, can connect to pair.\n");
                        state = STOP_INQUIRY;
                    }
                    else {
                        state = START_INQUIRY;
                    }           
                    break;
                }
                case GAP_EVENT_INQUIRY_COMPLETE: {
                    switch (state){
                        case START_INQUIRY:                        
                            printf("Inquiry complete\n");
                            printf("Peer not found, starting scan again\n");
                            gap_inquiry_start(QUERY_DURATION);
                            break;                   
                        case STOP_INQUIRY:
                            printf("Start to connect and query for SPP service\n");
                            state = START_SDP_QUERY;
                            handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;
                            (void) sdp_client_register_query_callback(&handle_sdp_client_query_request);
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case HCI_EVENT_PIN_CODE_REQUEST: {
                    printf("Received PIN Code request, sending response 1234.\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "1234");
                    break;
                }
                case HCI_EVENT_USER_CONFIRMATION_REQUEST: {
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;
                }
                case RFCOMM_EVENT_INCOMING_CONNECTION: {
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel 0x%02x requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_cid);
					break;
                }
				case RFCOMM_EVENT_CHANNEL_OPENED: {
					if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                    } 
                    else {
                        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
                        rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID 0x%02x, max frame size %u\n", rfcomm_cid, rfcomm_mtu);
                        state = CONNECTED;
                    }
                    break;
                }
                case RFCOMM_EVENT_CHANNEL_CLOSED: {
                    printf("RFCOMM channel closed\n");
                    rfcomm_cid = 0;
                    spp_server_channel=0;
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
                    state = START_INQUIRY;
                    gap_inquiry_start(QUERY_DURATION);
                    break;
                }
                case RFCOMM_EVENT_CAN_SEND_NOW: {
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
            if (strchr(packet, '>') != NULL) {
                data_packet = malloc(size*sizeof(uint8_t));
                data_packet_size = size;
                data_packet = memcpy(data_packet, packet, size);
                communication_flow_state = RESPONSE_RECEIVED;
                if (data_packet == NULL) {
                    printf("data_packet is NULL after memcpy!\n");
                }
            }
            break;
        }
        default:
            break;
    }
}
