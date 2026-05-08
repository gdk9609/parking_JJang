#include <stdio.h>
#include <string.h>
#include "parking.h"

static int admin_login(void) {
    char password[64];
    printf("관리자 비밀번호 입력: ");
    safe_read_line(password, sizeof(password));
    if (strcmp(password, ADMIN_PASSWORD) == 0) {
        write_log_msg("관리자 로그인 성공");
        return 1;
    }
    write_log_msg("관리자 로그인 실패");
    printf("비밀번호가 틀렸습니다.\n");
    return 0;
}

void admin_modify_reservation(void) {
    char code[MAX_CODE];
    Reservation r;
    int index;
    printf("수정할 예약번호 입력: ");
    safe_read_line(code, sizeof(code));
    if (!find_reservation_by_code(code, &r, &index)) {
        printf("해당 예약번호를 찾을 수 없습니다.\n");
        return;
    }

    while (1) {
        char reserved[64], entry[64], exit_time[64];
        format_time(r.reserved_at, reserved, sizeof(reserved));
        format_time(r.entry_time, entry, sizeof(entry));
        format_time(r.exit_time, exit_time, sizeof(exit_time));
        print_line();
        printf("관리자 예약 수정\n");
        print_line();
        printf("예약번호: %s / 차량번호: %s / 전화번호: %s\n", r.code, r.car_number, r.phone);
        printf("차종: %s / 주차타워: %c Tower / 상태: %s\n", car_type_name(r.car_type), 'A' + r.tower_id - 1, reservation_status_name(r.status));
        printf("예약: %s / 입차: %s / 출차: %s\n", reserved, entry, exit_time);
        printf("주차요금: %d원 / 최종정산금액: %d원\n", r.total_fee, r.final_paid);
        print_line();
        printf("1. 입차시간 수정\n");
        printf("2. 출차시간 수정\n");
        printf("3. 총 주차요금 수정\n");
        printf("4. 최종 정산금액 수정\n");
        printf("5. 주차타워 수정\n");
        printf("6. 차종 수정\n");
        printf("7. 상태 수정\n");
        printf("0. 저장 후 나가기\n");
        int choice = read_int("수정 항목 선택: ", 0, 7);
        if (choice == 0) break;
        if (choice == 1 || choice == 2) {
            char buf[64];
            time_t t;
            printf("새 시간 입력(YYYY-MM-DD HH:MM): ");
            safe_read_line(buf, sizeof(buf));
            if (!parse_datetime(buf, &t)) {
                printf("날짜/시간 형식이 올바르지 않습니다.\n");
                continue;
            }
            if (choice == 1) r.entry_time = t;
            else r.exit_time = t;
        } else if (choice == 3) {
            r.total_fee = read_int("새 총 주차요금: ", 0, 10000000);
        } else if (choice == 4) {
            r.final_paid = read_int("새 최종 정산금액: ", 0, 10000000);
        } else if (choice == 5) {
            r.tower_id = choose_tower();
        } else if (choice == 6) {
            r.car_type = choose_car_type();
        } else if (choice == 7) {
            printf("0. 취소됨\n1. 예약 완료\n2. 입차 완료\n3. 정산 완료\n4. 시간 초과 취소\n");
            r.status = read_int("새 상태 선택: ", 0, 4);
        }
    }

    if (update_reservation(&r, index) < 0) {
        printf("예약 수정 저장에 실패했습니다.\n");
        return;
    }
    rebuild_tower_current_from_reservations();
    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "관리자 예약 수정 - 예약번호 %s", r.code);
    write_log_msg(logbuf);
    printf("예약 정보가 수정되었습니다.\n");
}

void admin_menu(void) {
    if (!admin_login()) return;
    while (1) {
        expire_old_reservations();
        print_line();
        printf("관리자 페이지\n");
        print_line();
        printf("1. 주차타워 정보 조회\n");
        printf("2. 주차타워별 예약 가능 개수 조회\n");
        printf("3. 현재 주차 상태 조회\n");
        printf("4. 전체 예약 목록 조회\n");
        printf("5. 현재 입차 차량 목록 조회\n");
        printf("6. 결제 내역 및 매출 조회\n");
        printf("7. 매출 통계 조회/CSV 생성\n");
        printf("8. 예약 만료 검사 실행\n");
        printf("9. 예약번호 발송 기록 조회\n");
        printf("10. 로그 조회\n");
        printf("11. 관리자 예약 수정\n");
        printf("12. 관리자 결제 수정\n");
        printf("13. 실시간 주차 가능 대수 자동 새로고침\n");
        printf("0. 뒤로가기\n");
        print_line();
        int choice = read_int("메뉴 선택: ", 0, 13);
        switch (choice) {
            case 1: show_tower_info(); pause_enter(); break;
            case 2: show_all_tower_capacity(); pause_enter(); break;
            case 3: show_current_parking_status(); pause_enter(); break;
            case 4: show_all_reservations(); pause_enter(); break;
            case 5: show_entered_reservations(); pause_enter(); break;
            case 6: show_payment_records(); pause_enter(); break;
            case 7: show_sales_statistics(); pause_enter(); break;
            case 8: expire_old_reservations(); printf("예약 만료 검사를 완료했습니다. 만료 예약은 삭제됩니다.\n"); pause_enter(); break;
            case 9: show_messages(); pause_enter(); break;
            case 10: show_logs(); pause_enter(); break;
            case 11: admin_modify_reservation(); pause_enter(); break;
            case 12: admin_modify_payment(); pause_enter(); break;
            case 13: auto_refresh_availability(); pause_enter(); break;
            case 0: return;
            default: break;
        }
    }
}
