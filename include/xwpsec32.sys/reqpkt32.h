
// 32 bits OS/2 device driver and IFS support. Provides 32 bits kernel
// services (DevHelp) and utility functions to 32 bits OS/2 ring 0 code
// (device drivers and installable file system drivers).
// Copyright (C) 1995, 1996, 1997  Matthieu WILLM (willm@ibm.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

/* Status word in RPH */

#define STERR        0x8000           /* Bit 15 - Error                */
#define STINTER      0x0400           /* Bit 10 - Interim character    */
#define STBUI        0x0200           /* Bit  9 - Busy                 */
#define STDON        0x0100           /* Bit  8 - Done                 */
#define STECODE      0x00FF           /* Error code                    */
#define WRECODE      0x0000

#define STATUS_DONE       0x0100
#define STATUS_ERR_UNKCMD 0x8003

#pragma pack(1)
struct reqhdr {
    unsigned char  length;
    unsigned char  unit;
    unsigned char  command;
    unsigned short status;
    unsigned char  flags;
    unsigned char  reserved[3];
    unsigned long  link;
};

struct reqpkt_ioctl {
    struct reqhdr  header;
    unsigned char  cat;
    unsigned char  func;
    PTR16          parm;
    PTR16          data;
    unsigned short sfn;
    unsigned short parmlen;
    unsigned short datalen;
};

struct reqpkt_open {
    struct reqhdr  header;
    unsigned short sfn;
};

struct reqpkt_close {
    struct reqhdr  header;
    unsigned short sfn;
};

struct reqpkt_read {
    struct reqhdr  header;
    unsigned char  media_descriptor;
    unsigned long  transfer_address;
    unsigned short count;
    unsigned long  first_sector;
    unsigned short sfn;
};

struct reqpkt_write {
    struct reqhdr  header;
    unsigned char  media_descriptor;
    unsigned long  transfer_address;
    unsigned short count;
    unsigned long  first_sector;
    unsigned short sfn;
};

struct reqpkt_init {
    struct reqhdr header;
    unsigned char unit;
    union {
        struct {
            PTR16         devhelp;
            PTR16         initarg;
            unsigned char drivenum;
        } input;
        struct {
            unsigned short codeend;
            unsigned short dataend;
            PTR16          bpbarray;
            unsigned short status;
        } output;
    } u;
};
#pragma pack()
