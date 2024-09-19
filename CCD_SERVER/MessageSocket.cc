#include "MessageSocket.h"

MessageSocket::MessageSocket(int socket) {
  SocketID = socket;
}

MessageSocket::~MessageSocket(void) {
  ;
}

MessageSocket::SendMessage(const char *message_content) {
  
