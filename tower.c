#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "parking.h"

static void make_tower(ParkingTower *t, int id, const char *name, int total,
                       int compact, int midsize, int suv, int large, int ev) {
    memset(t, 0, sizeof(*t));
    t->id = id;
    snprintf(t->name, sizeof(t->name), "%s", name);
    t->total_capacity = total;
    t->capacity[CAR_COMPACT] = compact;
    t->capacity[CAR_MIDSIZE] = midsize;
    t->capacity[CAR_SUV] = suv;
    t->capacity[CAR_LARGE] = large;
    t->capacity[CAR_EV] = ev;
    snprintf(t->operating_hours, sizeof(t->operating_hours), "%s", DEFAULT_OPERATING_HOURS);
    t->active = 1;
}

int init_default_towers(void) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    if (count > 0) return 0;
    ParkingTower a, b, c;
    make_tower(&a, 1, "A Tower", 20, 3, 6, 4, 2, 5);
    make_tower(&b, 2, "B Tower", 50, 8, 17, 10, 5, 10);
    make_tower(&c, 3, "C Tower", 70, 10, 25, 15, 8, 12);
    if (append_record(TOWERS_FILE, &a, sizeof(a)) < 0) return -1;
    if (append_record(TOWERS_FILE, &b, sizeof(b)) < 0) return -1;
    if (append_record(TOWERS_FILE, &c, sizeof(c)) < 0) return -1;
    write_log_msg("기본 주차타워 데이터 생성");
    return 0;
}

int get_tower_by_id(int tower_id, ParkingTower *tower, int *index_out) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    for (int i = 0; i < count; i++) {
        ParkingTower tmp;
        if (read_record_at(TOWERS_FILE, i, &tmp, sizeof(tmp)) == 0) {
            if (tmp.id == tower_id && tmp.active) {
                if (tower) *tower = tmp;
                if (index_out) *index_out = i;
                return 1;
            }
        }
    }
    return 0;
}

int update_tower(const ParkingTower *tower, int index) {
    if (!tower) return -1;
    return update_record_at(TOWERS_FILE, index, tower, sizeof(ParkingTower));
}

void show_tower_simple_list(void) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            printf("%d. %s / 총 %d대 / %s\n", t.id, t.name, t.total_capacity, t.operating_hours);
        }
    }
}

int choose_tower(void) {
    int tower_id;
    printf("\n주차타워를 선택하세요.\n");
    show_tower_simple_list();
    while (1) {
        tower_id = read_int("주차타워 선택: ", 1, 999);
        if (get_tower_by_id(tower_id, NULL, NULL)) return tower_id;
        printf("존재하지 않는 주차타워입니다. 다시 선택하세요.\n");
    }
}

int get_available_slots(int tower_id, int car_type) {
    ParkingTower t;
    if (!get_tower_by_id(tower_id, &t, NULL)) return -1;
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) return -1;
    int reserved = count_active_reservations(tower_id, car_type);
    int available = t.capacity[car_type] - t.current[car_type] - reserved;
    if (available < 0) available = 0;
    return available;
}

int increase_tower_current(int tower_id, int car_type) {
    ParkingTower t;
    int index;
    if (!get_tower_by_id(tower_id, &t, &index)) return -1;
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) return -1;
    if (t.current[car_type] >= t.capacity[car_type]) return -1;
    t.current[car_type]++;
    return update_tower(&t, index);
}

int decrease_tower_current(int tower_id, int car_type) {
    ParkingTower t;
    int index;
    if (!get_tower_by_id(tower_id, &t, &index)) return -1;
    if (car_type < 1 || car_type > CAR_TYPE_COUNT) return -1;
    if (t.current[car_type] > 0) t.current[car_type]--;
    return update_tower(&t, index);
}

int rebuild_tower_current_from_reservations(void) {
    int tower_count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    for (int i = 0; i < tower_count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            for (int type = 1; type <= CAR_TYPE_COUNT; type++) t.current[type] = 0;
            int res_count = count_records(RESERVATIONS_FILE, sizeof(Reservation));
            for (int j = 0; j < res_count; j++) {
                Reservation r;
                if (read_record_at(RESERVATIONS_FILE, j, &r, sizeof(r)) == 0 && r.status == RES_ENTERED && r.tower_id == t.id) {
                    if (r.car_type >= 1 && r.car_type <= CAR_TYPE_COUNT && t.current[r.car_type] < t.capacity[r.car_type]) {
                        t.current[r.car_type]++;
                    }
                }
            }
            update_tower(&t, i);
        }
    }
    return 0;
}

