// https://github.com/espressif/esp-lwip/blob/a587d929899304264d81a469dc843316d0db5e64/src/include/lwip/sockets.h#L78
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>


#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"
#include "esp_timer.h"


#include "ir_nec_encoder.h"
#include "socket_utils.c"
#include "IR.c"


/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define WIFI_SSID      				"AP_Mobile"
#define WIFI_PASS      				"Haxi%8sm3$09jhsd"
#define TCP_PORT				   	4567				
#define ESP_WIFI_CHANNEL   			1
#define MAX_STA_CONN       			2


#define GPIO_OUTPUT_PIN  GPIO_NUM_3
#define GPIO_INPUT_PIN  GPIO_NUM_1


static ir_rx_t ir_rx;
static ir_tx_t ir_tx;

static int phone;

static const char *TAG = "AP_Mobile";

enum Modes {					
  NONE,			// 0
  IRTX,  		// 1
  IRRX,  		// 2
};

static uint16_t mode = NONE;


// Wifi event listener
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) // event ID
{	
	// station connected to access point
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
                 
    // station disconnected to the access point 
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}



// initialiazing the access point
void wifi_init_softap(void)
{

	// Make initial AP to build on
	
    ESP_ERROR_CHECK(esp_netif_init());					// init netif
    ESP_ERROR_CHECK(esp_event_loop_create_default());	// create default loop

	
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // get initial configs
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); 				// initialize wifi w/ configs pointer 
    													// since there is no kernel to interact 
    													// with the memory of the device, pointers
    													// are a great way to conserve memory. this 
    													// also creates a globaly mutable 

	// register the wifi_event_handler to the AP
    													
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,				
                                                        ESP_EVENT_ANY_ID,		
                                                        &wifi_event_handler,	// Wifi event listener
                                                        NULL,					
                                                        NULL));					
	// Wifi configurations
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
            
#endif
            .pmf_cfg = {
                    .required = true,
            },
            
            .ssid_hidden = 1,
        },
    };

    
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));	 			// set up AP mode	
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));	// configure the AP

	// Wifi Start
    ESP_ERROR_CHECK(esp_wifi_start());

    // DEBUG

    ESP_LOGE(TAG, 
    	"wifi_init_softap finished. SSID:%s password:%s channel:%d",
        EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}



// Stop the current mode's resourses

// TODO: send a packet to client with information about stoping the mode here
void stop_mode() {

	if (mode == IRTX) {							// Stop IRTX
		
		ESP_LOGI("STOP", "Shutting down IRTX Processes");
		stopIRTX(ir_tx);

		
	} else if (mode == IRRX) {					// Stop IRRX
		
		ESP_LOGI("STOP", "Shutting down IRRX Processes");
		stopIRRX(ir_rx);
		
		
	}

	// respond Quit to clinet
	char exit_msg[8] = "END\r\n\r\n";
	send(phone, &exit_msg, (size_t)sizeof(exit_msg), 0);

	mode = NONE;
}


void cc_operations(char *command) {
	
	// char function[5];
	// strncpy(function, command, 4);

	

	// if the copy is correct, just make sure that 
	ESP_LOGI("CC", "Parsing command: %s | command_length: %d", command, sizeof command);

	if(check_for_exit(command) == 1) {
		stop_mode();
	}
	
	// if there isnt a mode selected yet
	if (mode == NONE) {

		if (strcmp("IRTX", command) == 0) {
		
			ESP_LOGI("CC", "Starting IRTX now.");
			ir_tx = startIRTX(ir_tx);
			
			mode = IRTX;

		// start transmitting IR data
		} else if (strcmp("IRRX", command) == 0) {
		
			ESP_LOGI("CC", "Starting IRRX now.");
			ir_rx = startIRRX(ir_rx);
			
			mode = IRRX;
		}

	// if there is already a mode selected
	} else {

		if (mode == IRTX) {			// IRTX	
			
			if (strcmp("RPLY", command) == 0) { 	

				// REPLAY previous IRRX session
				ESP_LOGI("CC RPLY", "Replaying captured frames now");
				replay_frames(ir_tx);

				stop_mode();

				
			// parse hex code	
			} else {

		

				// parse command and format data
				uint16_t rx_buffer[3];

				parse_data(rx_buffer, command);

				ir_nec_scan_code_t ir_data = {
						.address = rx_buffer[0],
						.command = rx_buffer[1],
				};
				
				// sending IR data
				ESP_LOGI("CC IRTX", "Sending: 0x%04X 0x%04X", rx_buffer[0], rx_buffer[1]);
				sendIR(ir_tx, ir_data);
					
			
			}
		}
		
	}
}




// starts command and control server
void start_cc(void) {

	// Create server
	int server = create_server_socket(TCP_PORT);
	
    if(server == 0) {
		ESP_LOGE("TCP", "Created server, listening for incomming connections...");	
		return;
    }

	ESP_LOGE("TCP", "Created server, listening for incomming connections...");	
	listen(server, 1);


	struct sockaddr_in6 phone_addr;


	// try to recieve tcp connections
	while(1) {

		// Try to accept connection
		socklen_t addr_size = sizeof(phone_addr);

		phone = accept(server, (struct sockaddr*) &phone_addr, &addr_size);

		// Make phone socket non-blocking
		if(fcntl(phone, F_SETFL, fcntl(phone, F_GETFL) | O_NONBLOCK) < 0) {
		    ESP_LOGE("TCP", "Couldnt make socket non-blocking.");	
		} 
		
		
		
		if(phone < 0) {	
            ESP_LOGE("TCP", "Unable to accept connection");	

            // sleeping
			unsigned int microseconds = 5e6;
			usleep(microseconds);
			
		} else {														
		
			ESP_LOGE("TCP", "Accepted new connection!");	// Successfull TCP connection	
			
			while(1) {		// TCP Reveive Loop

				unsigned int microseconds = 5e5;
				usleep(microseconds);
				
				char recv_buffer[11];

				// recv data from phone
				uint16_t len = recv(phone, recv_buffer, sizeof(recv_buffer) - 1, 0);
				
				
				if(len == 0) {
				
	            	ESP_LOGE("TCP", "Connection closed. Restarting...");	
	            	break;
		
				} else if(len < sizeof(recv_buffer) && len > 0) {  					// command recieved
					
					recv_buffer[len-1] = '\0';


					ESP_LOGE("TCP", "Received(%d): %s", len, recv_buffer);	
					
					cc_operations(recv_buffer);

					memset(recv_buffer, 0, sizeof recv_buffer);
					
				} else {


					if(mode == IRRX) {												// Try to recieve IRRX
									
						ESP_LOGE("TCP", "Trying to receive IR data...");	
						recvIR(ir_rx, phone);
					}			
				}
				
			}
		
		}	
	}
}




void app_main(void)
{
    //Initialize NVS
    // TODO: what is nvs used for... maybe to initialize the memory????
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
	wifi_init_softap();

	// build, configure and start the AP
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

   
    start_cc();
    
}
