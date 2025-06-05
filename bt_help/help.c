#include <stdio.h>
#include "pico/stdlib.h"
#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "inttypes.h"
#include "cJSON.h"
#include "ws2812.h"

#define QUERY_DURATION 5
#define WS2812_PIN 15
#define TOUCH_PIN 16
// Major device class: 0x01--(Computer) or 0x02--(Phone)  minor device class 0x0c(Laptop)
#define PEER_COD 0b0001100001100
#define SPP_SERVICE_UUID 0x1101

enum {
    START_INQUIRY=0,
    STOP_INQUIRY,
    PEER_NAME_INQUIRY,
    START_SDP_INQUERY,
    SDP_CHANNEL_QUERY,
};
static uint8_t state;
static bd_addr_t peer_addr;
uint8_t peer_name[240];

uint8_t client_buff[2048];

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint8_t spp_server_channel=0;
static uint16_t rfcomm_cid;
static uint16_t rfcomm_mtu;
static uint8_t page_scan_repetition_mode;
static uint16_t clock_offset;
   

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_context_callback_registration_t handle_sdp_client_query_request;

void set_client_device_name(bd_addr_t addr) {
    if (rfcomm_cid == 0) {
        printf("server channel 0\n");
    }
    sprintf(client_buff, "{\"name\":\"C0\",\"type\":\"set\",\"data\":\"%s\"}", 
                bd_addr_to_str(addr));
    rfcomm_request_can_send_now_event(rfcomm_cid);

}

void process_data(uint8_t *packet, uint16_t size) {
    cJSON* json_obj = cJSON_CreateObject();
    json_obj = cJSON_Parse(packet);
    uint8_t *type = cJSON_GetStringValue(cJSON_GetObjectItem(json_obj, "type"));
    uint8_t *data = cJSON_GetStringValue(cJSON_GetObjectItem(json_obj, "data"));

    if(strcmp(type, "rgb") == 0) {
        ws2812_play(data, strlen(data));
        cJSON_Delete(json_obj);
        return;
    }

}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            spp_server_channel = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (sdp_event_query_complete_get_status(packet)){
                state = START_SDP_INQUERY;
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
    if (state != START_SDP_INQUERY) return;
    state = SDP_CHANNEL_QUERY;
    sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, peer_addr, SPP_SERVICE_UUID);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint32_t class_of_device;
    uint8_t name_len;
    

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {

                case BTSTACK_EVENT_STATE:

                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    state = START_INQUIRY;
                    gap_inquiry_start(QUERY_DURATION);
                    break;

                case GAP_EVENT_INQUIRY_RESULT:
                    if (state != START_INQUIRY) break;
                    class_of_device = gap_event_inquiry_result_get_class_of_device(packet);
                    gap_event_inquiry_result_get_bd_addr(packet, event_addr);
                    memcpy(peer_addr, event_addr, 6);
                    page_scan_repetition_mode = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
                    clock_offset = gap_event_inquiry_result_get_clock_offset(packet);                  
                    break;
                    
                case GAP_EVENT_INQUIRY_COMPLETE:
                    switch (state){
                        case PEER_NAME_INQUIRY:
                            gap_remote_name_request(peer_addr ,page_scan_repetition_mode,  clock_offset | 0x8000);
                            gap_inquiry_start(QUERY_DURATION);
                            break;
                        case START_INQUIRY:                    
                            gap_inquiry_start(QUERY_DURATION);
                            break;                        
                        case STOP_INQUIRY:
                            gap_inquiry_stop();
                            break;
                        default:
                            break;
                    }
                    
                    break;
                case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:

                    strcpy(peer_name, &packet[9]);
                    if (strcmp(peer_name, "PicoW_SPP_S")==0) { 
                            state = STOP_INQUIRY;
                        printf("Start to connect and query for SPP service\n");    
                        sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, peer_addr, SPP_SERVICE_UUID); 
                    } else {
                        state = START_INQUIRY;
                        gap_inquiry_start(QUERY_DURATION);
                    }
                    break;

                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;
/* 
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel 0x%02x requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_cid);
					break;
*/					
				case RFCOMM_EVENT_CHANNEL_OPENED:
					if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                        state = START_INQUIRY;
                        gap_inquiry_start(QUERY_DURATION);
                    } else {
                        rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        bd_addr_t add_buff;
                        gap_local_bd_addr(add_buff);
                        set_client_device_name(add_buff);
                        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);

                        // disable page/inquiry scan to get max performance
                        //gap_discoverable_control(0);
                        //gap_connectable_control(0);
                    }

					break;
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    rfcomm_send(rfcomm_cid, client_buff, strlen(client_buff));
                    break;


                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_cid = 0;
                    spp_server_channel=0;
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

                    // re-enable page/inquiry scan again
                    state = START_INQUIRY;
                    gap_inquiry_start(QUERY_DURATION);

                    //gap_discoverable_control(0);
                    //gap_connectable_control(0);
                    break;



                default:
                    break;
			}
            break;
                        
        case RFCOMM_DATA_PACKET:
            // data in packet
            process_data(packet, size);

            break;

        default:
            break;
	}

}
void switch_callback(uint gpio, uint32_t event_mask) {
    if (gpio == TOUCH_PIN && event_mask == GPIO_IRQ_EDGE_RISE) {
        sprintf(client_buff, "{\"name\":\"S0\",\"type\":\"cmd\",\"data\":\"switch\"}");
        rfcomm_request_can_send_now_event(spp_server_channel);
    }
}

int main()
{
    stdio_init_all();

    if (cyw43_arch_init()) {
        puts("cyw43 init error");
        return 0;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

    gpio_set_irq_enabled_with_callback(TOUCH_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &switch_callback);
    
    ws2812_init(pio0, 0, WS2812_PIN);

    l2cap_init();

    rfcomm_init();

    handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;
    sdp_client_register_query_callback(&handle_sdp_client_query_request);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    

    //gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_set_local_name("PiocW_Client_0");


    gap_discoverable_control(0);
    gap_connectable_control(0);
    // turn on!
	hci_power_control(HCI_POWER_ON);


    
  
    

    while(1) {
        tight_loop_contents();
    }

    return 0;
}