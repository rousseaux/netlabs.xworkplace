
typedef struct _PROCRECORD
{
    RECORDCORE  recc;
    ULONG       ulPID;
    CHAR        szPID[30];
    PSZ         pszPID;             // points to szPID
    CHAR        szModuleName[300];
    PSZ         pszModuleName;      // points to szModuleName
    CHAR        szTitle[500];
    PQPROCESS32 pProcess;
} PROCRECORD, *PPROCRECORD;

typedef struct _MODRECORD
{
    RECORDCORE  recc;
    CHAR        szModuleName[300];
    PSZ         pszModuleName;      // points to szModuleName
    PQMODULE32  pModule;
    BOOL        fSubModulesInserted;
} MODRECORD, *PMODRECORD;

#define ID_PROCLISTCNR  1000
#define ID_PROCINFO     1001
#define ID_PROCSPLIT    1002

#define ID_XPSM_MAIN        100

#define ID_XPSM_XPSTAT      200
#define ID_XPSMI_EXIT       201

#define ID_XPSM_VIEW        300
#define ID_XPSMI_PIDLIST    301
#define ID_XPSMI_SIDLIST    302
#define ID_XPSMI_PIDTREE    303
#define ID_XPSMI_MODTREE    304
#define ID_XPSMI_WORDWRAP   305
#define ID_XPSMI_REFRESH    306

