
/*
 *@@sourcefile xwpsec32.h:
 *      declarations for xwpsec32.sys.
 *
 */

/*
 *  Utility functions
 */

void _sprintf(const char *pcszFormat, ...);
extern int utilOpenLog(VOID);
extern int utilWriteLog(const char *pcszFormat, ...);
extern VOID utilWriteLogInfo(VOID);

extern unsigned long utilGetTaskPID(void);

extern BOOL utilNeedsVerify(VOID);

extern int utilSemRequest(PULONG pulMutex,
                          ULONG ulTimeout);
extern VOID utilSemRelease(PULONG pulMutex);

extern int utilDaemonRequest(ULONG ulEventCode);
extern VOID utilDaemonDone(VOID);

/*
 *  Regular OS/2 PDD strategy routines
 */

// extern int sec32_write(PTR16 reqpkt);
extern int sec32_close(PTR16 reqpkt);
// extern int sec32_read(PTR16 reqpkt);
extern int sec32_open(PTR16 reqpkt);
extern int sec32_init_base(PTR16 reqpkt);
extern int sec32_invalid_command(PTR16 reqpkt);
extern int sec32_ioctl(PTR16 reqpkt);
extern int sec32_shutdown(PTR16 reqpkt);
extern int sec32_init_complete(PTR16 reqpkt);


/*
 * SES kernel security callbacks
 */
extern struct SecImp_s SecurityImports;

ULONG CallType OPEN_PRE(PSZ pszPath,
                        ULONG fsOpenFlags,
                        ULONG fsOpenMode,
                        ULONG SFN);

ULONG CallType OPEN_POST(PSZ pszPath,
                         ULONG fsOpenFlags,
                         ULONG fsOpenMode,
                         ULONG SFN,
                         ULONG ActionTaken,
                         ULONG RC);

ULONG CallType READ_PRE(ULONG SFN,
                        PUCHAR pBuffer,
                        ULONG cbBuf);

VOID  CallType READ_POST(ULONG SFN,
                         PUCHAR PBUFFER,
                         ULONG CBBYTESREAD,
                         ULONG RC);

ULONG CallType WRITE_PRE(ULONG SFN,
                         PUCHAR pBuffer,
                         ULONG cbBuf);

VOID  CallType WRITE_POST(ULONG SFN,
                          PUCHAR PBUFFER,
                          ULONG CBBUF,
                          ULONG cbBytesWritten,
                          ULONG RC);

VOID  CallType CLOSE(ULONG SFN);

VOID  CallType CHGFILEPTR(ULONG SFN,
                          PLONG  SeekOff,
                          PUSHORT SeekType,
                          PLONG Absolute, // physical (FS)
                          PLONG pLogical); // logical (app)

ULONG CallType DELETE_PRE(PSZ pszPath);

VOID  CallType DELETE_POST(PSZ pszPath,
                           ULONG RC);

ULONG CallType MOVE_PRE(PSZ pszNewPath,
                        PSZ pszOldPath);

VOID  CallType MOVE_POST(PSZ pszNewPath,
                         PSZ pszOldPath,
                         ULONG RC);

ULONG CallType LOADEROPEN(PSZ pszPath,
                          ULONG SFN);

ULONG CallType GETMODULE(PSZ pszPath);

ULONG CallType EXECPGM(PSZ pszPath,
                       PCHAR pchArgs);

ULONG CallType FINDFIRST(PFINDPARMS pParms);
/*          typedef struct {
             PSZ    pszPath;      // well formed path
             ULONG  ulHandle;     // search handle
             ULONG  rc;           // rc user got from findfirst
             PUSHORT pResultCnt;  // count of found files
             USHORT usReqCnt;     // count user requested
             USHORT usLevel;      // search level
             USHORT usBufSize;    // user buffer size
             USHORT fPosition;    // use position information?
             PCHAR  pcBuffer;     // ptr to user buffer
             ULONG  Position;     // Position for restarting search
             PSZ    pszPosition;  // file to restart search with
         } FINDPARMS, *PFINDPARMS;
*/

ULONG CallType CALLGATE16(VOID);

ULONG CallType CALLGATE32(VOID);

ULONG CallType SETFILESIZE(ULONG SFN,
                           PULONG pSize);

ULONG CallType QUERYFILEINFO_POST(ULONG  SFN,
                                  PUCHAR pBuffer,
                                  ULONG  cbBuffer,
                                  ULONG  InfoLevel);

ULONG CallType MAKEDIR(PSZ pszPath);

ULONG CallType CHANGEDIR(PSZ pszPath);

ULONG CallType REMOVEDIR(PSZ pszPath);

ULONG CallType FINDNEXT(PFINDPARMS pParms);

ULONG CallType FINDFIRST3X(ULONG ulSrchHandle,
                           PSZ pszPath);  //DGE02

VOID  CallType FINDCLOSE(ULONG ulSearchHandle);              //DGE02

ULONG CallType FINDFIRSTNEXT3X(ULONG ulSrchHandle,
                               PSZ pszFile);//DGE02

ULONG CallType FINDCLOSE3X(ULONG ulSrchHandle);              //DGE02

VOID  CallType EXECPGM_POST(PSZ pszPath,
                            PCHAR pchArgs,
                            ULONG NewPID);

ULONG CallType CREATEVDM(PSZ pszProgram,
                         PSZ pszArgs);

VOID  CallType CREATEVDMPOST(int rc);

ULONG CallType SETDATETIME(PDATETIME pDateTimeBuf);

ULONG CallType SETFILEINFO(ULONG  SFN,
                           PUCHAR pBuffer,
                           ULONG  cbBuffer,
                           ULONG  InfoLevel);

ULONG CallType SETFILEMODE(PSZ     pszPath,
                           PUSHORT pNewAttribute);

ULONG CallType SETPATHINFO(PSZ    pszPathName,
                           ULONG  InfoLevel,
                           PUCHAR pBuffer,
                           ULONG  cbBuffer,
                           ULONG  InfoFlags);

ULONG CallType DEVIOCTL(ULONG  SFN,
                        ULONG  Category, /* Category 8 and 9 only.*/
                        ULONG  Function,
                        PUCHAR pParmList,
                        ULONG  cbParmList,
                        PUCHAR pDataArea,
                        ULONG  cbDataArea,
                        ULONG  PhysicalDiskNumber); /* Category 9 only */

ULONG CallType TRUSTEDPATHCONTROL(VOID);

VOID CallType AUDIT_STARTEVENT(ULONG AuditRC,
                               PSESSTARTEVENT pSESStartEvent);

VOID CallType AUDIT_WAITEVENT(ULONG AuditRC,
                              PSESEVENT pSESEventInfo,
                              ULONG ulTimeout);

VOID CallType AUDIT_RETURNEVENTSTATUS(ULONG AuditRC,
                                      PSESEVENT pSESEventInfo);

VOID CallType AUDIT_REGISTERDAEMON(ULONG AuditRC,
                                   ULONG ulDaemonID,
                                   ULONG ulEventList);

VOID CallType AUDIT_RETURNWAITEVENT(ULONG AuditRC,
                                    PSESEVENT pSESEventInfo,
                                    ULONG ulTimeout);

