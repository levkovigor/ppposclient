#ifndef _GSM_H_
#define _GSM_H_
#ifdef __cplusplus
extern "C" {
#endif


#define BUF_SIZE (1024)


void PPPOS_init(int txPin, int rxPin, int baudrate, int uart_number, char* user, char* pass);

bool PPPOS_isConnected();

void PPPOS_start();

bool PPPOS_status();

void PPPOS_stop();

void PPPOS_write(char* cmd);

char* PPPOS_read();

#ifdef __cplusplus
}
#endif
#endif
