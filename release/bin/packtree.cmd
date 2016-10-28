/* PACKTREE.CMD:
 *
 * Packs a folder and all of its subfolders into a
 * zip file of the same name. The files are not yet
 * deleted (if you want this, add an "-m" switch to
 * the "zip" command line below.
 *
 * This requires that ZIP.EXE (InfoZip) be on the PATH.
 *
 *  (W) (C) 1998 Ulrich M”ller. All rights reserved.
 */

'@echo off'

/* get the name of the folder without the full path */
foldername = filespec("NAME", directory());

/* now pack */
Say "Packing "directory()" into file "foldername".zip..."
"zip -r9 ..\"foldername".zip *"

