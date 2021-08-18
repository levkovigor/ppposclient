#ifndef _PPPOSCLIENT_H_
#define _PPPOSCLIENT_H_

#define PPPOS_RXBUFFER_LENGTH       1024
#define PPPOS_CLIENT_DEF_CONN_TIMEOUT_MS  (3000)
#define PPPOS_CLIENT_MAX_WRITE_RETRY      (10)
#define PPPOS_CLIENT_SELECT_TIMEOUT_US    (1000000)
#define PPPOS_CLIENT_FLUSH_BUFFER_SIZE    (1024)

#include <Arduino.h>
#include "Client.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include <errno.h>


class PPPOSClient : public Client
{
 
  public:  

      PPPOSClient() : _connected(false), _socket(0) {}
      virtual int connect(IPAddress ip, uint16_t port);
      virtual int connect(const char *host, uint16_t port);
      virtual size_t write(uint8_t data);
      virtual size_t write(const uint8_t *buf, size_t size);
      virtual int available();
      virtual int read();
      virtual int read(uint8_t *buf, size_t size);
      virtual operator bool() { return 0; }
      virtual int peek();
      virtual void flush();
      virtual void stop();
      virtual uint8_t connected();
    
      using Print::write;
    
  protected:
    bool _connected;
    int _socket;
    uint8_t RxBuffer[PPPOS_RXBUFFER_LENGTH];
    int _startPos = 0;
    int _endPos = 0;
};

#endif /* _PPPOSCLIENT_H_ */
