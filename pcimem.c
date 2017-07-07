/*
 * pcimem.c: Simple program to read/write from/to a pci device from userspace.
 *
 *  Copyright (C) 2010, Bill Farrow (bfarrow@beyondelectronics.us)
 *
 *  Based on the devmem2.c code
 *  Copyright (C) 2000, Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <unistd.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static struct dump_range {
	const char *name, *fname;
	uint32_t ranges[10][2];

} dump_ranges[]= {
	/* { */
	/* 	.name = "bar0", */
	/* 	.fname = "resource0", */
	/* 	.ranges= { */
	/* 		{ 0x00000, 0x0FFFF }, */
	/* 		{ 0x10000, 0x1FFFF }, */
	/* 		{ 0x20000, 0x2FFFF }, */
	/* 		{ 0x30000, 0x3FFFF }, */
	/* 	} */
	/* }, */
	{
		.name = "bar2",
		.fname = "resource2",
		.ranges= {
			{ 0x00000, 0x07EFF },

			/* { 0x00000, 0x07EFF }, */
			/* { 0x10000, 0x100FF }, */
			/* { 0x10100, 0x101FF }, */
			/* { 0x10200, 0x102FF }, */
			/* { 0x10300, 0x103FF }, */
		}
	},
};

int main(int argc, char **argv) {
	int fd;
	void *map_base;
	char path_buffer[PATH_MAX];
	uint64_t page_size, page_mask;


	page_size = sysconf(_SC_PAGE_SIZE);
	page_mask = ~(page_size - 1);
	
	if(argc < 2) {
		// pcimem /sys/bus/pci/devices/0001\:00\:07.0/resource0 0x100 w 0x00
		// argv[0]  [1]                                         [2]   [3] [4]
		fprintf(stderr, "\nUsage:\t%s { sys dir }  ]\n"
			"\tsys dir: sysfs dir for the pci resource to act on\n",
			argv[0]);
		exit(1);
	}

	for(int i=0; i < ARRAY_SIZE(dump_ranges); ++i) {
		struct dump_range * drange = &dump_ranges[i];
		uint64_t range_max = 0;
		int rindex;
		
		snprintf(path_buffer, sizeof(path_buffer), "%s/%s", argv[1], drange->fname);
		path_buffer[sizeof(path_buffer)] = '\0';

		fd = open(path_buffer, O_RDONLY | O_SYNC);
		if (fd == -1) {
			printf("[%s]: %s", drange->name, strerror(errno));
			fflush(stdout);
			continue;
		}

		/* find the number of pages we need to map for the ranges */
		for (rindex = 0 ; rindex < ARRAY_SIZE(drange->ranges); ++rindex) {
			typeof(drange->ranges[0][0]) *range = drange->ranges[rindex];
			if (range[0] > range[1]) {
				int low = range[1];
				range[1] = range[0];
				range[0] = low;
			}

			if (range[1] > range_max)
				range_max = range[1];

		}

		map_base = mmap(0, (range_max | page_mask) +1 , PROT_READ,
				MAP_SHARED, fd, 0);

		if(map_base == MAP_FAILED) {
			printf("[%s]: %s", drange->name, strerror(errno));
			fflush(stdout);
			close(fd);
			continue;
		}

		for (rindex = 0 ; rindex < ARRAY_SIZE(drange->ranges); ++rindex){
			typeof(drange->ranges[0][0]) *range = drange->ranges[rindex];
			uint32_t index;
			
			if (range[0] == range[1] && range[0] == 0)
				continue;
			
			for( index = range[0]; index <= range[1];
			     index += sizeof(uint64_t)) {
				uint64_t *ptr = (uint64_t*) (map_base + index);
				printf("[%s + %08X] = %016lX\n", drange->name,
				       index, *ptr);
				fflush(stdout);
			}
		}
		
		munmap(map_base, (range_max | page_mask) +1);
		close(fd);
	}

    return 0;
}
