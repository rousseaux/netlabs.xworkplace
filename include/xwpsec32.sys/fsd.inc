;
; $Header$
;

; 32 bits OS/2 device driver and IFS support. Provides 32 bits kernel
; services (DevHelp) and utility functions to 32 bits OS/2 ring 0 code
; (device drivers and installable file system drivers).
; Copyright (C) 1995, 1996, 1997  Matthieu WILLM (willm@ibm.net)
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

CODE32 segment
         extrn FS32_ALLOCATEPAGESPACE : far
;        extrn fs32_attach            : far            ; N/A (local FSD)
;        extrn fs32_attribute         : far            ; N/A (data)
;        extrn fs32_cancellockrequest : far            ; N/A (not supported)
         extrn FS32_CHDIR             : far
         extrn FS32_CHGFILEPTR        : far
         extrn FS32_CLOSE             : far
         extrn FS32_COMMIT            : far
;        extrn fs32_copy              : far            ; N/A (emulated)
         extrn FS32_DELETE            : far
         extrn FS32_DOPAGEIO          : far
         extrn FS32_EXIT              : far
         extrn FS32_FILEATTRIBUTE     : far
         extrn FS32_FILEINFO          : far
;        extrn fs32_fileio            : far            ; N/A (not supported)
;        extrn fs32_filelocks         : far            ; N/A (not supported)
         extrn FS32_FINDCLOSE         : far
         extrn FS32_FINDFIRST         : far
         extrn FS32_FINDFROMNAME      : far
         extrn FS32_FINDNEXT          : far
;        extrn fs32_findnotifyclose   : far            ; N/A (obsolete)
;        extrn fs32_findnotifyfirst   : far            ; N/A (obsolete)
;        extrn fs32_findnotifynext    : far            ; N/A (obsolete)
         extrn FS32_FLUSHBUF          : far
         extrn FS32_FSCTL             : far
         extrn FS32_FSINFO            : far
         extrn fs32_init              : near
         extrn FS32_IOCTL             : far
         extrn FS32_MKDIR             : far
         extrn FS32_MOUNT             : far
         extrn FS32_MOVE              : far
;        extrn fs32_name              : far            ; N/A (data)
         extrn FS32_NEWSIZE           : far
;        extrn fs32_nmpipe            : far            ; N/A (local FSD)
         extrn FS32_OPENCREATE        : far
         extrn FS32_OPENPAGEFILE      : far
         extrn FS32_PATHINFO          : far
;        extrn fs32_processname       : far            ; N/A (no special processing)
         extrn FS32_READ              : far
         extrn FS32_RMDIR             : far
;        extrn fs32_setswap           : far            ; N/A (obsolete)
         extrn FS32_SHUTDOWN          : far
         extrn FS32_WRITE             : far
CODE32 ends

