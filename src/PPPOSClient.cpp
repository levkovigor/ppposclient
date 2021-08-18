#include "PPPOSClient.h"


void PPPOSClient::stop()
{
   lwip_close(_socket);
   _startPos = 0;
   _endPos = 0;
   bzero(RxBuffer, sizeof(RxBuffer));
   _connected = false;
}

int PPPOSClient::connect(IPAddress ip, uint16_t port)
{
    int32_t timeout = PPPOS_CLIENT_DEF_CONN_TIMEOUT_MS;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_e("socket: %d", errno);
        return 0;
    }
    fcntl( sockfd, F_SETFL, fcntl( sockfd, F_GETFL, 0 ) | O_NONBLOCK );

    uint32_t ip_addr = ip;
    struct sockaddr_in serveraddr;
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    memcpy((void *)&serveraddr.sin_addr.s_addr, (const void *)(&ip_addr), 4);
    serveraddr.sin_port = htons(port);
    fd_set fdset;
    struct timeval tv;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    tv.tv_sec = 0;
    tv.tv_usec = timeout * 1000;

    #ifdef ESP_IDF_VERSION_MAJOR
        int res = lwip_connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    #else
        int res = lwip_connect_r(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    #endif
    if (res < 0 && errno != EINPROGRESS) {
        log_e("connect on fd %d, errno: %d, \"%s\"", sockfd, errno, strerror(errno));
        close(sockfd);
        return 0;
    }

    res = select(sockfd + 1, nullptr, &fdset, nullptr, timeout<0 ? nullptr : &tv);
    if (res < 0) {
        log_e("select on fd %d, errno: %d, \"%s\"", sockfd, errno, strerror(errno));
        close(sockfd);
        return 0;
    } else if (res == 0) {
        log_i("select returned due to timeout %d ms for fd %d", timeout, sockfd);
        close(sockfd);
        return 0;
    } else {
        int sockerr;
        socklen_t len = (socklen_t)sizeof(int);
        res = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sockerr, &len);

        if (res < 0) {
            log_e("getsockopt on fd %d, errno: %d, \"%s\"", sockfd, errno, strerror(errno));
            close(sockfd);
            return 0;
        }

        if (sockerr != 0) {
            log_e("socket error on fd %d, errno: %d, \"%s\"", sockfd, sockerr, strerror(sockerr));
            close(sockfd);
            return 0;
        }
    }

    fcntl( sockfd, F_SETFL, fcntl( sockfd, F_GETFL, 0 ) & (~O_NONBLOCK) );

    _connected = true;
    _socket = sockfd;
    return 1;
}

int PPPOSClient::connect(const char *host, uint16_t port)
{
   ip_addr_t ip_addr;
   IPAddress aResult = static_cast<uint32_t>(0);
   struct in_addr retAddr;
   struct hostent* he = gethostbyname(host);
   if (he == nullptr) {
      retAddr.s_addr = 0;
      return 0;
   } else {
      retAddr = *(struct in_addr*) (he->h_addr_list[0]);
   }
   inet_aton(inet_ntoa(retAddr), &ip_addr);
   aResult = ip_addr.u_addr.ip4.addr;
   return connect(aResult, port);
}

size_t PPPOSClient::write(uint8_t data)
{
    return write(&data, 1);
}

int PPPOSClient::read()
{
    if (!_connected) return -1;
    if (_startPos >= (_endPos - 1))  {
      _startPos = 0;
      _endPos = 0;
      bzero(RxBuffer, sizeof(RxBuffer));
      int _r = lwip_recv(_socket, RxBuffer, sizeof(RxBuffer)-1, MSG_DONTWAIT);
      if (_r > 0) {
        _endPos = _r + 1;
      } else {
        return -1;
      }
   }
   _startPos++;
   return RxBuffer[_startPos-1];
}

