#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "ir_nec_encoder.h"
#include "parse_nec.c"
#include "parsing_utils.c"


#define EXAMPLE_IR_RESOLUTION_HZ     1000000 // 1MHz resolution, 1 tick = 1us 
#define EXAMPLE_IR_TX_GPIO_NUM       3
#define EXAMPLE_IR_RX_GPIO_NUM       1


static uint16_t received_bytes[30][2];
static uint16_t stored_bytes = 0;


// IRTX & IRRX Structs
// 										ir_rx_t
typedef struct {
	rmt_channel_handle_t channel;		// rx channel
	QueueHandle_t recv_queue;			// receive queue
	rmt_receive_config_t recv_config;	// rx configurations
	
} ir_rx_t;

// 										ir_tx_t
typedef struct {
	rmt_channel_handle_t channel;		// tx channel
	rmt_encoder_handle_t ir_encoder;	// tx NEC encoder
	
} ir_tx_t;




// On IR receive done:
static bool ir_rx_done(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}


// start recieving IR data
ir_rx_t startIRRX(ir_rx_t ir_rx) {
	
	if(ir_rx.channel != NULL) {
		ESP_ERROR_CHECK(rmt_enable(ir_rx.channel));
		return ir_rx;
	} else {

	    rmt_rx_channel_config_t rx_channel_cfg = {
	        .clk_src = RMT_CLK_SRC_DEFAULT,
	        .resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
	        .mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
	        .gpio_num = EXAMPLE_IR_RX_GPIO_NUM,
	    };

	    rmt_channel_handle_t rx_channel = NULL;
	    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));
	    

	    QueueHandle_t receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
	    assert(receive_queue);

	    rmt_rx_event_callbacks_t cbs = {
	        .on_recv_done = ir_rx_done,
	    };
	    
	    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue));

	    // the following timing requirement is based on NEC protocol
	    rmt_receive_config_t receive_config = {
	        .signal_range_min_ns = 1250,     // the shortest duration for NEC signal is 560us, 1250ns < 560us, valid signal won't be treated as noise
	        .signal_range_max_ns = 12000000, // the longest duration for NEC signal is 9000us, 12000000ns > 9000us, the receive won't stop early
	    };

	   
		ESP_ERROR_CHECK(rmt_enable(rx_channel));

		ir_rx_t ir_rx = {
			.channel = rx_channel,
			.recv_queue = receive_queue,
			.recv_config = receive_config
		};
		
		ESP_LOGI("IR_RX", "RX Started");
		
		return ir_rx;
	}
}


// Receive IR data
void recvIR(ir_rx_t ir_rx, int socket) {

	rmt_symbol_word_t raw_symbols[64];
	rmt_rx_done_event_data_t rx_data;

	ESP_ERROR_CHECK(rmt_receive(ir_rx.channel, raw_symbols, sizeof(raw_symbols), &ir_rx.recv_config));
	
	
	if (xQueueReceive(ir_rx.recv_queue, &rx_data, pdMS_TO_TICKS(1000)) == pdPASS) {
			
		ESP_ERROR_CHECK(rmt_receive(ir_rx.channel, raw_symbols, sizeof(raw_symbols), &ir_rx.recv_config));


		uint16_t frame_buffer[3];
		
		send_parsed_nec_frame(rx_data.received_symbols, rx_data.num_symbols, socket, frame_buffer);


		if(frame_buffer[0] + frame_buffer[1] != 0) {
			received_bytes[stored_bytes][0] = frame_buffer[0];
			received_bytes[stored_bytes][1] = frame_buffer[1];
			
			stored_bytes++;
			
			ESP_LOGE("IR_RX", "Recieved IR data!");

		}
    }
}

// stop recieving IR data
void stopIRRX(ir_rx_t ir_rx) {
	// disable IRRX
	ESP_ERROR_CHECK(rmt_disable(ir_rx.channel));
	
}




// creates and returns an IR encoder for tx 
rmt_encoder_handle_t createIREncoder(void) {
	// load encoder configs 
    ir_nec_encoder_config_t nec_encoder_cfg = {
        .resolution = EXAMPLE_IR_RESOLUTION_HZ,
    };

    // create and return encoder
    rmt_encoder_handle_t nec_encoder = NULL;
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &nec_encoder));

	return nec_encoder;
}


// creates, enables and returns the ir tx channel 
ir_tx_t startIRTX(ir_tx_t ir_tx) {

	if(ir_tx.channel != NULL) {
		ESP_ERROR_CHECK(rmt_enable(ir_tx.channel));
		return ir_tx;
	
	} else {
	
		// config channel
		rmt_tx_channel_config_t tx_channel_cfg = {
		    .clk_src = RMT_CLK_SRC_DEFAULT,
		    .resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
		    .mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
		    .trans_queue_depth = 4,  // number of transactions that allowed to pending in the background, this example won't queue multiple transactions, so queue depth > 1 is sufficient
		    .gpio_num = EXAMPLE_IR_TX_GPIO_NUM,
		};

		// create new channel
		rmt_channel_handle_t tx_channel = NULL;
		ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));


		// add carier to tx channel
		rmt_carrier_config_t carrier_cfg = {
		    .duty_cycle = 0.33,
		    .frequency_hz = 38000, // 38KHz
		};
		
		ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

		// enable tx channel 
		ESP_ERROR_CHECK(rmt_enable(tx_channel));

		// returned struct of needed data
		ir_tx_t ir_tx = {
			.channel = tx_channel,
			.ir_encoder = createIREncoder()
		};

		ESP_LOGI("IR_TX", "TX Started.");
		return ir_tx;
	}
}



// disables IRTX capabilities
void stopIRTX(ir_tx_t ir_tx) {
	ESP_ERROR_CHECK(rmt_disable(ir_tx.channel));
}

// sent ir data
void sendIR(ir_tx_t ir_tx, ir_nec_scan_code_t data) {

	rmt_transmit_config_t transmit_config = {
	    .loop_count = 0, // no loop
	};

	ESP_ERROR_CHECK(rmt_transmit(ir_tx.channel, ir_tx.ir_encoder, &data, sizeof(data), &transmit_config));
}


void replay_frames(ir_tx_t ir_tx) {

	rmt_transmit_config_t transmit_config = {
		    .loop_count = 0, // no loop
	};

	
	ESP_LOGE("LOOP", "Starting loop of %d packets now.", stored_bytes);
	
	
	for(uint16_t i = 0; i < stored_bytes; i++) {

		unsigned int microseconds = 5e5;
		usleep(microseconds);
		
		ir_nec_scan_code_t data = {
			.address = received_bytes[i][0],
			.command = received_bytes[i][1],
		};

		sendIR(ir_tx, data);
	}

	memset(received_bytes, 0, sizeof received_bytes);
	stored_bytes = 0;
	
}

