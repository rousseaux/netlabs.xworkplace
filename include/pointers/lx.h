
#ifndef LX_H
#define LX_H

#pragma pack(1)

// ----------------------------------------------------------------------------

// *** DOS EXE  header

// signature "MZ"
#define SIG_DOSEXEHDR  0x5A4D 

// DOS 2.0 header
typedef struct _DOSEXEHEADER
   {
         USHORT         usSig;             // 00
         USHORT         usPartPage;        // 02
         USHORT         usPageCount;       // 04
         USHORT         usReloCount;       // 06
         USHORT         usHeaderSize;      // 08
         USHORT         usMinAlloc;        // 0a
         USHORT         usMaxAlloc;        // 0c
         USHORT         usInitSS;          // 0e
         USHORT         usInitSP;          // 10
         USHORT         usCheckSum;        // 12
         USHORT         usInitIP;          // 14
         USHORT         usInitCS;          // 16
         USHORT         usRelTblOfs;       // 18
         USHORT         usOverlay;         // 1a
         USHORT         usUnused[ 16];     // 1c
         ULONG          ulOfsExtHeader;    // 3C
   } DOSEXEHEADER, *PDOSEXEHEADER;

// ----------------------------------------------------------------------------

// *** LX header

// signature "LX"
#define SIG_LXHDR      0x584C
#define EXE_PAGELEN    4096


// All offsets relative to start of LX header !!!
typedef struct _LXHEADER
   {
         USHORT         usSig;             //     magic number LXmagic
         BYTE           bBOrder;           //     The byte ordering for the .EXE
         BYTE           bWOrder;           //     The word ordering for the .EXE
         ULONG          ulLevel;           //     The EXE format level for now = 0
         USHORT         usCpu;             //     The CPU type
         USHORT         usOS;              //     The OS type
         ULONG          ulVer;             //     Module version
         ULONG          ulMflags;          //     Module flags
         ULONG          ulMpages;          //     Module # pages
         ULONG          ulStartObj;        //     Object # for instruction pointer
         ULONG          ulEIP;             //     Extended instruction pointer
         ULONG          ulStackObj;        //     Object # for stack pointer
         ULONG          ulESP;             //     Extended stack pointer
         ULONG          ulPageSize;        //     .EXE page size
         ULONG          ulPageShift;       //     Page alignment shift in .EXE
         ULONG          ulFixupSize;       //     Fixup section size
         ULONG          ulFixupSum;        //     Fixup section checksum
         ULONG          ulLdrSize;         //     Loader section size
         ULONG          ulLdrSum;          //     Loader section checksum
         ULONG          ulObjTabOfs;       //     Object table offset
         ULONG          ulObjCnt;          //     Number of objects in module
         ULONG          ulObjMapOfs;       //     Object page map offset
         ULONG          ulIterMapOfs;      //     Object iterated data map offset
         ULONG          ulRsrcTabOfs;      //     Offset of Resource Table
         ULONG          ulRsrcCnt;         //     Number of resource entries
         ULONG          ulResTabOfs;       //     Offset of resident name table
         ULONG          ulEntTabOfs;       //     Offset of Entry Table
         ULONG          ulDirTabOfs;       //     Offset of Module Directive Table
         ULONG          ulDirCnt;          //     Number of module directives
         ULONG          ulFPageTabOfs;     //     Offset of Fixup Page Table
         ULONG          ulFRecTabOfs;      //     Offset of Fixup Record Table
         ULONG          ulImpModOfs;       //     Offset of Import Module Name Table
         ULONG          ulImpModCnt;       //     Number of entries in Import Module Name Table
         ULONG          ulImpProcOfs;      //     Offset of Import Procedure Name Table
         ULONG          ulPageSumOfs;      //     Offset of Per-Page Checksum Table
         ULONG          ulDataPageOfs;     //     Offset of Enumerated Data Pages
         ULONG          ulPreload;         //     Number of preload pages
         ULONG          ulNResTabOfs;      //     Offset of Non-resident Names Table
         ULONG          ulCbNResTab;       //     Size of Non-resident Name Table
         ULONG          ulNResSum;         //     Non-resident Name Table Checksum
         ULONG          ulAutoData;        //     Object # for automatic data object
         ULONG          ulDebugInfoOfs;    //     Offset of the debugging information
                                                  // RELATIVE TO START OF EXE FILE}
         ULONG          ulDebugLen;        //     The length of the debugging info. in bytes
         ULONG          ulInstPreload;     //     Number of instance pages in preload section of .EXE file
         ULONG          ulInstDemand;      //     Number of instance pages in demand load section of .EXE file
         ULONG          ulHeapSize;        //     Size of heap - for 16-bit apps
         ULONG          ulStackSize;     
         BYTE           ulReserved[20];
   } LXHEADER, *PLXHEADER;

