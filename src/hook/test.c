int CheckRemoveableDrive(void)
{
    APIRET  arc;
    HFILE   hFile;
    ULONG   ulTemp;

    arc = DosOpen("X:",   // <--- CD drive letter!
                  &hFile,
                  &ulTemp,
                  0,
                  FILE_NORMAL,
                  OPEN_ACTION_FAIL_IF_NEW
                         | OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_FLAGS_DASD
                         | OPEN_FLAGS_FAIL_ON_ERROR
                         | OPEN_ACCESS_READONLY
                         | OPEN_SHARE_DENYNONE,
                  NULL);
    if (arc == NO_ERROR)
    {
        UCHAR   achUnit[4] = { 'C', 'D', '0', '1' };
                    // "CD01" string,
                    // req. for all CD-ROM ioctl's
        struct
        {
            UCHAR   uchCtlAdr;
            UCHAR   achUPC[7];
            UCHAR   uchReserved;
            UCHAR   uchFrame;
        } Result;

        ULONG ulDeviceStatus = 0;

        ulTemp = 0;

        arc = DosDevIOCtl(hFile,
                          IOCTL_CDROMDISK,
                          CDROMDISK_GETUPC,
                          achUnit, sizeof(achUnit), NULL,
                          &Result, sizeof(Result), &ulTemp );
        if (arc == NO_ERROR)
        {
            CHAR szUPC[100];
            sprintf(szUPC,
                    "UPC: %02x%02x%02x%02x%02x%02x%02x",
                    Result.achUPC[0], Result.achUPC[1], Result.achUPC[2],
                    Result.achUPC[3], Result.achUPC[4], Result.achUPC[5], Result.achUPC[6]);
            _Pmpf(("New ADR: 0x%lX", Result.uchCtlAdr));
            _Pmpf(("New UPC: %s", szUPC));
        }
        else
            _Pmpf(("DosDevIOCtl rc: %d", arc));

        arc = DosDevIOCtl(hFile,
                          IOCTL_CDROMDISK,
                          CDROMDISK_DEVICESTATUS,
                          achUnit, sizeof(achUnit), NULL,
                          &ulDeviceStatus, sizeof(ulDeviceStatus), &ulTemp);
        if (arc == NO_ERROR)
        {
            _Pmpf(("ulStatus: 0x%lX", ulDeviceStatus));
            _Pmpf(("  CDDA support:  %s", (ulDeviceStatus & (1<<30)) ? "yes" : "no"));
            _Pmpf(("  Playing audio: %s", (ulDeviceStatus & (1<<12)) ? "yes" : "no"));
            _Pmpf(("  Disk present:  %s", (ulDeviceStatus & (1<<11)) ? "yes" : "no"));
            _Pmpf(("  Door open:     %s", (ulDeviceStatus & (1    )) ? "yes" : "no"));
        }
        else
            _Pmpf(("DosDevIOCtl rc: %d", arc));

        DosClose(hFile);
    }
    else
        _Pmpf(("Open failed !\n"));

    return 0;
}

