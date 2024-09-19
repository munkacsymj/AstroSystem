class MessageSocket {
public:
  MessageSocket(int socket);
  ~MessageSocket(void);
  SendMessage(const char *message_content,
	      int message_length);
  char *ReceiveMessage(void);
private:
  int SocketID;
};