int recommend_tower_for_car_type(int car_type) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    int best_id = 0;
    int best_available = -1;
    double best_ratio = 999.0;
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            int available = get_available_slots(t.id, car_type);
            int current_total = 0;
            for (int type = 1; type <= CAR_TYPE_COUNT; type++) current_total += t.current[type];
            double ratio = t.total_capacity > 0 ? (double)current_total / (double)t.total_capacity : 1.0;
            if (available > best_available || (available == best_available && ratio < best_ratio)) {
                best_available = available;
                best_ratio = ratio;
                best_id = t.id;
            }
        }
    }
    if (best_available <= 0) return 0;
    return best_id;
}

void show_recommended_tower(void) {
    expire_old_reservations();
    int car_type = choose_car_type();
    int tower_id = recommend_tower_for_car_type(car_type);
    print_line();
    printf("%s 기준 최적 주차타워 추천\n", car_type_name(car_type));
    print_line();
    if (tower_id == 0) {
        printf("현재 해당 차종으로 예약 가능한 주차타워가 없습니다.\n");
    } else {
        ParkingTower t;
        get_tower_by_id(tower_id, &t, NULL);
        printf("추천 타워: %s\n", t.name);
        printf("남은 %s 자리: %d대\n", car_type_name(car_type), get_available_slots(tower_id, car_type));
        printf("추천 기준: 해당 차종 잔여 대수가 많고, 전체 혼잡도가 낮은 타워 우선\n");
    }
    print_line();
}

void show_tower_info(void) {
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    print_line();
    printf("주차타워 정보\n");
    print_line();
    printf("요금 정책: 기본 1시간 %d원 / 추가 10분당 %d원 / 1일 최대 %d원\n\n", BASE_FEE, EXTRA_UNIT_FEE, DAILY_MAX_FEE);
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            printf("[%s] 총 수용대수: %d대 / 운영시간: %s\n", t.name, t.total_capacity, t.operating_hours);
            for (int type = 1; type <= CAR_TYPE_COUNT; type++) {
                printf("- %s: %d대\n", car_type_name(type), t.capacity[type]);
            }
            printf("\n");
        }
    }
    print_line();
}

void show_all_tower_capacity(void) {
    expire_old_reservations();
    rebuild_tower_current_from_reservations();
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    print_line();
    printf("주차타워별 차종별 예약 가능 대수\n");
    print_line();
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            printf("[%s] / 운영시간: %s\n", t.name, t.operating_hours);
            int total_available = 0;
            for (int type = 1; type <= CAR_TYPE_COUNT; type++) {
                int reserved = count_active_reservations(t.id, type);
                int available = t.capacity[type] - t.current[type] - reserved;
                if (available < 0) available = 0;
                total_available += available;
                printf("- %s: %d / %d  (주차중 %d, 예약중 %d)\n",
                       car_type_name(type), available, t.capacity[type], t.current[type], reserved);
            }
            printf("- 전체 예약 가능: %d / %d\n\n", total_available, t.total_capacity);
        }
    }
    print_line();
}

void show_current_parking_status(void) {
    expire_old_reservations();
    rebuild_tower_current_from_reservations();
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    print_line();
    printf("현재 주차타워 주차 상태\n");
    print_line();
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            int current_total = 0;
            for (int type = 1; type <= CAR_TYPE_COUNT; type++) current_total += t.current[type];
            printf("[%s] 현재 주차: %d / %d / 운영시간: %s\n", t.name, current_total, t.total_capacity, t.operating_hours);
            for (int type = 1; type <= CAR_TYPE_COUNT; type++) {
                printf("- %s: %d / %d\n", car_type_name(type), t.current[type], t.capacity[type]);
            }
            printf("\n");
        }
    }
    print_line();
}

void show_car_type_availability(void) {
    expire_old_reservations();
    int car_type = choose_car_type();
    int count = count_records(TOWERS_FILE, sizeof(ParkingTower));
    print_line();
    printf("%s 예약 가능 여부\n", car_type_name(car_type));
    print_line();
    for (int i = 0; i < count; i++) {
        ParkingTower t;
        if (read_record_at(TOWERS_FILE, i, &t, sizeof(t)) == 0 && t.active) {
            int available = get_available_slots(t.id, car_type);
            printf("%s: %s / 남은 %s 자리 %d대\n",
                   t.name, available > 0 ? "예약 가능" : "예약 불가", car_type_name(car_type), available);
        }
    }
    print_line();
}

void auto_refresh_availability(void) {
    int seconds = read_int("새로고침 간격(초, 1~60): ", 1, 60);
    int repeat = read_int("새로고침 횟수(1~100): ", 1, 100);
    for (int i = 0; i < repeat; i++) {
        printf("\033[2J\033[H");
        printf("실시간 주차 가능 대수 자동 새로고침 (%d/%d)\n", i + 1, repeat);
        show_all_tower_capacity();
        if (i < repeat - 1) sleep((unsigned int)seconds);
    }
}
