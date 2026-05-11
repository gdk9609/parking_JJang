#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parking.h"

static int receipt_sales_amount(const Payment *p) {
    if (!p) return 0;
    return p->parking_fee;
}

int append_payment(const Payment *payment) {
    if (!payment) return -1;
    return append_record(PAYMENTS_FILE, payment, sizeof(Payment));
}

char *make_receipt_number(char *buf, size_t size) {
    char timebuf[32];
    time_t t = now_time();
    struct tm *tm_ptr = localtime(&t);
    if (!buf || size == 0) return buf;
    if (tm_ptr) strftime(timebuf, sizeof(timebuf), "%Y%m%d%H%M%S", tm_ptr);
    else snprintf(timebuf, sizeof(timebuf), "00000000000000");
    snprintf(buf, size, "E%s%04d", timebuf, rand() % 10000);
    return buf;
}

void print_receipt(const Payment *p) {
    if (!p) return;
    char entry[64], exit_time[64], duration[64];
    format_time(p->entry_time, entry, sizeof(entry));
    format_time(p->exit_time, exit_time, sizeof(exit_time));
    format_duration_minutes(p->total_minutes, duration, sizeof(duration));
    print_line();
    printf("전자 영수증\n");
    print_line();
    printf("영수증 번호: %s\n", p->receipt_no);
    printf("예약번호: %s\n", p->reservation_code);
    printf("차량번호: %s\n", p->car_number);
    printf("전화번호: %s\n", p->phone);
    printf("주차타워: %c Tower\n", 'A' + p->tower_id - 1);
    printf("차종: %s\n", car_type_name(p->car_type));
    printf("입차시간: %s\n", entry);
    printf("출차시간: %s\n", exit_time);
    printf("주차시간: %s\n", duration);
    printf("주차요금: %d원\n", p->parking_fee);
    printf("예약보증금 차감: %d원\n", p->deposit_used);
    printf("출차 시 추가 결제금액: %d원\n", p->final_fee);
    printf("매출 반영 금액: %d원\n", receipt_sales_amount(p));
    printf("결제방식: %s\n", p->method);
    print_line();
}

void save_receipt_text(const Payment *p) {
    if (!p) return;
    char entry[64], exit_time[64], line[1024];
    format_time(p->entry_time, entry, sizeof(entry));
    format_time(p->exit_time, exit_time, sizeof(exit_time));
    snprintf(line, sizeof(line),
             "영수증번호 %s | 예약 %s | 차량 %s | 전화 %s | %c Tower | %s | 입차 %s | 출차 %s | 주차요금 %d원 | 보증금차감 %d원 | 추가결제 %d원 | 매출반영 %d원 | %s",
             p->receipt_no, p->reservation_code, p->car_number, p->phone,
             'A' + p->tower_id - 1, car_type_name(p->car_type), entry, exit_time,
             p->parking_fee, p->deposit_used, p->final_fee, receipt_sales_amount(p), p->method);
    append_text_line(RECEIPTS_FILE, line);
}

void show_payment_records(void) {
    int count = count_records(PAYMENTS_FILE, sizeof(Payment));
    int total_sales = 0;
    int tower_sales[4] = {0, 0, 0, 0};
    print_line();
    printf("영수증 기록 및 매출\n");
    print_line();
    if (count == 0) {
        printf("영수증 기록이 없습니다.\n");
        print_line();
        return;
    }
    for (int i = 0; i < count; i++) {
        Payment p;
        char entry[64], exit_time[64];
        if (read_record_at(PAYMENTS_FILE, i, &p, sizeof(p)) == 0) {
            int sales = receipt_sales_amount(&p);
            format_time(p.entry_time, entry, sizeof(entry));
            format_time(p.exit_time, exit_time, sizeof(exit_time));
            total_sales += sales;
            if (p.tower_id >= 1 && p.tower_id <= 3) tower_sales[p.tower_id] += sales;
            printf("영수증 %s | 예약 %s | 차량 %s | %c Tower | %s | %d분 | 주차요금 %d원 | 추가결제 %d원 | 매출 %d원 | %s\n",
                   p.receipt_no, p.reservation_code, p.car_number, 'A' + p.tower_id - 1,
                   car_type_name(p.car_type), p.total_minutes, p.parking_fee, p.final_fee, sales, p.method);
            printf("  입차: %s / 출차: %s\n", entry, exit_time);
        }
    }
    print_line();
    printf("총 영수증 건수: %d건\n", count);
    printf("총 매출: %d원\n", total_sales);
    printf("A Tower 매출: %d원\n", tower_sales[1]);
    printf("B Tower 매출: %d원\n", tower_sales[2]);
    printf("C Tower 매출: %d원\n", tower_sales[3]);
    print_line();
}

