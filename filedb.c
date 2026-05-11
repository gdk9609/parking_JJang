#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "parking.h"

static int ensure_file_exists(const char *path) {
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    close(fd);
    return 0;
}

static int truncate_if_broken_record_file(const char *path, size_t size) {
    struct stat st;
    if (stat(path, &st) < 0) return 0;
    if (size == 0) return 0;
    if (st.st_size > 0 && st.st_size % (off_t)size != 0) {
        int fd = open(path, O_WRONLY | O_TRUNC);
        if (fd < 0) return -1;
        close(fd);
        char logbuf[256];
        snprintf(logbuf, sizeof(logbuf), "데이터 파일 구조 변경 감지로 초기화: %s", path);
        append_text_line(LOG_FILE, logbuf);
    }
    return 0;
}

int ensure_data_files(void) {
    struct stat st;
    if (stat(DATA_DIR, &st) < 0) {
        if (mkdir(DATA_DIR, 0755) < 0) {
            perror("mkdir data");
            return -1;
        }
    }
    if (ensure_file_exists(LOG_FILE) < 0) return -1;
    if (ensure_file_exists(TOWERS_FILE) < 0) return -1;
    if (ensure_file_exists(RESERVATIONS_FILE) < 0) return -1;
    if (ensure_file_exists(PARKING_FILE) < 0) return -1;
    if (ensure_file_exists(PAYMENTS_FILE) < 0) return -1;
    if (ensure_file_exists(MESSAGES_FILE) < 0) return -1;
    if (ensure_file_exists(RECEIPTS_FILE) < 0) return -1;
    if (ensure_file_exists(SALES_REPORT_FILE) < 0) return -1;

    if (truncate_if_broken_record_file(TOWERS_FILE, sizeof(ParkingTower)) < 0) return -1;
    if (truncate_if_broken_record_file(RESERVATIONS_FILE, sizeof(Reservation)) < 0) return -1;
    if (truncate_if_broken_record_file(PARKING_FILE, sizeof(ParkingRecord)) < 0) return -1;
    if (truncate_if_broken_record_file(PAYMENTS_FILE, sizeof(Payment)) < 0) return -1;

    if (init_default_towers() < 0) return -1;
    rebuild_tower_current_from_reservations();
    return 0;
}

int append_record(const char *path, const void *record, size_t size) {
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open append_record");
        return -1;
    }
    ssize_t written = write(fd, record, size);
    if (written != (ssize_t)size) {
        perror("write append_record");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int read_record_at(const char *path, int index, void *record, size_t size) {
    if (index < 0) return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    off_t offset = (off_t)index * (off_t)size;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }
    ssize_t r = read(fd, record, size);
    close(fd);
    if (r != (ssize_t)size) return -1;
    return 0;
}

int update_record_at(const char *path, int index, const void *record, size_t size) {
    if (index < 0) return -1;
    int fd = open(path, O_RDWR);
    if (fd < 0) return -1;
    off_t offset = (off_t)index * (off_t)size;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }
    ssize_t w = write(fd, record, size);
    close(fd);
    if (w != (ssize_t)size) return -1;
    return 0;
}

int delete_record_at(const char *path, int index, size_t size) {
    if (index < 0 || size == 0) return -1;
    int count = count_records(path, size);
    if (index >= count) return -1;

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    int src = open(path, O_RDONLY);
    if (src < 0) return -1;
    int dst = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        close(src);
        return -1;
    }

    char buffer[512];
    if (size > sizeof(buffer)) {
        close(src);
        close(dst);
        unlink(tmp_path);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        ssize_t r = read(src, buffer, size);
        if (r != (ssize_t)size) {
            close(src);
            close(dst);
            unlink(tmp_path);
            return -1;
        }
        if (i == index) continue;
        if (write(dst, buffer, size) != (ssize_t)size) {
            close(src);
            close(dst);
            unlink(tmp_path);
            return -1;
        }
    }

    close(src);
    close(dst);
    if (rename(tmp_path, path) < 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

int count_records(const char *path, size_t size) {
    struct stat st;
    if (stat(path, &st) < 0) return 0;
    if (size == 0) return 0;
    return (int)(st.st_size / (off_t)size);
}

int append_text_line(const char *path, const char *line) {
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open append_text_line");
        return -1;
    }
    size_t len = strlen(line);
    if (write(fd, line, len) != (ssize_t)len) {
        perror("write text");
        close(fd);
        return -1;
    }
    if (len == 0 || line[len - 1] != '\n') {
        if (write(fd, "\n", 1) != 1) {
            perror("write newline");
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}


int delete_text_lines_matching(const char *path, const char *keyword1, const char *keyword2) {
    if (!path) return -1;

    FILE *src = fopen(path, "r");
    if (!src) return -1;

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE *dst = fopen(tmp_path, "w");
    if (!dst) {
        fclose(src);
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), src)) {
        int matched = 0;
        if (keyword1 && keyword1[0] && strstr(line, keyword1)) matched = 1;
        if (keyword2 && keyword2[0] && strstr(line, keyword2)) matched = 1;
        if (!matched) fputs(line, dst);
    }

    fclose(src);
    fclose(dst);

    if (rename(tmp_path, path) < 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

void print_text_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("파일을 열 수 없습니다: %s\n", path);
        return;
    }
    char buf[512];
    ssize_t r;
    int empty = 1;
    while ((r = read(fd, buf, sizeof(buf) - 1)) > 0) {
        empty = 0;
        buf[r] = '\0';
        printf("%s", buf);
    }
    if (empty) printf("기록이 없습니다.\n");
    close(fd);
}
