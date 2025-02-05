#include "driver/rmt_rx.h"
#include "ir_nec_encoder.h"
#include "lwip/sockets.h"

#include "esp_log.h"



#include <string.h>

#define IR_NEC_DECODE_MARGIN 200     // Tolerance for parsing RMT symbols into bit stream
#define NEC_LEADING_CODE_DURATION_0  9000
#define NEC_LEADING_CODE_DURATION_1  4500
#define NEC_PAYLOAD_ZERO_DURATION_0  560
#define NEC_PAYLOAD_ZERO_DURATION_1  560
#define NEC_PAYLOAD_ONE_DURATION_0   560
#define NEC_PAYLOAD_ONE_DURATION_1   1690
#define NEC_REPEAT_CODE_DURATION_0   9000
#define NEC_REPEAT_CODE_DURATION_1   2250


static uint16_t s_nec_code_address;
static uint16_t s_nec_code_command;

/**
 * @brief Check whether a duration is within expected range
 */
static inline bool nec_check_in_range(uint32_t signal_duration, uint32_t spec_duration)
{
    return (signal_duration < (spec_duration + IR_NEC_DECODE_MARGIN)) &&
           (signal_duration > (spec_duration - IR_NEC_DECODE_MARGIN));
}

/**
 * @brief Check whether a RMT symbol represents NEC logic zero
 */
static bool nec_parse_logic0(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ZERO_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ZERO_DURATION_1);
}

/**
 * @brief Check whether a RMT symbol represents NEC logic one
 */
static bool nec_parse_logic1(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ONE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ONE_DURATION_1);
}

/**
 * @brief Decode RMT symbols into NEC address and command
 */
static bool nec_parse_frame(rmt_symbol_word_t *rmt_nec_symbols)
{
    rmt_symbol_word_t *cur = rmt_nec_symbols;
    
    uint16_t address = 0;
    uint16_t command = 0;
    
    bool valid_leading_code = nec_check_in_range(cur->duration0, NEC_LEADING_CODE_DURATION_0) &&
                              nec_check_in_range(cur->duration1, NEC_LEADING_CODE_DURATION_1);
    if (!valid_leading_code) {
    	ESP_LOGI("NEC PARSE FRAME", "INVALID LEADING CODE");
        return false;
    }
    
    cur++;
    
    for (int i = 0; i < 16; i++) {
    
        if (nec_parse_logic1(cur)) {
            address |= 1 << i;
        } else if (nec_parse_logic0(cur)) {				// parsing nec address
            address &= ~(1 << i);
        } else {
            return false;
        }
        
        cur++;
    }
    
    for (int i = 0; i < 16; i++) {
        if (nec_parse_logic1(cur)) {
            command |= 1 << i;
        } else if (nec_parse_logic0(cur)) {				// parsing nec command
            command &= ~(1 << i);
        } else {
            return false;
        }
        
        cur++;
        
    }
    // save address and command
    s_nec_code_address = address;
    s_nec_code_command = command;
    return true;
}

/**
 * @brief Check whether the RMT symbols represent NEC repeat code
 */
static bool nec_parse_frame_repeat(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_REPEAT_CODE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_REPEAT_CODE_DURATION_1);
}

/**
 * @brief Decode RMT symbols into NEC scan code and print the result
 */
static void send_parsed_nec_frame(rmt_symbol_word_t *rmt_nec_symbols, size_t symbol_num, int socket, uint16_t *buffer)
{
	ESP_LOGE("PARSE NEC", "RECIEVED A FRAME!");
	
	char data[12];
	
    
    // decode RMT symbols
    switch (symbol_num) {
    case 34: // NEC normal frame
        if (nec_parse_frame(rmt_nec_symbols)) {
            sprintf(data, "%04X %04X\n", s_nec_code_address, s_nec_code_command);
        }

		send(socket, data, strlen(data), 0);

	    buffer[0] = s_nec_code_address;
	    buffer[1] = s_nec_code_command;
        
        break;
    case 2: // NEC repeat frame
// 
//         if (nec_parse_frame_repeat(rmt_nec_symbols)) {
//             sprintf(data, "%04X %04X\n", s_nec_code_address, s_nec_code_command);
//         }

        buffer[0] = 0;
        buffer[1] = 0;
		break;
    default:
        break;
    }

    
}
