#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <locale.h>
#include <time.h>
#include "parking.h"

void handle_sigint(int sig) {
    (void)sig;
    write_log_msg("SIGINT 입력으로 프로그램 안전 종료");
    printf("\n프로그램을 종료합니다.\n");
    exit(0);
}

int main(void) {
    setlocale(LC_ALL, "");
    srand((unsigned int)time(NULL));
    signal(SIGINT, handle_sigint);
    if (ensure_data_files() < 0) {
        printf("데이터 파일 초기화에 실패했습니다.\n");
        return 1;
    }
    start_expiration_thread();
    write_log_msg("프로그램 시작");

    while (1) {
        expire_old_reservations();
        print_line();
        printf("무인 주차관리시스템 - 예약 전용\n");
        print_line();
        printf("1. 사용자 페이지\n");
        printf("2. 관리자 페이지\n");
        printf("0. 프로그램 종료\n");
        print_line();
        int choice = read_int("메뉴 선택: ", 0, 2);
        if (choice == 1) user_menu();
        else if (choice == 2) admin_menu();
        else if (choice == 0) {
            write_log_msg("프로그램 정상 종료");
            printf("프로그램을 종료합니다.\n");
            break;
        }
    }
    stop_expiration_thread();
    return 0;
}
