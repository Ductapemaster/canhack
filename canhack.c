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
    PASS_DOORS = 0x12F84010,
    TRUNK = 0x12F84310,
    TURN_SIG = 0x0AF87010,
    WHEELS = 0x0EF86350,
    BRAKE_PDL = 0x12F81010,
    UNK2 = 0x12F83110,
    RPM = 0x12F85050
};

#define UNKNOWN_COUNT 1024
static int unknown[UNKNOWN_COUNT];

union dataframe {

	struct __packed {
        uint16_t gear;
        uint8_t unk1;
        uint8_t unk2;
        uint8_t unk3;
        uint8_t unk4;
    } shifter_frm;

    struct __packed {
        uint8_t flag;
    } drv_doors_frm;

    struct __packed {
        uint8_t flag;
    } pass_doors_frm;

    struct __packed {
        uint8_t flag;
    } trunk_frm;
    
    struct __packed {
        uint8_t _pad1;
        uint8_t flag;
    } turn_sig_frm;

    struct __packed {
        uint8_t wheel1;
        uint8_t wheel2;
        uint8_t wheel3;
        uint8_t wheel4;
        uint16_t _pad;
        uint16_t counter;
    } wheels_frm;

    struct __packed {
        uint8_t pedal_flag;
        uint8_t _pad1;
        uint8_t unk2;
        uint8_t _pad2;
        uint8_t unk3;
    } brake_pdl_frm;

    struct __packed {
        uint8_t unk1;
    } unk_frm2;

    struct __packed {
        uint8_t rpm0;
        uint8_t rpm1;
        uint8_t rpm2;
        uint8_t _pad1;
        uint8_t _pad2;
        uint8_t _pad3;
    } rpm_frm;
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
	union dataframe *dat;

	dat = (union dataframe *)frm->data;
    
    // CAN Arbitration ID is 29 bits, so mask off the upper 3 bits
    // What do the upper 3 bits mean in libsocketCAN?? Flags?	
    switch (frm->can_id & 0x7FFFFFFF) {

        case SHIFTER:
            
            move(1, 1);
            clrtoeol();
           
            // Endianness of the 16 bit gear value is wrong - need to check
           
            uint16_t gear = (dat->shifter_frm.gear & 0xFF00) >> 8 | (dat->shifter_frm.gear & 0x00FF) << 8; 
             mvprintw(1, 1, "Gear Bytes=%04X unk1=%02X unk2=%02X unk3=%02X unk4=%02X",
                    gear,
                    dat->shifter_frm.unk1,
                    dat->shifter_frm.unk2,
                    dat->shifter_frm.unk3,
                    dat->shifter_frm.unk4
                    );
            break;
        
        case DRIVER_DOORS:
            move(2, 1);
            clrtoeol();
            mvprintw(2, 1, "Driver Door: %s", (dat->drv_doors_frm.flag & 0x80) ? "Open" : "Clsd");
            move(3, 1);
            clrtoeol();
            mvprintw(3, 1, "Driver Rear Door: %s", (dat->drv_doors_frm.flag & 0x20) ? "Open" : "Clsd");
            break;

        case PASS_DOORS:
            move(4, 1);
            clrtoeol();
            mvprintw(4, 1, "Passenger Door: %s", (dat->pass_doors_frm.flag & 0x40) ? "Open" : "Clsd");
            move(5, 1);
            clrtoeol();
            mvprintw(5, 1, "Passenger Rear Door: %s", (dat->pass_doors_frm.flag & 0x10) ? "Open" : "Clsd");
            break;
        
        case TRUNK:
            move(6, 1);
            clrtoeol();
            mvprintw(6, 1, "Trunk: %s",
                    (dat->trunk_frm.flag & 0x80) ? "Open" : "Clsd"
                    );
            break;

        case TURN_SIG:
            move(7, 1);
            clrtoeol();
            mvprintw(7, 1, "Turn Signal: %s-%s",                  
                    (dat->turn_sig_frm.flag & 0x80) ? "<" : "-",
                    (dat->turn_sig_frm.flag & 0x40) ? ">" : "-"           
                    );
            break;
        
        case WHEELS:
            move(8, 1);
            clrtoeol();
            uint16_t ctr = (dat->wheels_frm.counter & 0xFF00) >> 8 | (dat->wheels_frm.counter & 0x00FF) << 8; 

            mvprintw(8, 1, "Wheel 1: %02X Wheel 2: %02X Wheel 3: %02X Wheel 4: %02X Counter: %04X",
                    dat->wheels_frm.wheel1,
                    dat->wheels_frm.wheel2,
                    dat->wheels_frm.wheel3,
                    dat->wheels_frm.wheel4,
                    ctr
                    );
            break;

        case RPM:
            move(9, 1);
            clrtoeol();
            uint32_t rpm = ( 0x00FFFFFF & ( (uint32_t)dat->rpm_frm.rpm0 << 16 | (uint32_t)dat->rpm_frm.rpm1 << 8 | (uint32_t)dat->rpm_frm.rpm2 ) );
            mvprintw(9, 1, "RPM: %02X%02X%02X Real RPM? : %d Pad: %02X%02X%02X",
                    dat->rpm_frm.rpm0,                  
                    dat->rpm_frm.rpm1,                  
                    dat->rpm_frm.rpm2,   
                    rpm,
                    dat->rpm_frm._pad1,                  
                    dat->rpm_frm._pad2,                  
                    dat->rpm_frm._pad3
                    );
            break;                  


       case BRAKE_PDL:
            move(10, 1);
            clrtoeol();
            mvprintw(10, 1, "Brake Pedal Engaged: %s %02X Byte[2]: %02X Byte[0]: %02X",                  
                    (dat->brake_pdl_frm.pedal_flag & 0x10) ? "True " : "False",
                    dat->brake_pdl_frm.pedal_flag,
                    dat->brake_pdl_frm.unk2,
                    dat->brake_pdl_frm.unk3
                    );
            break;

       case UNK2:
            move(16, 1);
            clrtoeol();
            mvprintw(16, 1, "Byte[0]: %02X",                  
                    dat->unk_frm2.unk1
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
