CFLAGS += -Wall -g
CFLAGS += `ncurses5-config --cflags`
LDFLAGS += `ncurses5-config --libs`

all: toyothack canwatch

toyothack:
canwatch:
