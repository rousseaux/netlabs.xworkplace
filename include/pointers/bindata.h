
#ifndef BINDATA_H
#define BINDATA_H

// data structures

typedef struct _BINDATA
   {
         ULONG          cbSize;
         BYTE           bData[1];
   } BINDATA, *PBINDATA;

#endif // BINDATA_H