void show_sales_statistics(void) {
    int count = count_records(PAYMENTS_FILE, sizeof(Payment));
    int total_sales = 0;
    int tower_sales[4] = {0, 0, 0, 0};
    int car_sales[CAR_TYPE_COUNT + 1] = {0};
    int method_card = 0, method_cash = 0, method_mobile = 0;
    int method_card_count = 0, method_cash_count = 0, method_mobile_count = 0;

    print_line();
    printf("매출 통계\n");
    print_line();
    if (count == 0) {
        printf("영수증 기록이 없습니다.\n");
        print_line();
        return;
    }

    for (int i = 0; i < count; i++) {
        Payment p;
        if (read_record_at(PAYMENTS_FILE, i, &p, sizeof(p)) == 0) {
            int sales = receipt_sales_amount(&p);
            total_sales += sales;
            if (p.tower_id >= 1 && p.tower_id <= 3) tower_sales[p.tower_id] += sales;
            if (p.car_type >= 1 && p.car_type <= CAR_TYPE_COUNT) car_sales[p.car_type] += sales;
            if (strcmp(p.method, "카드") == 0) { method_card += sales; method_card_count++; }
            else if (strcmp(p.method, "현금") == 0) { method_cash += sales; method_cash_count++; }
            else { method_mobile += sales; method_mobile_count++; }
        }
    }

    printf("총 영수증 건수: %d건\n", count);
    printf("총 매출: %d원\n", total_sales);
    printf("평균 주차요금: %d원\n\n", count > 0 ? total_sales / count : 0);

    printf("[주차타워별 매출]\n");
    printf("A Tower: %d원\n", tower_sales[1]);
    printf("B Tower: %d원\n", tower_sales[2]);
    printf("C Tower: %d원\n\n", tower_sales[3]);

    printf("[차종별 매출]\n");
    for (int type = 1; type <= CAR_TYPE_COUNT; type++) {
        printf("%s: %d원\n", car_type_name(type), car_sales[type]);
    }
    printf("\n[결제수단별 매출]\n");
    printf("카드: %d건 / %d원\n", method_card_count, method_card);
    printf("현금: %d건 / %d원\n", method_cash_count, method_cash);
    printf("모바일 결제: %d건 / %d원\n", method_mobile_count, method_mobile);

    printf("\n※ 매출은 출차 시 남는 영수증 기록의 주차요금 기준으로 집계합니다.\n");
    printf("※ 보증금 차감 때문에 출차 시 추가 결제금액이 0원이어도 매출은 주차요금으로 반영됩니다.\n");

    if (export_sales_report_csv() == 0) {
        printf("\n웹사이트 연동용 CSV가 %s 에 저장되었습니다.\n", SALES_REPORT_FILE);
    }
    print_line();
}

int export_sales_report_csv(void) {
    FILE *fp = fopen(SALES_REPORT_FILE, "w");
    if (!fp) return -1;
    fprintf(fp, "receipt_no,reservation_code,car_number,phone,tower,car_type,minutes,parking_fee,deposit_used,extra_payment,revenue,method\n");
    int count = count_records(PAYMENTS_FILE, sizeof(Payment));
    for (int i = 0; i < count; i++) {
        Payment p;
        if (read_record_at(PAYMENTS_FILE, i, &p, sizeof(p)) == 0) {
            fprintf(fp, "%s,%s,%s,%s,%c Tower,%s,%d,%d,%d,%d,%d,%s\n",
                    p.receipt_no, p.reservation_code, p.car_number, p.phone,
                    'A' + p.tower_id - 1, car_type_name(p.car_type), p.total_minutes,
                    p.parking_fee, p.deposit_used, p.final_fee, receipt_sales_amount(&p), p.method);
        }
    }
    fclose(fp);
    return 0;
}
