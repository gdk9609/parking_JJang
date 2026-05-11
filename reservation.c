#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include "parking.h"

static pthread_t expire_thread_id;
static int expire_thread_running = 0;

static int parking_code_exists_in_payments(const char *code) {
    int count = count_records(PAYMENTS_FILE, sizeof(Payment));
    for (int i = 0; i < count; i++) {
        Payment p;
        if (read_record_at(PAYMENTS_FILE, i, &p, sizeof(p)) == 0 && strcmp(p.reservation_code, code) == 0) return 1;
    }
    return 0;
}

static int reservation_code_exists_anywhere(const char *code) {
    if (find_reservation_by_code(code, NULL, NULL)) return 1;
    if (find_parking_by_code(code, NULL, NULL)) return 1;
    if (parking_code_exists_in_payments(code)) return 1;
    return 0;
}

static void make_random_reservation_code(char *out, size_t size) {
    const char *chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int char_count = 62;
    if (!out || size < 6) return;

    for (int attempt = 0; attempt < 10000; attempt++) {
        int has_upper = 0;
        int has_lower = 0;
        for (int i = 0; i < 5; i++) {
            char c = chars[rand() % char_count];
            out[i] = c;
            if (isupper((unsigned char)c)) has_upper = 1;
            if (islower((unsigned char)c)) has_lower = 1;
        }
        out[5] = '\0';
        if (has_upper && has_lower && !reservation_code_exists_anywhere(out)) return;
    }

    snprintf(out, size, "A%04d", rand() % 10000);
}

static int has_active_reservation_for_car(const char *car_number) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    time_t now = now_time();
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (strcmp(r.car_number, car_number) == 0 && r.status == RES_RESERVED &&
                difftime(now, r.reserved_at) <= RESERVATION_TIMEOUT_SECONDS) return 1;
        }
    }

    count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0 && strcmp(p.car_number, car_number) == 0) return 1;
    }
    return 0;
}

static void send_reservation_message(const Reservation *r) {
    char timebuf[64];
    char line[512];
    format_time(now_time(), timebuf, sizeof(timebuf));
    snprintf(line, sizeof(line), "[%s] TO %s: 예약번호 %s / 차량번호 %s / %s / %c Tower",
             timebuf, r->phone, r->code, r->car_number, car_type_name(r->car_type), 'A' + r->tower_id - 1);
    append_text_line(MESSAGES_FILE, line);
}

static int remaining_seconds_for_reservation(const Reservation *r) {
    if (!r || r->status != RES_RESERVED) return 0;
    int elapsed = (int)difftime(now_time(), r->reserved_at);
    int remaining = RESERVATION_TIMEOUT_SECONDS - elapsed;
    return remaining > 0 ? remaining : 0;
}

static void show_towers_for_selected_car_type(int car_type) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    printf("\n%s 기준 주차타워 목록\n", car_type_name(car_type));
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            int available = get_available_slots(t.id, car_type);
            printf("%d. %s / 남은 %s 자리 %d대 / 운영시간 %s\n",
                   t.id, t.name, car_type_name(car_type), available, t.operating_hours);
        }
    }
}

static int choose_tower_after_car_type(int car_type) {
    int recommended = recommend_tower_for_car_type(car_type);

    show_towers_for_selected_car_type(car_type);
    if (recommended == 0) {
        printf("현재 해당 차종으로 예약 가능한 주차타워가 없습니다.\n");
        return 0;
    }

    ParkingTower rec_tower;
    if (get_tower_by_id(recommended, &rec_tower, NULL)) {
        printf("\n추천 타워: %s / 남은 %s 자리 %d대\n",
               rec_tower.name, car_type_name(car_type), get_available_slots(recommended, car_type));
        if (ask_yes_no("추천 타워로 배정하시겠습니까? (y/n): ")) return recommended;
    }

    while (1) {
        int tower_id = read_int("주차타워 선택: ", 1, 999);
        ParkingTower t;
        if (!get_tower_by_id(tower_id, &t, NULL)) {
            printf("존재하지 않는 주차타워입니다. 다시 선택하세요.\n");
            continue;
        }
        int available = get_available_slots(tower_id, car_type);
        if (available <= 0) {
            printf("선택한 타워에는 %s 예약 가능 자리가 없습니다. 다시 선택하세요.\n", car_type_name(car_type));
            continue;
        }
        return tower_id;
    }
}

int next_reservation_id(void) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    int max_id = 0;
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0 && r.id > max_id) max_id = r.id;
    }
    return max_id + 1;
}

