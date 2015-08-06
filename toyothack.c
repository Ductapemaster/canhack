/*
 * Copyright 2013 Fabio Baltieri <fabio.baltieri@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <endian.h>

#define __packed __attribute__((packed))

static int sk;

enum frame_ids {
    SHIFTER = 0x12F85150,
    DRIVER_DOORS = 0x12F83010,
    TRUNK = 0x12F84310,
    TURN_SIG = 0x0AF87010
};

#define UNKNOWN_COUNT 1024
static int unknown[UNKNOWN_COUNT];

union toyoframe {

	struct __packed {
		uint16_t a;
		uint16_t b;
		uint8_t flags;
		uint8_t seq;
	} wheel_speed;

	struct __packed {
		uint32_t _pad;
		uint8_t distance_a;
		uint16_t speed;
		uint8_t distance_b;
	} unkb4;

	struct __packed {
		uint8_t flags;
		uint8_t _pad0;
		uint8_t _pad1;
		uint8_t _pad2;
		uint8_t _pad3;
		uint8_t _pad4;
		uint8_t _pad5;
		uint8_t _pad6;
	} brake;

	struct __packed {
		uint8_t flags0;
		int16_t unk0;
		int16_t unk1;
		uint8_t unk2;
		int16_t throttle;
	} throttle;

	struct __packed {
		uint16_t rpm;
		uint8_t _pad0;
		uint8_t unk0;
		uint8_t _pad1;
		uint8_t _pad2;
		uint8_t unk1;
		int8_t unk2;
	} engine;

	struct __packed {
		int16_t fuel_usage;
	} fuel_usage;

    struct __packed {
        uint16_t gear;
        uint8_t unk1;
        uint8_t unk2;
        uint8_t unk3;
        uint8_t unk4;
    } shifter;

    struct __packed {
        uint8_t flag;
    } drv_doors;

    struct __packed {
        uint8_t flag;
    } trunk;
    
    struct __packed {
        uint8_t _pad1;
        uint8_t flag;
    } turn_sig;
};

static void unknown_frame(int id)
{
	int i;

	for (i = 0; i < UNKNOWN_COUNT; i++)
		if (unknown[i] == 0 || unknown[i] == id)
			break;
	if (i == UNKNOWN_COUNT)
		return;

	unknown[i] = id;

	move(LINES - 3, 1);
	clrtoeol();
	mvprintw(LINES - 3, 1, "unk:");
	for (i = 0; i < UNKNOWN_COUNT; i++) {
		if (unknown[i] == 0)
			break;
		printw(" %02x", unknown[i]);
	}
	printw(" (%d)", i);
}

static void process_one(struct can_frame *frm)
{
	//int i;
	union toyoframe *toy;

	toy = (union toyoframe *)frm->data;
    
    // CAN Arbitration ID is 29 bits, so mask off the upper 3 bits
    // What do the upper 3 bits mean in libsocketCAN?? Flags?	
    switch (frm->can_id & 0x7FFFFFFF) {

        case SHIFTER:
            
            move(1, 1);
            clrtoeol();
           
            // Endianness of the 16 bit gear value is wrong - need to check
            mvprintw(1, 1, "Gear Bytes=%04X unk1=%02X unk2=%02X unk3=%02X unk4=%02X",
                    toy->shifter.gear,
                    toy->shifter.unk1,
                    toy->shifter.unk2,
                    toy->shifter.unk3,
                    toy->shifter.unk4
                    );
            break;
        
        case DRIVER_DOORS:
            move(2, 1);
            clrtoeol();
            mvprintw(2, 1, "Driver Door: %s Driver Rear Door: %s Raw Bytes=%02X",
                    (toy->drv_doors.flag & 0x80) ? "Open" : "Clsd",
                    (toy->drv_doors.flag & 0x20) ? "Open" : "Clsd",
                    
                    toy->drv_doors.flag
                    );
            break;

        case TRUNK:
            move(3, 1);
            clrtoeol();
            mvprintw(3, 1, "Trunk: %s Raw Bytes=%02X",
                    (toy->trunk.flag & 0x80) ? "Open" : "Clsd",
                    toy->trunk.flag
                    );
            break;

        case TURN_SIG:
            move(4, 1);
            clrtoeol();
            mvprintw(4, 1, "Turn Signal: %s-%s",                  
                    (toy->turn_sig.flag & 0x80) ? "<" : "-",
                    (toy->turn_sig.flag & 0x40) ? ">" : "-"           
                    );
            break;
        
        default:
            //break;
            unknown_frame(frm->can_id & 0x7FFFFFFF);
        
    }
    
        
	refresh();
}

static int net_init(char *ifname)
{
	int recv_own_msgs;
	struct sockaddr_can addr;
	struct ifreq ifr;

	sk = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (sk < 0) {
		perror("socket");
		exit(1);
	}

	memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(sk, SIOCGIFINDEX, &ifr) < 0) {
		perror("SIOCGIFINDEX");
		exit(1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(sk, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return 1;
	}

	recv_own_msgs = 0; /* 0 = disabled (default), 1 = enabled */
	setsockopt(sk, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
			&recv_own_msgs, sizeof(recv_own_msgs));

	return 0;
}

static void receive_one(void)
{
	struct can_frame frm;
	struct sockaddr_can addr;
	int ret;
	socklen_t len;

	ret = recvfrom(sk, &frm, sizeof(struct can_frame), 0,
			(struct sockaddr *)&addr, &len);
	if (ret < 0) {
		perror("recvfrom");
		exit(1);
	}

	process_one(&frm);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("syntax: %s IFNAME\n", argv[0]);
		exit(1);
	}

	memset(unknown, 0, sizeof(unknown));

	initscr();

	net_init(argv[1]);

	for (;;)
		receive_one();

	endwin();

	return 0;
}
