/*
 * Original work copyright 2013 Fabio Baltieri <fabio.baltieri@gmail.com>
 * Modified work copyright 2015 Dan Kouba
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

// SocketCAN flags
// Upper 3 bits of ID integer in CAN frame are used for these flags
#define CAN_EFF_FLAG 0x80000000U
#define CAN_RTR_FLAG 0x40000000U
#define CAN_ERR_FLAG 0x20000000U

static int sk;
static int frame_cnt = 0;
static int valid_frame_cnt = 0;

union dataframe {

	struct __packed {
        uint8_t b7;
        uint8_t b6;
        uint8_t b5;
        uint8_t b4;
        uint8_t b3;
        uint8_t b2;
        uint8_t b1;
        uint8_t b0;
    } generic;

};


static void process_one(struct can_frame *frm)
{
	valid_frame_cnt++;
	union dataframe *dat;

	dat = (union dataframe *)frm->data;

    bool eff_flag = frm->can_id & CAN_EFF_FLAG;
    bool rtr_flag = frm->can_id & CAN_RTR_FLAG;
    bool err_flag = frm->can_id & CAN_ERR_FLAG;
    
    // ID
    move(0, 0);
    clrtoeol();
    move(1, 0);
    clrtoeol();
    move(2, 0);
    clrtoeol();
    move(3, 0);
    clrtoeol();
    // Should update total framecount when any frame is receved, not just when the filter is matched.
    mvprintw(0, 0, "Arbitration ID: %#08X, Frame Number: %d/%d", frm->can_id & 0x1FFFFFFF, valid_frame_cnt, frame_cnt);
    mvprintw(1, 0, "EFF Frame: %s", eff_flag ? "True" : "False");
    mvprintw(2, 0, "RTR Frame: %s", rtr_flag ? "True" : "False");
    mvprintw(3, 0, "ERR Frame: %s", err_flag ? "True" : "False");

    // Hex data
    move(5, 0);
    clrtoeol();
    mvprintw(5, 0, " %02X  %02X  %02X  %02X  %02X  %02X  %02X  %02X", 
            dat->generic.b7,
            dat->generic.b6,
            dat->generic.b5,
            dat->generic.b4,
            dat->generic.b3,
            dat->generic.b2,
            dat->generic.b1,
            dat->generic.b0
            );

    // Decimal Data
    move(6, 0);
    clrtoeol();
	mvprintw(6, 0, "%03d %03d %03d %03d %03d %03d %03d %03d", 
            dat->generic.b7,
            dat->generic.b6,
            dat->generic.b5,
            dat->generic.b4,
            dat->generic.b3,
            dat->generic.b2,
            dat->generic.b1,
            dat->generic.b0
            );

    // Char Data
    move(7, 0);
    clrtoeol();
	mvprintw(7, 0, "  %c   %c   %c   %c   %c   %c   %c   %c", 
            (dat->generic.b7 > 32 && dat->generic.b7 < 127) ? dat->generic.b7 : '.',
            (dat->generic.b6 > 32 && dat->generic.b6 < 127) ? dat->generic.b6 : '.',
            (dat->generic.b5 > 32 && dat->generic.b5 < 127) ? dat->generic.b5 : '.',
            (dat->generic.b4 > 32 && dat->generic.b4 < 127) ? dat->generic.b4 : '.',
            (dat->generic.b3 > 32 && dat->generic.b3 < 127) ? dat->generic.b3 : '.',
            (dat->generic.b2 > 32 && dat->generic.b2 < 127) ? dat->generic.b2 : '.',
            (dat->generic.b1 > 32 && dat->generic.b1 < 127) ? dat->generic.b1 : '.',
            (dat->generic.b0 > 32 && dat->generic.b0 < 127) ? dat->generic.b0 : '.'
            );

	
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

	recv_own_msgs = 0; // 0 = disabled (default), 1 = enabled //
	setsockopt(sk, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
			&recv_own_msgs, sizeof(recv_own_msgs));

	return 0;
}

static void receive_one(uint32_t arb_id)
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

    frame_cnt++;

    if ( (frm.can_id & 0x1FFFFFFF) == arb_id) {
	    process_one(&frm);
    }
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Syntax: %s <Interface Name> <Arbitration ID>\n", argv[0]);
		exit(1);
	}

    uint32_t id = (uint32_t)strtol(argv[2], NULL, 16) & 0x7FFFFFFF;
	
    initscr();

	net_init(argv[1]);

	for (;;)
		receive_one(id);


	endwin();

	return 0;
}