void expire_old_reservations(void) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    time_t now = now_time();
    int i = 0;
    while (i < count) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (r.status == RES_RESERVED && difftime(now, r.reserved_at) > RESERVATION_TIMEOUT_SECONDS) {
                char logbuf[256];
                snprintf(logbuf, sizeof(logbuf), "예약 만료 삭제 - 예약번호 %s, 차량번호 %s, 보증금 미반환", r.code, r.car_number);
                write_log_msg(logbuf);
                delete_record_at(RESERVATIONS_FILE, i, sizeof(Reservation));
                count--;
                continue;
            }
        }
        i++;
    }
}

static void *expiration_thread_func(void *arg) {
    (void)arg;
    while (expire_thread_running) {
        expire_old_reservations();
        for (int i = 0; i < 10 && expire_thread_running; i++) sleep(1);
    }
    return NULL;
}

int start_expiration_thread(void) {
    if (expire_thread_running) return 0;
    expire_thread_running = 1;
    if (pthread_create(&expire_thread_id, NULL, expiration_thread_func, NULL) != 0) {
        expire_thread_running = 0;
        write_log_msg("예약 만료 스레드 시작 실패");
        return -1;
    }
    write_log_msg("예약 만료 스레드 시작");
    return 0;
}

void stop_expiration_thread(void) {
    if (!expire_thread_running) return;
    expire_thread_running = 0;
    pthread_join(expire_thread_id, NULL);
    write_log_msg("예약 만료 스레드 종료");
}

int count_active_reservations(int tower_id, int car_type) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    int active = 0;
    time_t now = now_time();
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (r.tower_id == tower_id && r.car_type == car_type && r.status == RES_RESERVED) {
                if (difftime(now, r.reserved_at) <= RESERVATION_TIMEOUT_SECONDS) active++;
            }
        }
    }
    return active;
}

int find_reservation_by_code(const char *code, Reservation *res, int *index_out) {
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (strcmp(r.code, code) == 0) {
                if (res) *res = r;
                if (index_out) *index_out = i;
                return 1;
            }
        }
    }
    return 0;
}

int update_reservation(const Reservation *res, int index) {
    if (!res) return -1;
    return update_record_at(RESERVATIONS_FILE, index, res, sizeof(Reservation));
}

void create_reservation(void) {
    expire_old_reservations();
    Reservation r;
    memset(&r, 0, sizeof(r));
    r.id = next_reservation_id();
    make_random_reservation_code(r.code, sizeof(r.code));

    print_line();
    printf("주차 예약\n");
    print_line();
    printf("주의: 예약 후 20분 이내 입차하지 않으면 예약 데이터가 자동 삭제되며 보증금 %d원은 반환되지 않습니다.\n", RESERVATION_DEPOSIT);
    printf("예약 완료 후 예약번호는 전화번호로 발송된 것으로 기록됩니다.\n\n");

    read_car_number(r.car_number, sizeof(r.car_number));
    if (has_active_reservation_for_car(r.car_number)) {
        printf("이미 예약 중이거나 주차 중인 차량입니다.\n");
        return;
    }
    read_phone_number(r.phone, sizeof(r.phone));

    r.car_type = choose_car_type();
    r.tower_id = choose_tower_after_car_type(r.car_type);
    if (r.tower_id == 0) return;

    r.reserved_at = now_time();
    r.entry_time = 0;
    r.exit_time = 0;
    r.status = RES_RESERVED;
    r.deposit = RESERVATION_DEPOSIT;
    r.deposit_forfeited = 0;
    r.total_fee = 0;
    r.final_paid = 0;

    if (append_record(RESERVATIONS_FILE, &r, sizeof(r)) < 0) {
        printf("예약 저장에 실패했습니다.\n");
        return;
    }

    send_reservation_message(&r);
    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "예약 생성 - 예약번호 %s, 차량번호 %s", r.code, r.car_number);
    write_log_msg(logbuf);

    char timebuf[64];
    format_time(r.reserved_at, timebuf, sizeof(timebuf));
    print_line();
    printf("예약이 완료되었습니다.\n");
    printf("예약번호: %s\n", r.code);
    printf("차량번호: %s\n", r.car_number);
    printf("전화번호: %s\n", r.phone);
    printf("차종: %s\n", car_type_name(r.car_type));
    printf("주차타워: %c Tower\n", 'A' + r.tower_id - 1);
    printf("운영시간: %s\n", DEFAULT_OPERATING_HOURS);
    printf("예약시각: %s\n", timebuf);
    printf("예약 제한시간: 20분\n");
    printf("예약보증금: %d원\n", r.deposit);
    printf("전화번호로 예약번호가 발송되었습니다. 실제 문자 대신 data/messages.txt에 기록됩니다.\n");
    print_line();
}