// ----------------------------------------------------------------------------

// *** object Table entry

typedef struct _OBJECT
   {
         ULONG          ulSize;            //     Objectvirtualsize
         ULONG          ulBase;            //     Objectbasevirtualaddress
         ULONG          ulFlags;           //     Attributeflags
         ULONG          ulPageMap;         //     Objectpagemapindex
         ULONG          ulMapSize;         //     Numberofentriesinobjectpagemap
         ULONG          ulReserved;        //     Reserved
   } OBJECT, *POBJECT;

#define OBJECT_READABLE         0x0001
#define OBJECT_WRITEABLE        0x0002
#define OBJECT_EXECUTEABLE      0x0004
#define OBJECT_RESOURCE         0x0008
#define OBJECT_DISCARDABLE      0x0010
#define OBJECT_SHARED           0x0020
#define OBJECT_PRELOADPAGES     0x0040
#define OBJECT_INVALIDPAGES     0x0080
#define OBJECT_ZEROFILLEDPAGES  0x0100

// next 3 for PDDs VDDs only
#define OBJECT_RESIDENT         0x0200
#define OBJECT_CONTIGUOUS       0x0300
#define OBJECT_LOCKABLE         0x0400

#define OBJECT_RESERVED         0x0800
#define OBJECT_16               0x1000
#define OBJECT_BIGORDEFAULT     0x2000
#define OBJECT_CODECONFORM      0x4000
#define OBJECT_16IOPL           0x8000



// object map table entry
// determine entry count from all pobject->ulMapSize fields

typedef struct _OBJECTMAP
   {
         ULONG          ulPageDataOffset;  //     file offset of page 
         USHORT         usPageSize;        //     # bytes of page data
         USHORT         usPageFlags;       //     Per-Page attributes
   } OBJECTMAP, *POBJECTMAP;

// ----------------------------------------------------------------------------

// *** resource table entry

typedef struct _RESOURCE
   {
         USHORT        usResType;          //     Resource type
         USHORT        usResName;          //     Resource name
         ULONG         ulResSize;          //     Resource size
         USHORT        usResObj;           //     Object number        
         ULONG         ulResOffs;          //     Offset within object
   } RESOURCE, *PRESOURCE;

// *** resource for RES file layout structures 

// macro to calculate resource name, 
// as 16 string ids are grouped per resource name
//
// name     message ids
// -----    -------------
// 1     =>     0 -    15
// 1000  => 65520 - 65335
#define RESSTRING_NAME(id) ((id / 16) + 1)

#define RESSTRINGTABLE_LEN(p)  (p->ulDataLen + sizeof( RESSTRINGTABLE)) 
#define RESSTRINGTABLE_NEXT(p) ((PBYTE) p + RESSTRINGTABLE_LEN(p))
#define RESSTRING_NEXT(p)      ((PBYTE) p + strlen( p->szString) + 2)

// some constants
#define RESHEADER_ORDINALFLAG 0xff
#define SIG_RESSTRINGTABLE    0x01b5

typedef struct _RESHEADER                  // header is not written to exe
   {
         BYTE          ulOrdinalFlag1;     //     needs to be 0xff
         USHORT        usType;             //     RT_* value
         BYTE          ulOrdinalFlag2;     //     needs to be 0xff
         USHORT        usName;             //     resource name (== ID or ID segment)
   } RESHEADER, *PRESHEADER;

typedef struct _RESSTRING
   {
         BYTE          bLen;
         CHAR          szString[ 1];
   } RESSTRING, *PRESSTRING;

