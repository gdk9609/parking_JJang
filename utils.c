#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "parking.h"

void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') s[len - 1] = '\0';
}

void clear_stdin_extra(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

void safe_read_line(char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
}

int read_int(const char *prompt, int min, int max) {
    char buf[64];
    char *end;
    long value;
    while (1) {
        if (prompt) printf("%s", prompt);
        safe_read_line(buf, sizeof(buf));
        if (buf[0] == '\0') {
            printf("입력이 비어 있습니다. 다시 입력하세요.\n");
            continue;
        }
        value = strtol(buf, &end, 10);
        if (*end != '\0') {
            printf("숫자만 입력하세요.\n");
            continue;
        }
        if (value < min || value > max) {
            printf("%d부터 %d 사이의 값을 입력하세요.\n", min, max);
            continue;
        }
        return (int)value;
    }
}

void pause_enter(void) {
    char buf[8];
    printf("\nEnter를 누르면 계속합니다...");
    safe_read_line(buf, sizeof(buf));
}

const char *car_type_name(int type) {
    switch (type) {
        case CAR_COMPACT: return "경차";
        case CAR_MIDSIZE: return "중형차";
        case CAR_SUV: return "SUV";
        case CAR_LARGE: return "대형차";
        case CAR_EV: return "전기차";
        default: return "알 수 없음";
    }
}

const char *reservation_status_name(int status) {
    switch (status) {
        case RES_CANCELLED: return "취소됨";
        case RES_RESERVED: return "예약 완료";
        case RES_ENTERED: return "입차 완료";
        case RES_PAID: return "정산 완료";
        case RES_EXPIRED: return "시간 초과 취소";
        default: return "알 수 없음";
    }
}

int choose_car_type(void) {
    printf("\n차종을 선택하세요.\n");
    printf("1. 경차\n");
    printf("2. 중형차\n");
    printf("3. SUV\n");
    printf("4. 대형차\n");
    printf("5. 전기차\n");
    return read_int("차종 선택: ", 1, 5);
}

void print_line(void) {
    printf("========================================\n");
}

int ask_yes_no(const char *prompt) {
    char buf[16];
    while (1) {
        printf("%s", prompt);
        safe_read_line(buf, sizeof(buf));
        if (strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0 || strcmp(buf, "예") == 0 || strcmp(buf, "네") == 0) return 1;
        if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0 || strcmp(buf, "아니오") == 0 || strcmp(buf, "아니요") == 0) return 0;
        printf("y/n 또는 예/아니오로 입력하세요.\n");
    }
}

int format_phone_number(const char *input, char *out, size_t out_size) {
    char digits[16];
    int n = 0;
    if (!input || !out || out_size < 14) return 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (isdigit((unsigned char)input[i])) {
            if (n >= 15) return 0;
            digits[n++] = input[i];
        } else if (input[i] == '-' || input[i] == ' ' || input[i] == '\t') {
            continue;
        } else {
            return 0;
        }
    }
    digits[n] = '\0';
    if (n != 11) return 0;
    if (digits[0] != '0' || digits[1] != '1' || digits[2] != '0') return 0;
    snprintf(out, out_size, "%.3s-%.4s-%.4s", digits, digits + 3, digits + 7);
    return 1;
}

int read_phone_number(char *out, size_t out_size) {
    char buf[MAX_PHONE];
    while (1) {
        printf("전화번호 입력(01012345678 또는 010-1234-5678): ");
        safe_read_line(buf, sizeof(buf));
        if (format_phone_number(buf, out, out_size)) {
            printf("전화번호가 %s 형식으로 저장됩니다.\n", out);
            return 1;
        }
        printf("전화번호 형식이 올바르지 않습니다. 010-****-**** 형식이어야 합니다.\n");
    }
}

static int decode_utf8_one(const char *s, unsigned int *codepoint, int *consumed) {
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) return 0;
    if ((c0 & 0xE0) == 0xC0) {
        unsigned char c1 = (unsigned char)s[1];
        if ((c1 & 0xC0) != 0x80) return 0;
        *codepoint = ((unsigned int)(c0 & 0x1F) << 6) | (unsigned int)(c1 & 0x3F);
        *consumed = 2;
        return 1;
    }
    if ((c0 & 0xF0) == 0xE0) {
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return 0;
        *codepoint = ((unsigned int)(c0 & 0x0F) << 12) | ((unsigned int)(c1 & 0x3F) << 6) | (unsigned int)(c2 & 0x3F);
        *consumed = 3;
        return 1;
    }
    if ((c0 & 0xF8) == 0xF0) {
        unsigned char c1 = (unsigned char)s[1];
        unsigned char c2 = (unsigned char)s[2];
        unsigned char c3 = (unsigned char)s[3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return 0;
        *codepoint = ((unsigned int)(c0 & 0x07) << 18) | ((unsigned int)(c1 & 0x3F) << 12) |
                     ((unsigned int)(c2 & 0x3F) << 6) | (unsigned int)(c3 & 0x3F);
        *consumed = 4;
        return 1;
    }
    return 0;
}

static int is_hangul_syllable_codepoint(unsigned int cp) {
    return cp >= 0xAC00 && cp <= 0xD7A3;
}

int validate_car_number(const char *input) {
    if (!input) return 0;
    size_t len = strlen(input);
    if (len < 9) return 0;

    int pos = 0;
    int digit_prefix = 0;
    while (isdigit((unsigned char)input[pos]) && digit_prefix < 3) {
        pos++;
        digit_prefix++;
    }
    if (digit_prefix != 2 && digit_prefix != 3) return 0;

    unsigned int cp = 0;
    int consumed = 0;
    if (!decode_utf8_one(input + pos, &cp, &consumed)) return 0;
    if (!is_hangul_syllable_codepoint(cp)) return 0;
    pos += consumed;

    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)input[pos + i])) return 0;
    }
    pos += 4;
    return input[pos] == '\0';
}

int read_car_number(char *out, size_t out_size) {
    char buf[MAX_NAME];
    while (1) {
        printf("차량번호 입력(예: 12가3456 또는 123가4567): ");
        safe_read_line(buf, sizeof(buf));
        if (validate_car_number(buf)) {
            snprintf(out, out_size, "%s", buf);
            return 1;
        }
        printf("차량번호 형식이 올바르지 않습니다. 숫자 2~3자리 + 한글 1자리 + 숫자 4자리 형식이어야 합니다.\n");
    }
}
