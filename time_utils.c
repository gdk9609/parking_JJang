#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "parking.h"

time_t now_time(void) {
    return time(NULL);
}

void format_time(time_t t, char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (t == 0) {
        snprintf(buf, size, "-");
        return;
    }
    struct tm *tm_ptr = localtime(&t);
    if (!tm_ptr) {
        snprintf(buf, size, "-");
        return;
    }
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_ptr);
}

int seconds_to_minutes_ceil(time_t start, time_t end) {
    if (end <= start) return 1;
    long diff = (long)difftime(end, start);
    int minutes = (int)((diff + 59) / 60);
    if (minutes < 1) minutes = 1;
    return minutes;
}

int parse_hhmm_to_minutes(const char *s, int *out) {
    if (!s || !out) return 0;
    if (strlen(s) != 5) return 0;
    if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1])) return 0;
    if (s[2] != ':') return 0;
    if (!isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4])) return 0;
    int hour = (s[0] - '0') * 10 + (s[1] - '0');
    int minute = (s[3] - '0') * 10 + (s[4] - '0');
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return 0;
    *out = hour * 60 + minute;
    return 1;
}

int parse_datetime(const char *s, time_t *out) {
    int y, mo, d, h, mi;
    char tail;
    struct tm tmv;
    time_t result;
    if (!s || !out) return 0;
    if (sscanf(s, "%d-%d-%d %d:%d%c", &y, &mo, &d, &h, &mi, &tail) != 5) return 0;
    if (y < 1970 || y > 2099) return 0;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) return 0;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = y - 1900;
    tmv.tm_mon = mo - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = h;
    tmv.tm_min = mi;
    tmv.tm_sec = 0;
    tmv.tm_isdst = -1;
    result = mktime(&tmv);
    if (result == (time_t)-1) return 0;
    struct tm *check = localtime(&result);
    if (!check) return 0;
    if (check->tm_year != y - 1900 || check->tm_mon != mo - 1 || check->tm_mday != d || check->tm_hour != h || check->tm_min != mi) return 0;
    *out = result;
    return 1;
}

int charged_minutes_for_fee(int actual_minutes) {
    if (actual_minutes <= 0) actual_minutes = 1;
    if (actual_minutes <= BASE_MINUTES) return BASE_MINUTES;
    int full_days = actual_minutes / 1440;
    int rem = actual_minutes % 1440;
    int charged = full_days * 1440;
    if (rem == 0) return charged;
    if (rem <= BASE_MINUTES) return charged + BASE_MINUTES;
    int extra = rem - BASE_MINUTES;
    int units = (extra + EXTRA_UNIT_MINUTES - 1) / EXTRA_UNIT_MINUTES;
    return charged + BASE_MINUTES + units * EXTRA_UNIT_MINUTES;
}

void format_duration_minutes(int minutes, char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (minutes < 0) minutes = 0;
    int days = minutes / 1440;
    int rem = minutes % 1440;
    int hours = rem / 60;
    int mins = rem % 60;
    if (days > 0) snprintf(buf, size, "%d일 %d시간 %d분", days, hours, mins);
    else snprintf(buf, size, "%d시간 %d분", hours, mins);
}