typedef struct _RESSTRINGTABLE             // written to exe
   {
         USHORT        usSig;              //     needs to be 0x01b5
   } RESSTRINGTABLE, *PRESSTRINGTABLE;


// resident/nonresidentnames

typedef struct _RNAME 
   {
         BYTE          bSize;              //     name len
         CHAR          szName[ 1];         //     resident name
   } RNAME, *PRNAME;


// Entry Table entry

#define ENTRY_UNUSED        0x00           //     Empty bundle
#define ENTRY_CODE16        0x01           //     16-bit offset entry point
#define ENTRY_GATE16        0x02           //     286 call gate (16-bit IOPL)
#define ENTRY_CODE32        0x03           //     32-bit offset entry point
#define ENTRY_FORWARD       0x04           //     Forwarder entry point
#define ENTRY_MASK_TYPEINFO 0x80           //     Typing information present flag

typedef struct _ENTRYBUNDLE
   {
         BYTE          bCount;             //     count of entries in this bundle
         BYTE          bType;              //     Bundle type
         USHORT        usObjectNo;         //     object number
   } ENTRYBUNDLE, *PENTRYBUNDLE;


typedef struct _ENTRYCODE16
   {
         BYTE          bFlags;             //     Entry point flags
         USHORT        usOffset;           //     Offset in Object
   } ENTRYCODE16, *PENTRYCODE16;

typedef struct _ENTRYGATE16
   {
         BYTE          bFlags;             //     Entry point flags
         USHORT        usOffset;           //     Offset in Object
         USHORT        usCallgate;         //     Callgate Seelctor
   } ENTRYGATE16, *PENTRYGATE16;

typedef struct _ENTRYCODE32
   {
         BYTE          bFlags;             //     Entry point flags
         ULONG         ulOffset;           //     Offset in Object
   } ENTRYCODE32, *PENTRYCODE32;

typedef struct _ENTRYFORWARD
   {
         BYTE          bFlags;             //     Entry point flags
         USHORT        usModuleOrd;        //     Module ordinal number
         ULONG         usOfsOrOrdNum;      //     Proc name offset or imported ordninal number
   } ENTRYFORWARD, *PENTRYFORWARD;


// module format directive entry

typedef struct _DIRECTIVE
   {
         USHORT        usDirectiveNo;      //     directive no
         USHORT        usDirectiveLen;     //     directive data len
         ULONG         ulDirectiveOfs;     //     data offset
   } DIRECTIVE, *PDIRECTIVE;

// fixup record

typedef struct _FIXUPHEADER
   {
         BYTE          bSourceType;        //     fixup source type
         BYTE          bTargetFlags;       //     fixup target flags

   } FIXUPHEADER, *PFIXUPHEADER;


// dummy fixup len
#define FIXUP_DUMMY_LEN                4

// Source mask bits
#define FIXUP_SRC_SOURCEMASK        0x0f
#define FIXUP_SRC_BYTE              0x00
#define FIXUP_SRC_SEL16             0x02
#define FIXUP_SRC_WORD              0x03
#define FIXUP_SRC_OFS16             0x05
#define FIXUP_SRC_WORD2             0x06
#define FIXUP_SRC_OFS32             0x07

// flags
#define FIXUP_SRC_SROFS32           0x08
#define FIXUP_SRC_MASK_ALIAS        0x10
#define FIXUP_SRC_MASK_SRCLIST      0x20

// target mask bits
#define FIXUP_TRG_MASK_TARGET       0x03
#define FIXUP_TRG_INTERNAL          0x00 
#define FIXUP_TRG_IMPORT_ORD        0x01
#define FIXUP_TRG_IMPORT_NAME       0x02
#define FIXUP_TRG_INTERNAL_ENTRY    0x03

// flags
#define FIXUP_TRG_MASK_ADDITIVE     0x04
#define FIXUP_TRG_MASK_32           0x10
#define FIXUP_TRG_MASK_ADDITIVE32   0x20
#define FIXUP_TRG_MASK_ORDINAL16    0x40
#define FIXUP_TRG_MASK_ORDINAL8     0x80

#pragma pack()

#endif // LX_H


