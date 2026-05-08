#include <stdio.h>
#include "parking.h"

void user_menu(void) {
    while (1) {
        expire_old_reservations();
        print_line();
        printf("사용자 페이지 - 예약 전용 무인 주차관리 시스템\n");
        print_line();
        printf("1. 주차 예약\n");
        printf("2. 예약번호로 입차\n");
        printf("3. 예약번호로 출차 및 정산\n");
        printf("4. 예약 조회/입차 제한시간 카운트다운\n");
        printf("5. 예약 취소\n");
        printf("6. 예약 변경\n");
        printf("7. 예약번호 찾기(차량번호+전화번호)\n");
        printf("8. 입차 내역/경과시간/현재예상요금 조회\n");
        printf("9. 차종 입력 기반 예약 가능 여부 조회\n");
        printf("10. 차종 기반 최적 주차타워 추천\n");
        printf("11. 주차타워별 차종별 예약 가능 개수 조회\n");
        printf("12. 주차타워 정보 조회\n");
        printf("13. 현재 주차타워 주차 상태 조회\n");
        printf("14. 예상 주차요금 계산기\n");
        printf("15. 실시간 주차 가능 대수 자동 새로고침\n");
        printf("0. 뒤로가기\n");
        print_line();
        int choice = read_int("메뉴 선택: ", 0, 15);
        switch (choice) {
            case 1: create_reservation(); pause_enter(); break;
            case 2: process_entry(); pause_enter(); break;
            case 3: process_exit_and_payment(); pause_enter(); break;
            case 4: lookup_reservation(); pause_enter(); break;
            case 5: cancel_reservation(); pause_enter(); break;
            case 6: change_reservation(); pause_enter(); break;
            case 7: find_reservation_code_by_identity(); pause_enter(); break;
            case 8: lookup_entered_parking_status(); pause_enter(); break;
            case 9: show_car_type_availability(); pause_enter(); break;
            case 10: show_recommended_tower(); pause_enter(); break;
            case 11: show_all_tower_capacity(); pause_enter(); break;
            case 12: show_tower_info(); pause_enter(); break;
            case 13: show_current_parking_status(); pause_enter(); break;
            case 14: run_fee_calculator(); pause_enter(); break;
            case 15: auto_refresh_availability(); pause_enter(); break;
            case 0: return;
            default: break;
        }
    }
}
