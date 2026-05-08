CC=gcc
CFLAGS=-Wall -Wextra -std=c11
LDFLAGS=-pthread
TARGET=parking_system
SRCS=main.c user.c admin.c tower.c reservation.c parking.c fee.c payment.c filedb.c log.c utils.c time_utils.c
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c parking.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

reset-data:
	rm -rf data

run: $(TARGET)
	./$(TARGET)