void lookup_reservation(void) {
    expire_old_reservations();
    char code[MAX_CODE];
    Reservation r;
    int index;
    char reserved_at[64];
    printf("예약번호 입력: ");
    safe_read_line(code, sizeof(code));
    if (!find_reservation_by_code(code, &r, &index)) {
        ParkingRecord p;
        if (find_parking_by_code(code, &p, NULL)) {
            printf("이미 입차 완료된 차량입니다. 입차 후에는 예약목록에서 삭제되고 현재 입차 차량 데이터로 관리됩니다.\n");
            return;
        }
        printf("해당 예약번호를 찾을 수 없습니다. 만료, 입차, 출차 처리된 예약은 예약목록에서 삭제되었을 수 있습니다.\n");
        return;
    }
    format_time(r.reserved_at, reserved_at, sizeof(reserved_at));
    print_line();
    printf("예약 조회 결과\n");
    print_line();
    printf("예약번호: %s\n", r.code);
    printf("차량번호: %s\n", r.car_number);
    printf("전화번호: %s\n", r.phone);
    printf("차종: %s\n", car_type_name(r.car_type));
    printf("주차타워: %c Tower\n", 'A' + r.tower_id - 1);
    printf("운영시간: %s\n", DEFAULT_OPERATING_HOURS);
    printf("예약시각: %s\n", reserved_at);
    printf("상태: %s\n", reservation_status_name(r.status));
    if (r.status == RES_RESERVED) {
        int rem = remaining_seconds_for_reservation(&r);
        printf("입차 제한시간 카운트다운: %d분 %d초 안에 입차해야 합니다.\n", rem / 60, rem % 60);
    }
    printf("예약보증금: %d원\n", r.deposit);
    print_line();
}

void cancel_reservation(void) {
    expire_old_reservations();
    char code[MAX_CODE];
    Reservation r;
    int index;
    printf("취소할 예약번호 입력: ");
    safe_read_line(code, sizeof(code));
    if (!find_reservation_by_code(code, &r, &index)) {
        printf("해당 예약번호를 찾을 수 없습니다. 이미 입차/만료/출차 처리되었을 수 있습니다.\n");
        return;
    }
    if (r.status != RES_RESERVED) {
        printf("예약 완료 상태만 취소할 수 있습니다. 현재 상태: %s\n", reservation_status_name(r.status));
        return;
    }
    if (!ask_yes_no("정말 예약을 취소하시겠습니까? (y/n): ")) return;
    if (delete_record_at(RESERVATIONS_FILE, index, sizeof(Reservation)) < 0) {
        printf("예약 취소 삭제에 실패했습니다.\n");
        return;
    }
    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "예약 취소 삭제 - 예약번호 %s, 차량번호 %s", r.code, r.car_number);
    write_log_msg(logbuf);
    printf("예약이 취소되어 예약목록에서 삭제되었습니다.\n");
}

void change_reservation(void) {
    expire_old_reservations();
    char code[MAX_CODE];
    Reservation r;
    int index;
    printf("변경할 예약번호 입력: ");
    safe_read_line(code, sizeof(code));
    if (!find_reservation_by_code(code, &r, &index)) {
        printf("해당 예약번호를 찾을 수 없습니다. 이미 입차/만료/출차 처리되었을 수 있습니다.\n");
        return;
    }
    if (r.status != RES_RESERVED) {
        printf("예약 완료 상태만 변경할 수 있습니다. 현재 상태: %s\n", reservation_status_name(r.status));
        return;
    }
    int rem = remaining_seconds_for_reservation(&r);
    if (rem <= 0) {
        expire_old_reservations();
        printf("예약 제한시간이 지나 변경할 수 없습니다.\n");
        return;
    }
    printf("현재 남은 입차 제한시간: %d분 %d초\n", rem / 60, rem % 60);
    printf("주의: 예약을 변경해도 20분 제한시간은 새로 늘어나지 않습니다.\n");

    int new_type = choose_car_type();
    int new_tower = choose_tower_after_car_type(new_type);
    if (new_tower == 0) return;

    r.car_type = new_type;
    r.tower_id = new_tower;
    if (update_reservation(&r, index) < 0) {
        printf("예약 변경 저장에 실패했습니다.\n");
        return;
    }
    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "예약 변경 - 예약번호 %s, %c Tower, %s", r.code, 'A' + r.tower_id - 1, car_type_name(r.car_type));
    write_log_msg(logbuf);
    printf("예약이 변경되었습니다. 남은 입차 제한시간은 기존 예약 기준으로 유지됩니다.\n");
}

