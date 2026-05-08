#include <stdio.h>
#include <string.h>
#include "parking.h"

void write_log_msg(const char *action) {
    char timebuf[64];
    char line[512];
    format_time(now_time(), timebuf, sizeof(timebuf));
    snprintf(line, sizeof(line), "[%s] %s", timebuf, action ? action : "");
    append_text_line(LOG_FILE, line);
}

void show_logs(void) {
    print_line();
    printf("시스템 로그\n");
    print_line();
    print_text_file(LOG_FILE);
    print_line();
}

void show_messages(void) {
    print_line();
    printf("예약번호 발송 기록\n");
    print_line();
    print_text_file(MESSAGES_FILE);
    print_line();
}
