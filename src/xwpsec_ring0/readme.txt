
src\xwpsec_ring0 has the code for the ring-0 part of the
XWorkplace Installable Security Subsystem (ISS).

This is an SES base device driver. Aside from some 16-bit
thunking code written in assembler, it is a 32-bit device
driver, and can be compiled using VAC 3.08.

IBM ALP is needed for the assembler files.
From my testing, only ALP 4.00.001 was able to swallow
these files. I tested with 4.00.006, and it refused to accept
some of the constructs in there. I am no assembler freak, so
maybe someone else can help.

All this code compiles into the XWPSEC32.SYS driver file,
which forms the ring-0 part of XWorkplace Security.

Installation:
Copy XWPSEC32.SYS to \OS2\BOOT.

Add the following lines to CONFIG.SYS (in this order):
    BASEDEV=SESDD32.SYS
    BASEDEV=XWPSEC32.SYS

Make sure you have SES installed (you should have an \OS2\security
directory). The rest of the SES setup is NOT needed however.