size_t PPPOSClient::write(const uint8_t *buf, size_t size)
{   
    int res =0;
    int retry = PPPOS_CLIENT_MAX_WRITE_RETRY;
    int socketFileDescriptor = _socket;
    size_t totalBytesSent = 0;
    size_t bytesRemaining = size;

    if(!_connected || (socketFileDescriptor < 0)) {
        return 0;
    }

    while(retry) {
        fd_set set;
        struct timeval tv;
        FD_ZERO(&set);
        FD_SET(socketFileDescriptor, &set);
        tv.tv_sec = 0;
        tv.tv_usec = PPPOS_CLIENT_SELECT_TIMEOUT_US;
        retry--;

        if(select(socketFileDescriptor + 1, NULL, &set, NULL, &tv) < 0) {
            return 0;
        }

        if(FD_ISSET(socketFileDescriptor, &set)) {
            res = lwip_send(socketFileDescriptor, (void*) buf, bytesRemaining, MSG_DONTWAIT);
            if(res > 0) {
                totalBytesSent += res;
                if (totalBytesSent >= size) {
                    retry = 0;
                } else {
                    buf += res;
                    bytesRemaining -= res;
                    retry = PPPOS_CLIENT_MAX_WRITE_RETRY;
                }
            }
            else if(res < 0) {
                log_e("fail on fd %d, errno: %d, \"%s\"", fd(), errno, strerror(errno));
                if(errno != EAGAIN) {
                    stop();
                    res = 0;
                    retry = 0;
                }
            }
            else {
            }
        }
    }
    return totalBytesSent;  
}

int PPPOSClient::available()
{  
   if (!_connected) return false;
   if (_startPos >= (_endPos - 1))  {
      _startPos = 0;
      _endPos = 0;
      bzero(RxBuffer, sizeof(RxBuffer));
      int _r = lwip_recv(_socket, RxBuffer, sizeof(RxBuffer)-1, MSG_DONTWAIT);
      if (_r > 0) {
        _endPos = _r + 1;
      } else {
        return false;
      }
   }
   return true;
}

void PPPOSClient::flush() {
    char recv_buf[5000];
    int r = 0;
    do {
      bzero(recv_buf, sizeof(recv_buf));
      r = lwip_recv(_socket, recv_buf, sizeof(recv_buf)-1, MSG_DONTWAIT);
    }while(r > 0);
    _startPos = 0;
    _endPos = 0;
    bzero(RxBuffer, sizeof(RxBuffer));
}

uint8_t PPPOSClient::connected() {
    if (_connected) {
        uint8_t dummy;
        int res = lwip_recv(_socket, &dummy, 0, MSG_DONTWAIT);
        (void)res;
        if (res <= 0){
          switch (errno) {
              case EWOULDBLOCK:
              case ENOENT: 
                  _connected = true;
                  break;
              case ENOTCONN:
              case EPIPE:
              case ECONNRESET:
              case ECONNREFUSED:
              case ECONNABORTED:
                  _connected = false;
                  log_e("Disconnected: RES: %d, ERR: %d", res, errno);
                  break;
              default:
                  log_e("Unexpected: RES: %d, ERR: %d", res, errno);
                  _connected = true;
                  break;
          }
        } else {
          _connected = true;
        }
    }
    return _connected;
}

int PPPOSClient::read(uint8_t *buf, size_t size) {
    if (!_connected) return -1;
    int res = -1;
    if (available()){
        int j = 0;
    	for (int i = _startPos; i < _endPos; i++) {
            if (j < size) {
                buf[j] = RxBuffer[i]; 
            } else {
                res = j; 
                _startPos += j;
            }
            j++;
        }
    }
    return res;
}
	  
int PPPOSClient::peek() {
    if (!_connected) return -1;
	if (_startPos >= (_endPos - 1))  {
      _startPos = 0;
      _endPos = 0;
      bzero(RxBuffer, sizeof(RxBuffer));
      int _r = lwip_recv(_socket, RxBuffer, sizeof(RxBuffer)-1, MSG_DONTWAIT);
      if (_r > 0) {
        _endPos = _r + 1;
      } else {
        _connected = false;
        return -1;
      }
   }
   return RxBuffer[_startPos];
}