#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// parses input command from connected device and 
// returns the corresponding values in the buffer
void parse_data(uint16_t *buffer, char *data) {

	uint16_t i = 0;  
  	char *token = strtok(data, " ");
  
	while (token != NULL) {

		buffer[i] = (uint16_t)strtoul(token, NULL, 16);

		token = strtok(NULL, " ");
		i++;
	}
}

// parsing exit
uint16_t check_for_exit(char *data) {
	ESP_LOGE("Putils", "Checking for exit! msg: %s | len: %d", data, sizeof data);

	if(strcmp("exit", data) == 0) {
		return 1;
	} else {
		return 0;
	} 
	
}



