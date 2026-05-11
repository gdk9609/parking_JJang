#include <stdio.h>
#include <string.h>
#include "parking.h"

int next_parking_id(void) {
    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    int max_id = 0;
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0 && p.id > max_id) max_id = p.id;
    }
    return max_id + 1;
}

int append_parking_record(const ParkingRecord *parking) {
    if (!parking) return -1;
    return append_record(PARKING_FILE, parking, sizeof(ParkingRecord));
}

int find_parking_by_code(const char *code, ParkingRecord *parking, int *index_out) {
    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0) {
            if (strcmp(p.reservation_code, code) == 0) {
                if (parking) *parking = p;
                if (index_out) *index_out = i;
                return 1;
            }
        }
    }
    return 0;
}

int delete_parking_record(int index) {
    return delete_record_at(PARKING_FILE, index, sizeof(ParkingRecord));
}

void process_entry(void) {
    expire_old_reservations();
    char code[MAX_CODE];
    Reservation r;
    int index;

    print_line();
    printf("무인 입차 처리\n");
    print_line();
    printf("입차는 예약번호가 있어야만 가능합니다.\n");
    printf("예약번호 입력: ");
    safe_read_line(code, sizeof(code));

    if (!find_reservation_by_code(code, &r, &index)) {
        ParkingRecord existing;
        if (find_parking_by_code(code, &existing, NULL)) {
            printf("이미 입차 처리된 차량입니다.\n");
            return;
        }
        printf("해당 예약번호가 없습니다. 만료, 취소, 입차, 출차 처리된 예약은 예약목록에서 삭제되었을 수 있습니다.\n");
        return;
    }

    if (r.status == RES_EXPIRED || r.status == RES_CANCELLED) {
        printf("취소 또는 만료된 예약입니다. 입차할 수 없습니다.\n");
        return;
    }
    if (r.status != RES_RESERVED) {
        printf("예약 완료 상태인 예약만 입차할 수 있습니다. 현재 상태: %s\n", reservation_status_name(r.status));
        return;
    }

    time_t now = now_time();
    if (difftime(now, r.reserved_at) > RESERVATION_TIMEOUT_SECONDS) {
        char logbuf[256];
        snprintf(logbuf, sizeof(logbuf), "입차 시도 중 예약 만료 삭제 - 예약번호 %s", r.code);
        write_log_msg(logbuf);
        delete_record_at(RESERVATIONS_FILE, index, sizeof(Reservation));
        printf("예약 후 20분이 지나 입차할 수 없습니다. 예약 데이터는 삭제되며 보증금은 반환되지 않습니다.\n");
        return;
    }

    if (increase_tower_current(r.tower_id, r.car_type) < 0) {
        printf("주차타워 상태 갱신에 실패했습니다.\n");
        return;
    }

    ParkingRecord p;
    memset(&p, 0, sizeof(p));
    p.id = next_parking_id();
    snprintf(p.reservation_code, sizeof(p.reservation_code), "%s", r.code);
    snprintf(p.car_number, sizeof(p.car_number), "%s", r.car_number);
    snprintf(p.phone, sizeof(p.phone), "%s", r.phone);
    p.tower_id = r.tower_id;
    p.car_type = r.car_type;
    p.reserved_at = r.reserved_at;
    p.entry_time = now;
    p.deposit = r.deposit;

    if (append_parking_record(&p) < 0) {
        decrease_tower_current(r.tower_id, r.car_type);
        printf("입차 정보 저장에 실패했습니다.\n");
        return;
    }

    if (delete_record_at(RESERVATIONS_FILE, index, sizeof(Reservation)) < 0) {
        int parking_index;
        if (find_parking_by_code(p.reservation_code, NULL, &parking_index)) delete_parking_record(parking_index);
        decrease_tower_current(r.tower_id, r.car_type);
        printf("예약목록 삭제에 실패하여 입차를 취소했습니다.\n");
        return;
    }

    char timebuf[64];
    char logbuf[256];
    format_time(p.entry_time, timebuf, sizeof(timebuf));
    snprintf(logbuf, sizeof(logbuf), "입차 완료 및 예약목록 삭제 - 예약번호 %s, 차량번호 %s", p.reservation_code, p.car_number);
    write_log_msg(logbuf);

    print_line();
    printf("입차가 완료되었습니다.\n");
    printf("입차 완료 후 해당 데이터는 예약목록에서 삭제되고 현재 입차 차량 목록으로 이동됩니다.\n");
    printf("예약번호: %s\n", p.reservation_code);
    printf("차량번호: %s\n", p.car_number);
    printf("전화번호: %s\n", p.phone);
    printf("차종: %s\n", car_type_name(p.car_type));
    printf("주차타워: %c Tower\n", 'A' + p.tower_id - 1);
    printf("입차시간: %s\n", timebuf);
    print_line();
}

