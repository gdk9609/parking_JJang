#include <stdio.h>
#include <string.h>
#include "parking.h"

static int admin_login(void) {
    char admin_id[64];
    printf("관리자 아이디 입력: ");
    safe_read_line(admin_id, sizeof(admin_id));
    if (strcmp(admin_id, ADMIN_ID) == 0) {
        write_log_msg("관리자 로그인 성공");
        return 1;
    }
    write_log_msg("관리자 로그인 실패");
    printf("관리자 아이디가 틀렸습니다.\n");
    return 0;
}

void admin_modify_entry_time(void) {
    char code[MAX_CODE];
    ParkingRecord p;
    int index;

    printf("입차시간을 수정할 예약번호 입력: ");
    safe_read_line(code, sizeof(code));

    if (!find_parking_by_code(code, &p, &index)) {
        printf("현재 입차 중인 차량에서 해당 예약번호를 찾을 수 없습니다.\n");
        printf("예약 상태 차량이나 이미 출차한 차량의 시간은 이 메뉴에서 수정하지 않습니다.\n");
        return;
    }

    while (1) {
        char entry[64], duration[64];
        int elapsed = seconds_to_minutes_ceil(p.entry_time, now_time());
        format_time(p.entry_time, entry, sizeof(entry));
        format_duration_minutes(elapsed, duration, sizeof(duration));

        print_line();
        printf("관리자 입차시간 수정\n");
        print_line();
        printf("예약번호: %s\n", p.reservation_code);
        printf("차량번호: %s\n", p.car_number);
        printf("전화번호: %s\n", p.phone);
        printf("주차타워: %c Tower\n", 'A' + p.tower_id - 1);
        printf("차종: %s\n", car_type_name(p.car_type));
        printf("현재 입차시간: %s\n", entry);
        printf("현재 경과시간: %s\n", duration);
        print_line();
        printf("1. 입차시간 수정\n");
        printf("0. 나가기\n");
        int choice = read_int("메뉴 선택: ", 0, 1);
        if (choice == 0) return;

        char input[64];
        time_t new_entry;
        printf("새 입차시간 입력 (YYYY-MM-DD HH:MM): ");
        safe_read_line(input, sizeof(input));
        if (!parse_datetime(input, &new_entry)) {
            printf("날짜 형식이 올바르지 않습니다. 예: 2026-05-08 13:30\n");
            continue;
        }
        if (new_entry > now_time()) {
            printf("입차시간은 현재 시각보다 미래일 수 없습니다.\n");
            continue;
        }

        p.entry_time = new_entry;
        if (update_record_at(PARKING_FILE, index, &p, sizeof(ParkingRecord)) < 0) {
            printf("입차시간 수정 저장에 실패했습니다.\n");
            return;
        }

        char logbuf[256];
        snprintf(logbuf, sizeof(logbuf), "관리자 입차시간 수정 - 예약번호 %s, 차량번호 %s", p.reservation_code, p.car_number);
        write_log_msg(logbuf);
        printf("입차시간이 수정되었습니다.\n");
        return;
    }
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
        printf("6. 영수증 기록 및 매출 조회\n");
        printf("7. 매출 통계 조회/CSV 생성\n");
        printf("8. 예약 만료 검사 실행\n");
        printf("9. 예약번호 발송 기록 조회\n");
        printf("10. 로그 조회\n");
        printf("11. 현재 입차 차량 입차시간 수정\n");
        printf("12. 실시간 주차 가능 대수 자동 새로고침\n");
        printf("0. 뒤로가기\n");
        print_line();
        int choice = read_int("메뉴 선택: ", 0, 12);
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
            case 11: admin_modify_entry_time(); pause_enter(); break;
            case 12: auto_refresh_availability(); pause_enter(); break;
            case 0: return;
            default: break;
        }
    }
}