void find_reservation_code_by_identity(void) {
    expire_old_reservations();
    char car_number[MAX_NAME];
    char phone[MAX_PHONE];
    int found = 0;
    read_car_number(car_number, sizeof(car_number));
    read_phone_number(phone, sizeof(phone));
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    print_line();
    printf("예약번호 확인 결과\n");
    print_line();
    for (int i = 0; i < count; i++) {
        Reservation r;
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            if (strcmp(r.car_number, car_number) == 0 && strcmp(r.phone, phone) == 0 && r.status == RES_RESERVED) {
                found = 1;
                printf("예약번호: %s / 상태: 예약 완료 / %c Tower / %s\n", r.code, 'A' + r.tower_id - 1, car_type_name(r.car_type));
                int rem = remaining_seconds_for_reservation(&r);
                printf("남은 입차 제한시간: %d분 %d초\n", rem / 60, rem % 60);
            }
        }
    }

    count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0) {
            if (strcmp(p.car_number, car_number) == 0 && strcmp(p.phone, phone) == 0) {
                found = 1;
                printf("예약번호: %s / 상태: 입차 완료 / %c Tower / %s\n",
                       p.reservation_code, 'A' + p.tower_id - 1, car_type_name(p.car_type));
            }
        }
    }
    if (!found) printf("차량번호와 전화번호가 일치하는 활성 예약 또는 입차 내역이 없습니다.\n");
    print_line();
}

void lookup_entered_parking_status(void) {
    expire_old_reservations();
    char car_number[MAX_NAME];
    char phone[MAX_PHONE];
    int found = 0;
    read_car_number(car_number, sizeof(car_number));
    read_phone_number(phone, sizeof(phone));
    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    print_line();
    printf("입차 내역 및 현재 예상요금 조회\n");
    print_line();
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0) {
            if (strcmp(p.car_number, car_number) == 0 && strcmp(p.phone, phone) == 0) {
                char entry[64], duration[64];
                int elapsed = seconds_to_minutes_ceil(p.entry_time, now_time());
                int expected_fee = calculate_parking_fee(elapsed);
                int final_fee = expected_fee - RESERVATION_DEPOSIT;
                if (final_fee < 0) final_fee = 0;
                format_time(p.entry_time, entry, sizeof(entry));
                format_duration_minutes(elapsed, duration, sizeof(duration));
                found = 1;
                printf("예약번호: %s\n", p.reservation_code);
                printf("입차시간: %s\n", entry);
                printf("입차 경과시간: %s\n", duration);
                printf("현재 예상 주차요금: %d원\n", expected_fee);
                printf("예약보증금 차감 후 예상 정산금액: %d원\n", final_fee);
            }
        }
    }
    if (!found) printf("현재 입차 중인 내역이 없습니다.\n");
    print_line();
}

void show_all_reservations(void) {
    expire_old_reservations();
    int count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
    print_line();
    printf("전체 예약 목록\n");
    print_line();
    if (count == 0) {
        printf("예약 기록이 없습니다. 입차 완료된 차량은 현재 입차 차량 목록에서 확인하세요.\n");
        print_line();
        return;
    }
    for (int i = 0; i < count; i++) {
        Reservation r;
        char reserved_at[64];
        if (read_record_at(RESERVATIONS_FILE, i, &r, sizeof(r)) == 0) {
            format_time(r.reserved_at, reserved_at, sizeof(reserved_at));
            printf("%s | %s | %s | %c Tower | %s | %s",
                   r.code, r.car_number, car_type_name(r.car_type), 'A' + r.tower_id - 1,
                   reserved_at, reservation_status_name(r.status));
            if (r.status == RES_RESERVED) {
                int rem = remaining_seconds_for_reservation(&r);
                printf(" | 남은시간 %d분 %d초", rem / 60, rem % 60);
            }
            printf("\n");
        }
    }
    print_line();
}

void show_entered_reservations(void) {
    expire_old_reservations();
    int count = count_records(PARKING_FILE, sizeof(ParkingRecord));
    print_line();
    printf("현재 입차 차량 목록\n");
    print_line();
    int found = 0;
    for (int i = 0; i < count; i++) {
        ParkingRecord p;
        char entry[64], duration[64];
        if (read_record_at(PARKING_FILE, i, &p, sizeof(p)) == 0) {
            int elapsed = seconds_to_minutes_ceil(p.entry_time, now_time());
            int expected_fee = calculate_parking_fee(elapsed);
            found = 1;
            format_time(p.entry_time, entry, sizeof(entry));
            format_duration_minutes(elapsed, duration, sizeof(duration));
            printf("%s | %s | %s | %c Tower | 입차 %s | 경과 %s | 현재예상요금 %d원\n",
                   p.reservation_code, p.car_number, car_type_name(p.car_type), 'A' + p.tower_id - 1, entry, duration, expected_fee);
        }
    }
    if (!found) printf("현재 입차 중인 차량이 없습니다.\n");
    print_line();
}
