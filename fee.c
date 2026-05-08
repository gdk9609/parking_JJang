#include <stdio.h>
#include "parking.h"

static int fee_for_one_day_part(int minutes) {
    if (minutes <= 0) return 0;
    if (minutes <= BASE_MINUTES) return BASE_FEE;
    int extra = minutes - BASE_MINUTES;
    int units = (extra + EXTRA_UNIT_MINUTES - 1) / EXTRA_UNIT_MINUTES;
    int fee = BASE_FEE + units * EXTRA_UNIT_FEE;
    if (fee > DAILY_MAX_FEE) fee = DAILY_MAX_FEE;
    return fee;
}

int calculate_parking_fee(int actual_minutes) {
    if (actual_minutes <= 0) actual_minutes = 1;
    int full_days = actual_minutes / 1440;
    int rem = actual_minutes % 1440;
    int fee = full_days * DAILY_MAX_FEE;
    if (rem > 0) fee += fee_for_one_day_part(rem);
    if (rem == 0 && full_days == 0) fee = BASE_FEE;
    return fee;
}

void run_fee_calculator(void) {
    char entry[64];
    char exit_time[64];
    time_t entry_t;
    time_t exit_t;
    int total_minutes;
    int charged_minutes;
    int fee;
    char duration[64], charged[64];

    print_line();
    printf("예상 주차요금 계산기\n");
    print_line();
    printf("요금 정책: 기본 1시간 %d원, 추가 10분당 %d원, 1일 최대 %d원\n", BASE_FEE, EXTRA_UNIT_FEE, DAILY_MAX_FEE);
    printf("입력 형식: YYYY-MM-DD HH:MM  예) 2026-05-08 09:30\n");

    while (1) {
        printf("예상 입차 날짜/시각 입력: ");
        safe_read_line(entry, sizeof(entry));
        if (parse_datetime(entry, &entry_t)) break;
        printf("날짜/시간 형식이 올바르지 않습니다. YYYY-MM-DD HH:MM 형식으로 입력하세요.\n");
    }

    while (1) {
        printf("예상 출차 날짜/시각 입력: ");
        safe_read_line(exit_time, sizeof(exit_time));
        if (parse_datetime(exit_time, &exit_t) && exit_t > entry_t) break;
        printf("출차 날짜/시간은 입차 이후여야 합니다.\n");
    }

    total_minutes = seconds_to_minutes_ceil(entry_t, exit_t);
    charged_minutes = charged_minutes_for_fee(total_minutes);
    fee = calculate_parking_fee(total_minutes);
    format_duration_minutes(total_minutes, duration, sizeof(duration));
    format_duration_minutes(charged_minutes, charged, sizeof(charged));

    print_line();
    printf("예상 주차 시간: %s\n", duration);
    printf("요금 적용 시간: %s\n", charged);
    printf("예상 주차요금: %d원\n", fee);
    printf("예약보증금 %d원은 정상 입차 후 정산 시 주차요금에서 차감됩니다.\n", RESERVATION_DEPOSIT);
    print_line();
}
