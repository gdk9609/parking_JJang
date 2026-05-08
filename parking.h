#ifndef PARKING_H
#define PARKING_H

#include <time.h>
#include <stddef.h>

#define DATA_DIR "data"
#define TOWERS_FILE "data/towers.dat"
#define RESERVATIONS_FILE "data/reservations.dat"
#define PARKING_FILE "data/parking.dat"
#define PAYMENTS_FILE "data/payments.dat"
#define LOG_FILE "data/log.txt"
#define MESSAGES_FILE "data/messages.txt"
#define RECEIPTS_FILE "data/receipts.txt"
#define SALES_REPORT_FILE "data/sales_report.csv"

#define MAX_NAME 64
#define MAX_PHONE 32
#define MAX_CODE 24
#define MAX_METHOD 32
#define MAX_RECEIPT 32
#define MAX_BUFFER 256

#define RESERVATION_TIMEOUT_SECONDS 1200
#define RESERVATION_DEPOSIT 2000

#define BASE_MINUTES 60
#define BASE_FEE 2000
#define EXTRA_UNIT_MINUTES 10
#define EXTRA_UNIT_FEE 300
#define DAILY_MAX_FEE 30000

#define ADMIN_PASSWORD "admin1234"

#define DEFAULT_OPERATING_HOURS "24시간 운영"

typedef enum {
    CAR_NONE = 0,
    CAR_COMPACT = 1,
    CAR_MIDSIZE = 2,
    CAR_SUV = 3,
    CAR_LARGE = 4,
    CAR_EV = 5,
    CAR_TYPE_COUNT = 5
} CarType;

typedef enum {
    RES_CANCELLED = 0,
    RES_RESERVED = 1,
    RES_ENTERED = 2,
    RES_PAID = 3,
    RES_EXPIRED = 4
} ReservationStatus;

typedef struct {
    int id;
    char name[MAX_NAME];
    int total_capacity;
    int capacity[CAR_TYPE_COUNT + 1];
    int current[CAR_TYPE_COUNT + 1];
    char operating_hours[32];
    int active;
} ParkingTower;

typedef struct {
    int id;
    char code[MAX_CODE];
    char car_number[MAX_NAME];
    char phone[MAX_PHONE];
    int tower_id;
    int car_type;
    time_t reserved_at;
    time_t entry_time;
    time_t exit_time;
    int status;
    int deposit;
    int deposit_forfeited;
    int total_fee;
    int final_paid;
} Reservation;

typedef struct {
    int id;
    char reservation_code[MAX_CODE];
    char receipt_no[MAX_RECEIPT];
    char car_number[MAX_NAME];
    char phone[MAX_PHONE];
    int tower_id;
    int car_type;
    time_t entry_time;
    time_t exit_time;
    int total_minutes;
    int charged_minutes;
    int parking_fee;
    int deposit_used;
    int final_fee;
    char method[MAX_METHOD];
} Payment;

void safe_read_line(char *buf, size_t size);
int read_int(const char *prompt, int min, int max);
void trim_newline(char *s);
void pause_enter(void);
const char *car_type_name(int type);
const char *reservation_status_name(int status);
int choose_car_type(void);
void print_line(void);
void clear_stdin_extra(void);
int format_phone_number(const char *input, char *out, size_t out_size);
int validate_car_number(const char *input);
int read_phone_number(char *out, size_t out_size);
int read_car_number(char *out, size_t out_size);
int ask_yes_no(const char *prompt);

void format_time(time_t t, char *buf, size_t size);
time_t now_time(void);
int seconds_to_minutes_ceil(time_t start, time_t end);
int parse_hhmm_to_minutes(const char *s, int *out);
int parse_datetime(const char *s, time_t *out);
int charged_minutes_for_fee(int actual_minutes);
void format_duration_minutes(int minutes, char *buf, size_t size);

int calculate_parking_fee(int actual_minutes);
void run_fee_calculator(void);

int ensure_data_files(void);
int append_record(const char *path, const void *record, size_t size);
int read_record_at(const char *path, int index, void *record, size_t size);
int update_record_at(const char *path, int index, const void *record, size_t size);
int delete_record_at(const char *path, int index, size_t size);
int count_records(const char *path, size_t size);
int append_text_line(const char *path, const char *line);
void print_text_file(const char *path);

void write_log_msg(const char *action);
void show_logs(void);
void show_messages(void);

int init_default_towers(void);
int get_tower_by_id(int tower_id, ParkingTower *tower, int *index_out);
int update_tower(const ParkingTower *tower, int index);
void show_tower_info(void);
void show_all_tower_capacity(void);
void show_current_parking_status(void);
void show_car_type_availability(void);
int choose_tower(void);
int get_available_slots(int tower_id, int car_type);
int increase_tower_current(int tower_id, int car_type);
int decrease_tower_current(int tower_id, int car_type);
void show_tower_simple_list(void);
int recommend_tower_for_car_type(int car_type);
void show_recommended_tower(void);
void auto_refresh_availability(void);
int rebuild_tower_current_from_reservations(void);

void expire_old_reservations(void);
int start_expiration_thread(void);
void stop_expiration_thread(void);
int count_active_reservations(int tower_id, int car_type);
void create_reservation(void);
void lookup_reservation(void);
void cancel_reservation(void);
void change_reservation(void);
void find_reservation_code_by_identity(void);
void lookup_entered_parking_status(void);
int find_reservation_by_code(const char *code, Reservation *res, int *index_out);
int update_reservation(const Reservation *res, int index);
void show_all_reservations(void);
void show_entered_reservations(void);
int next_reservation_id(void);

void process_entry(void);
void process_exit_and_payment(void);

int append_payment(const Payment *payment);
void show_payment_records(void);
void show_sales_statistics(void);
int export_sales_report_csv(void);
char *make_receipt_number(char *buf, size_t size);
void print_receipt(const Payment *p);
void save_receipt_text(const Payment *p);

void admin_menu(void);
void admin_modify_reservation(void);
void admin_modify_payment(void);
void user_menu(void);

void handle_sigint(int sig);

#endif