void process_exit_and_payment(void) {
    expire_old_reservations();
    char code[MAX_CODE];
    ParkingRecord pking;
    int parking_index;
    char method[MAX_METHOD];

    print_line();
    printf("출차 및 요금 정산\n");
    print_line();
    printf("정산도 예약번호가 있어야만 가능합니다.\n");
    printf("예약번호 입력: ");
    safe_read_line(code, sizeof(code));

    if (!find_parking_by_code(code, &pking, &parking_index)) {
        printf("현재 입차 중인 차량에서 해당 예약번호를 찾을 수 없습니다.\n");
        return;
    }

    time_t exit_time = now_time();
    int actual_minutes = seconds_to_minutes_ceil(pking.entry_time, exit_time);
    int charged_minutes = charged_minutes_for_fee(actual_minutes);
    int parking_fee = calculate_parking_fee(actual_minutes);
    int deposit_used = parking_fee < RESERVATION_DEPOSIT ? parking_fee : RESERVATION_DEPOSIT;
    int final_fee = parking_fee - deposit_used;
    if (final_fee < 0) final_fee = 0;

    print_line();
    printf("정산 정보\n");
    print_line();
    char entrybuf[64], exitbuf[64], actualbuf[64], chargedbuf[64];
    format_time(pking.entry_time, entrybuf, sizeof(entrybuf));
    format_time(exit_time, exitbuf, sizeof(exitbuf));
    format_duration_minutes(actual_minutes, actualbuf, sizeof(actualbuf));
    format_duration_minutes(charged_minutes, chargedbuf, sizeof(chargedbuf));
    printf("예약번호: %s\n", pking.reservation_code);
    printf("차량번호: %s\n", pking.car_number);
    printf("입차시간: %s\n", entrybuf);
    printf("출차시간: %s\n", exitbuf);
    printf("실제 주차 시간: %s\n", actualbuf);
    printf("요금 적용 시간: %s\n", chargedbuf);
    printf("주차요금: %d원\n", parking_fee);
    printf("예약보증금 차감: %d원\n", deposit_used);
    printf("출차 시 추가 결제금액: %d원\n", final_fee);
    printf("매출 반영 금액: %d원\n", parking_fee);
    print_line();

    printf("결제 방식 선택\n");
    printf("1. 카드\n");
    printf("2. 현금\n");
    printf("3. 모바일 결제\n");
    int m = read_int("결제 방식: ", 1, 3);
    if (m == 1) snprintf(method, sizeof(method), "카드");
    else if (m == 2) snprintf(method, sizeof(method), "현금");
    else snprintf(method, sizeof(method), "모바일 결제");

    Payment p;
    memset(&p, 0, sizeof(p));
    p.id = count_records(PAYMENTS_FILE, sizeof(Payment)) + 1;
    snprintf(p.reservation_code, sizeof(p.reservation_code), "%s", pking.reservation_code);
    make_receipt_number(p.receipt_no, sizeof(p.receipt_no));
    snprintf(p.car_number, sizeof(p.car_number), "%s", pking.car_number);
    snprintf(p.phone, sizeof(p.phone), "%s", pking.phone);
    p.tower_id = pking.tower_id;
    p.car_type = pking.car_type;
    p.entry_time = pking.entry_time;
    p.exit_time = exit_time;
    p.total_minutes = actual_minutes;
    p.charged_minutes = charged_minutes;
    p.parking_fee = parking_fee;
    p.deposit_used = deposit_used;
    p.final_fee = final_fee;
    snprintf(p.method, sizeof(p.method), "%s", method);

    if (append_payment(&p) < 0) {
        printf("영수증 기록 저장에 실패했습니다.\n");
        return;
    }

    save_receipt_text(&p);

    if (delete_parking_record(parking_index) < 0) {
        printf("입차 데이터 삭제에 실패했습니다. 영수증 데이터는 저장되었으므로 관리자에게 문의하세요.\n");
        return;
    }
    if (delete_text_lines_matching(MESSAGES_FILE, pking.reservation_code, pking.car_number) < 0) {
        write_log_msg("예약번호 발송 기록 삭제 실패");
    }
    decrease_tower_current(pking.tower_id, pking.car_type);

    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "출차 완료 - 예약번호 %s, 차량번호 %s, 주차요금 %d원, 추가결제 %d원, 영수증 %s, 입차/발송 기록 삭제",
             pking.reservation_code, pking.car_number, parking_fee, final_fee, p.receipt_no);
    write_log_msg(logbuf);

    print_line();
    printf("결제가 완료되었습니다.\n");
    printf("출차 완료 후 예약번호 발송 기록과 현재 입차 데이터는 삭제되고 전자 영수증 기록만 보존됩니다.\n");
    printf("전자 영수증 번호: %s\n", p.receipt_no);
    printf("주차요금/매출 반영 금액: %d원\n", parking_fee);
    printf("출차 시 추가 결제금액: %d원\n", final_fee);
    printf("결제방식: %s\n", method);
    print_line();
    print_receipt(&p);
}
