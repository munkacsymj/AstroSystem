#include <stdio.h>
#include <unistd.h>
#include <proc_messages.h>

int main(int argc, char **argv) {
  do {
    int message_id;
    int ret_val = ReceiveMessage("notify_test", &message_id);
    fprintf(stderr, "ReceiveMessage() returned %d\n", ret_val);
    sleep(1);
  } while (1);
  return 0;
}
