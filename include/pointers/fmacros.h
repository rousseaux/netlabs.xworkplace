
#ifndef FMACROS_H
#define FMACROS_H

// --------------------------------------
// read file contents to buffer
// --------------------------------------

#define READMEMORY(h, t, l)             \
{                                       \
rc = DosRead(  h,                       \
               &t,                      \
               l,                       \
               &ulBytesRead);           \
if (rc != NO_ERROR)                     \
   break;                               \
}rc                                     \

#define PREADMEMORY(h,p,l)              \
{                                       \
rc = DosRead(  h,                       \
               p,                       \
               l,                       \
               &ulBytesRead);           \
if (rc != NO_ERROR)                     \
   break;                               \
}rc                                     \

// ###
#define READULONG(h,t)                  \
READMEMORY(h, t, sizeof( ULONG))

// --------------------------------------
// query/modify file ptr
// --------------------------------------

#define MOVEFILEPTR(h,o)                \
{                                       \
rc = DosSetFilePtr( h,                  \
                    o,                  \
                    FILE_CURRENT,       \
                    &ulFilePtr);        \
if (rc != NO_ERROR)                     \
   break;                               \
}rc                                     \

#define QUERYFILEPTR(h)                 \
MOVEFILEPTR(h,0)


#define SETFILEPTR(h,o)                 \
{                                       \
rc = DosSetFilePtr( h,                  \
                    o,                  \
                    FILE_BEGIN,         \
                    &ulFilePtr);        \
if (rc != NO_ERROR)                     \
   break;                               \
}rc                                     \


// --------------------------------------
// skip file contents
// --------------------------------------

#define SKIPBYTES(h,n)                  \
if (n > 0)                              \
   MOVEFILEPTR(h,n)

#define SKIPULONG(h,n)                  \
SKIPBYTES(h, n * sizeof( ULONG))

// --------------------------------------
// write file contents
// --------------------------------------

// write memory
#define WRITEMEMORY(h,p,len)            \
{                                       \
rc = DosWrite( h,                       \
               p,                       \
               len,                     \
               &ulBytesWritten);        \
if (rc != NO_ERROR)                     \
   break;                               \
}rc                                     \


// write SZ
#define WRITESZ(h,s)                    \
WRITEMEMORY(h, s, strlen( s) + 1)

// write struct
#define WRITESTRUCT(h,s)                \
WRITEMEMORY(h, &s, sizeof( s))

#endif // FMACROS_H

