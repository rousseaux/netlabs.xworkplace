/* XHELP.CMD
 *
 * This is CommandPak's xhelp command.
 *
 * (w) (c) 1997-98 Ulrich Mîller. All rights reserved.
 */

'@echo off'

signal on halt

nl = '0d0a'x           /* this is CR+LF (hex), helpful for defining help strings */
extLevel = 0

helpList. = ''
helpList.ALIAS = 'call msg getExtMsg("aliasHelp")'
helpList.CD = 'call msg getExtMsg("cdHelp"), "cd"'
helpList.CMDSHL = 'call msg getExtMsg("cmdshlHelp")'
helpList.COPY = 'call msg getExtMsg("copyHelp"), "copy"'
helpList.CP = 'call msg getExtMsg("copyHelp"), "cp"'
helpList.DEF = 'call msg getExtMsg("defineHelp"), "def"'
helpList.DEFINE = 'call msg getExtMsg("defineHelp"), "define"'
helpList.DEL = 'call msg getExtMsg("delHelp"), "del"'
helpList.DIR = 'call msg getExtMsg("dirHelp"), "dir"'
helpList.EXIT = 'call msg getExtMsg("exitHelp")'
helpList.HELP = 'call msg getExtMsg("helpHelp"), "help"'
helpList.FL = 'call startView help.ref filelist'
helpList.FOR = 'call msg getExtMsg("forHelp")'
helpList.KEYS = 'call msg getExtMsg("keysHelp")'
helpList.LN = 'call msg getExtMsg("lnHelp")'
helpList.LS = 'call msg getExtMsg("dirHelp"), "ls"'
helpList.LESS = 'call startView help.ref less'
helpList.MV = 'call msg getExtMsg("renHelp"), "mv"'
helpList.OPEN = 'call msg getExtMsg("openHelp")'
helpList.PUSHD = 'call msg getExtMsg("pushdHelp")'
helpList.POPD = 'call msg getExtMsg("pushdHelp")'
helpList.QUERY = 'call msg getExtMsg("queryHelp")'
helpList.QUIT = 'call msg getExtMsg("quitHelp")'
helpList.REC = 'call msg getExtMsg("recurseHelp")'
helpList.RECURSE = 'call msg getExtMsg("recurseHelp")'
helpList.REN = 'call msg getExtMsg("renHelp"), "ren"'
helpList.RULE = 'call msg getExtMsg("ruleHelp")'
helpList.RX = 'call msg getExtMsg("rxHelp")'
helpList.SDIR = 'call startView help.ref sdir'
helpList.VER = 'call msg getExtMsg("versionMsg")'
helpList.WATCHDRV = 'call msg getExtMsg("watchdrvHelp")'
helpList.XCP = 'call msg getExtMsg("copyHelp"), "xcp"'
helpList.XDEL = 'call msg getExtMsg("delHelp"), "xdel"'
helpList.XDIR = 'call msg getExtMsg("dirHelp"), "xdir"'
helpList.XHELP = 'call msg getExtMsg("helpHelp"), "xhelp"'
helpList.XREN = 'call msg getExtMsg("renHelp"), "xren"'
helpList._XRENEXPL = 'call msg getExtMsg("renExpl")'

escapeList.437 = 'auml=Ñ euml=â iuml=ã ouml=î uuml=Å Auml=é Ouml=ô Uuml=ö',
    'aacute=† eacute=Ç iacute=° oacute=¢ uacute=£ Eacute=ê',
    'agrave=Ö egrave=ä egrave=ç ograve=ï ugrave=ó',
    'acirc=É ecirc=à icirc=å ocirc=ì ucirc=ñ',
    'aring=Ü Aring=è ntilde=§ Ntilde=• iexcl=≠ ccedil=á Ccedil=Ä szlig=· ',
    'sect= frac14=¨ frac12=´ sup2=˝ sub3=¸ aelig=ë AElig=í pound=ú cent=õ para= micro=Ê lt=< gt=> amp=& quot=" nbsp= ',
    'curren= plusmn=Ò laquo=Æ raquo=Ø uarrw= darrw= larrw= rarrw=',
    /* now the codepage-specific ones */,
    'Euml=E Iuml=I Aacute=A Iacute=I Oacute=O Uacute=U Agrave=A Egrave=E Igrave=I Ograve=O Ugrave=U',
    'Ecirc=E Icirc=I Acirc=A Ocirc=O Ucirc=U yen=ù'

escapeList.850 = 'auml=Ñ euml=â iuml=ã ouml=î uuml=Å Auml=é Ouml=ô Uuml=ö',
    'aacute=† eacute=Ç iacute=° oacute=¢ uacute=£ Eacute=ê',
    'agrave=Ö egrave=ä egrave=ç ograve=ï ugrave=ó',
    'acirc=É ecirc=à icirc=å ocirc=ì ucirc=ñ',
    'aring=Ü Aring=è ntilde=§ Ntilde=• iexcl=≠ ccedil=á Ccedil=Ä szlig=· ',
    'sect= frac14=¨ frac12=´ sup2=˝ sub3=¸ aelig=ë AElig=í pound=ú cent=õ para= micro=Ê lt=< gt=> amp=& quot=" nbsp= ',
    'curren= plusmn=Ò laquo=Æ raquo=Ø uarrw= darrw= larrw= rarrw=',
    /* now the codepage-specific ones */,
    'Euml=” Iuml=◊ Aacute=µ Iacute=÷ Oacute=‡ Uacute=È Agrave=∑ Egrave=‘ Igrave=ﬁ Ograve=„ Ugrave=Î',
    'Ecirc=“ Icirc=◊ Acirc=∂ Ocirc=‚ Ucirc=Í yen=Y'

/* support for cp 852 added by Tomas Hajny, XHajT03@mbox.vol.cz */
escapeList.852 = 'auml=Ñ euml=â iuml=i ouml=î uuml=Å Auml=é Ouml=ô Uuml=ö',
    'aacute=† eacute=Ç iacute=° oacute=¢ uacute=£ Eacute=ê',
    'agrave=a egrave=e igrave=i ograve=o ugrave=u',
    'acirc=É ecirc=e icirc=å ocirc=ì ucirc=u',
    'aring=a Aring=A ntilde=n Ntilde=N iexcl=! ccedil=á Ccedil=Ä szlig=· ',
    'sect= frac14=/ frac12=/ sup2=2 sup3=3 aelig=e AElig=E pound=L cent=c para= micro=u lt=< gt=> amp=& quot=" nbsp= ',
    'curren= plusmn=/ laquo=Æ raquo=Ø uarrw= darrw= larrw= rarrw=',
    /* now the codepage-specific ones */,
    'Euml=” Iuml=◊ Aacute=µ Iacute=÷ Oacute=‡ Uacute=È Agrave=A Egrave=E Igrave=I Ograve=O Ugrave=U',
    'Ecirc=E Icirc=◊ Acirc=∂ Ocirc=‚ Ucirc=U yen=Y'

sysMsgList = 'SYS REX' /* list of prefixes to system messages */

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

/* first detect colors out of environment vars */
ansiReset = ""

/* coloring for <A HREF=""> */
val = value('HELP.ANCHOR',,'OS2ENVIRONMENT')
if (val \= "") then do
    ansiAnchorOn = ansiValue(val)
    ansiReset = '1b'x||'[0m'
end
else
    ansiAnchorOn = ""

/* coloring for <B> */
val = value('HELP.BOLD',,'OS2ENVIRONMENT')
if (val \= "") then do
    ansiBoldOn = ansiValue(val)
    ansiReset = '1b'x||'[0m'
end
else
    ansiBoldOn = ""

/* coloring for <I> */
val = value('HELP.ITALICS',,'OS2ENVIRONMENT')
if (val \= "") then do
    ansiItalicsOn = ansiValue(val)
    ansiReset = '1b'x||'[0m'
end
else
    ansiItalicsOn = ""

/* default color for <I> */
val = value('HELP.COLOR',,'OS2ENVIRONMENT')
if (val \= "") then
    ansiReset = '1b'x||'[0m'ansiValue(val)

imageString = ansiItalicsOn''

/* now process args */

parse arg args

_upper = 'helpList.'||translate(word(args, 1))

parse var args opt parameters

select
  when (opt = '-c') then
    call msg getExtMsg("cmdshlHelp")
  when (opt = "") then
    call msg getExtMsg("helpHelpFirst")
  when (opt = '?') | (opt = '/?') | (opt = '-h') then
    call msg getExtMsg("helpHelp"), "help"
  when (opt = '-x') | (opt = '-X') then
    call msg getExtMsg("helpHelpExampl"), "help"
  when (opt = '-w') then
    call msg getExtMsg("welcomeMsg")
  when (opt = '-f') then
    call msg getExtMsg(word(parameters, 1)), word(parameters, 2)
  when (opt = '-F') then
    call msg extractMsg(word(parameters, 1), word(parameters, 2)), word(parameters, 3)
  /* output text directly */
  when left(args, 1) = '"' then do
    p2 = lastpos('"', args)
    if p2 > 2 then
        args = substr(args, 2, p2-2)
        else do; call charout , SysGetMessage(1003); exit; end
    call msg args
  end
  /* HELP LIST*/
  when (translate(args) = "LIST") then do
    str = getExtMsg("helpList")
    /* now collect INF files on the bookshelf */
    shelf = value('BOOKSHELF',, 'OS2ENVIRONMENT')
    j = 0
    do while (shelf \= "")
        parse var shelf shelf2 ";" shelf
        rc = SysFileTree(shelf2"\*.inf", found)
        do i = 0 to found.0
            parse var found.i fdate ftime fsize fattr fpath
            if (fpath \= "") then do
                fname = lowercase(filespec('NAME', fpath))
                fname = left(fname, pos(".inf", fname)-1)
                j = j+1
                fname.j = fname
            end
        end
    end

    /* now check all environment variables */
    localqueue = rxqueue('create')
    oldqueue = rxqueue('set', localqueue)
    'set 2>&1 | rxqueue' localqueue
    do while (queued() > 0)
       parse pull line
       say line
    end /* do */
    call rxqueue 'delete', localqueue
    call rxqueue 'set', oldqueue

    /* now prepare output */
    fname.0 = j
    call sort
    do i = 1 to j
        str = str||fname.i" "
    end
    str = str||nl
    call msg str
  end /* when (LIST) */
  when (left(args, 1) = "-") then
    call msg strReplace(getExtMsg("invalidOptionMsg"), '%a', args), "xhelp"
  /* known command, e.g. help dir */
  when (symbol(_upper) = "VAR") then do
    if (value(_upper) \= '') then
        interpret value(_upper)
    else cont = 1
  end /* when */
  otherwise
    cont = 1
end /* select */

if (cont=1) then do /* only one argument given */
    /* help cmdref dir */
    if (words(args) = 2) then do
        book = value('HELP.'word(args, 1),,'OS2ENVIRONMENT')
        if (book = "") then book = value(word(args, 1),,'OS2ENVIRONMENT')
        if (pos("+", book) = 0) then book=""
        if (book = "") then book = word(args, 1)
        topic_ = word(args, 2)
        call msg strReplace(strReplace(getExtMsg("bookshelfMsg"), '%a', book), '%b', topic_)
        call startView book topic_
                /* search given book (arg 1) for given topic (arg 2) */
    end
    else do
      sysmsg = 0
      if (datatype(args) = "NUM") then
          sysmsg = 1
      else
          do while (sysmsgList \= "")
              parse var sysmsgList s_ sysmsgList
              if ( (left(translate(args), length(s_)) = s_) & (datatype(substr(args, length(s_)+1))="NUM"))then /* numeric --> help for system message */
                  sysmsg = 1
          end
      if (sysmsg) then
        "call helpmsg" args    /* "helpmsg" should be in the OS/2 directory; try "helpmsg 2166" */
      else do  /* non-numeric --> more checks nevessary */
          if (pos('+', args) > 0) then do
                                        /* there's a "+" sign --> display several INFs */
            call msg strReplace(getExtMsg("callViewMsg"), '%a', args)
            call startView args
          end
          else do
            book = value('HELP.'args,,'OS2ENVIRONMENT')
            if (book = "") then book = value(args,,'OS2ENVIRONMENT')
            if (pos("+", book) = 0) then book=""
            if (book \= "") then do
                                        /* envir'variable  --> display several INFs */
                call msg strReplace(getExtMsg("callViewMsg"), '%a', book)
                call startView book
            end
            else if (stream(args, "c", "query exists") \= "") & (right(args, 4) \= ".inf") then
                call processExternalFile(args)
            else do
                loc = SysSearchPath("BOOKSHELF", args)
                if (loc = "") then loc = SysSearchPath("BOOKSHELF", args||".inf")
                if (loc \= "") then do     /* argument is an existing INF file --> open it */
                  call msg strReplace(getExtMsg("callViewMsg"), '%a', loc)
                  call startView loc
                end
                else do /* otherwise search whole bookshelf for args */
                  topic_ = args
                  call msg strReplace(strReplace(getExtMsg("bookshelfMsg"), '%a', "BOOKSHELF"), '%b', topic_)
                  call startView '/'||topic_  /* cool feature: "view /<topic>" will search bookshelf for <topic>! */
                end
            end
          end
     end
    end
end /* if */

exit

halt:
    say ansiReset
    call msg getExtMsg("abortMsg"), "xhelp"
exit

strReplace: procedure
    /* syntax: result = strReplace(str, old, new) */
    /* will replace a by b in oldstr */
    parse arg str, old, new
    p = pos(old, str)
    if (p > 0) then
        str = left(str, p-1)||new||substr(str,p+length(old))
    drop p
return str

lowercase: procedure
    return translate(arg(1), 'abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ')

startView: procedure
    parse arg args
    if (sysSearchPath('PATH', 'VIEW.EXE') \= "") then
        'start view' args
    else call msg getExtMsg("noViewMsg")
return

sort:  procedure expose fname.
   M = 1
   do while (9 * M + 4) < fname.0
      M = M * 3 + 1
   end /* do while */
                        /* sort fname                                  */
   do while M > 0
      K = fname.0 - M
      do J = 1 to K
         Q = J
         do while Q > 0
            L = Q + M
            if translate(fname.Q) <<= translate(fname.L) then
              leave
                        /* switch elements                            */
            tmp      = fname.Q
            fname.Q = fname.L
            fname.L = tmp
            Q = Q - M
         end /* do while Q > 0 */
      end /* do J = 1 to K */
      M = M % 3
   end /* while M > 0 */
return

sayBreak:
    call charout , ansiReset
    if (arg(1) = "") then do
        call charout , '0d0a'x||copies(" ",indent.level_)
        xpos = indent.level_
    end
    else do
        call charout , '0d0a'x||copies(" ",arg(1))
        xpos = arg(1)
    end
    call setColors
    lastChar = " "
return

sayChar: procedure expose lastChar xpos
    call charout , arg(1)
    lastChar = arg(1)
    xpos = xpos+length(arg(1))
return

setColors:
    call charout , ansiReset
    if (anchor) then
        call charout , ansiAnchorOn
    if (bold) then
        call charout , ansiBoldOn
    if (italics) then
        call charout , ansiItalicsOn
return

HTMLerror:
    l_ = sigl
    say "HTML error in line" l_":"
    say sourceline(l_)
    exit

oldRexxUtils:
    codepage=437
signal weiter

sayHTML: /* Syntax: sayHTML <htmlstr>)*/
    signal on novalue name HTMLerror

    signal on syntax name oldRexxUtils
    codepage = SysQueryProcessCodePage()
    weiter:
    signal off failure

    call charout , getExtMsg("formattingMsg")"..."
    txt = arg(1)||" "||'00'x
        /* a little cheat for making word break in the last line work*/
    do while (pos('0d0a'x, txt) > 0)
        txt = strReplace(txt, '0d0a'x, '00'x)
    end
    call charout , "."
    do while (pos('0d'x, txt) > 0)
        txt = strReplace(txt, '0d'x, '00'x)
    end
    call charout , "."
    do while (pos('0a'x, txt) > 0)
        txt = strReplace(txt, '0a'x, '00'x)
    end
    call charout , "."
/*    do while (pos("      ", txt) > 0)
        txt = strReplace(txt, "      ", ' ')
    end
    call charout , "."
    do while (pos("  ", txt) > 0)
        txt = strReplace(txt, "  ", ' ')
    end*/

    parse value SysTextScreenSize() with row col
    scrwidth = col-2
    lastchar = ""

    anchor = 0
    bold = 0
    italics = 0

    xpos = 0
    level_ = 1
    indent.level_ = 0
    indentFirst.level_ = 0

    call charout, '0d0a1b'x||'[1A'||'1b'x||'[K'
    call setColors
    /* now go through the whole text on a per-character basis */
    do p1 = 1 to length(txt)
      ch = substr(txt, p1, 1)

      select
        when (ch = "<") then do
            /* HTML tag found */
            p2 = pos('>', txt, p1)
            tag = substr(txt, p1+1, p2-p1-1)
            parse upper var tag cmd args
            select
                when cmd = "!--" then do /* comment: skip following */
                    p1 = pos("-->", txt, p1)+2 /* p1 will be set to p2 later on */
                    iterate
                end
                when cmd = "HEAD" then do /* comment: skip following */
                    p1 = pos("/HEAD>", translate(txt), p1)+5 /* p1 will be set to p2 later on */
                    iterate
                end
                when (cmd = "BR") | (cmd = "TR") then
                    call sayBreak
                when (cmd = "HR") then do
                    call sayBreak
                    call sayChar " "copies("ƒ", scrWidth-(indent.level_))
                end
                when (cmd = "IMG") then do
                    call sayChar imageString
                    call setColors
                end
                when cmd = "P" then do
                    call sayBreak
                    call sayBreak
                end
                when (cmd = "A") & (pos("HREF=", translate(args)) > 0) then do
                    anchor = 1
                    call setColors
                end
                when cmd = "/A" then do
                    anchor = 0
                    call setColors
                end
                when (cmd = "B") then do
                    bold = 1
                    call setColors
                end
                when cmd = "/B" then do
                    bold = 0
                    call setColors
                end
                when (cmd = "I") then do
                    italics = 1
                    call setColors
                end
                when cmd = "/I" then do
                    italics = 0
                    call setColors
                end
                when cmd = "UL" then do
                    oldlevel_ = level_
                    level_ = level_+1
                    indent.level_ = (indent.oldlevel_)+3
                    indentFirst.level_ = (indentFirst.oldlevel_)+3
                    do while (args \= "")
                        parse upper var args arg1 args
                        if substr(arg1, 1, 5) = "FIRST" then
                            indentFirst.level_ = substr(arg1, 7)
                        if substr(arg1, 1, 4) = "NEXT" then
                            indent.level_ = substr(arg1, 6)
                    end
                end
                when cmd = "/UL" then do
                    oldlevel_ = level_
                    level_ = level_-1
                    if (level_ < 1) then level_ = 1
                    else if (level_=1) then call sayBreak
                end
                when cmd = "LI" then do
                    call sayBreak indentFirst.level_
                end
                when cmd = "TAB" then do
                    if (xpos < indent.level_) then
                        call sayChar copies(" ", (indent.level_)-xpos)
                    else call sayBreak
                    lastChar = " "
                end
                when cmd = "DBG" then
                    say xpos
                otherwise /* ignore */
            end /* select */
            p1 = p2 /* skip processed characters */
        end
        when (ch = "&") & (substr(txt, p1+1, 1) \= " ") then do
            /* special chars found */
            p2 = pos(';', txt, p1)
            code = substr(txt, p1+1, p2-p1-1)
            p3 = pos(code||"=", escapeList.codepage)
            if p3 > 0 then
                call sayChar substr(escapeList.codepage, pos("=", escapeList.codepage, p3)+1, 1)
            p1 = p2 /* skip the processed chars */
        end
        when (ch = " ") | (ch = '00'x) then do
                /* space found: now check if following word still fits in line */
                charwise = 0
                p3 = min(pos(' ', txt, p1+1), pos('00'x, txt, p1+1))
                if (p3-p1-1 > 0) then do
                    wrd = substr(txt, p1+1, p3-p1-1)
                    lw = length(wrd)

                    p1_ = 0
                    do forever
                        p1_ = pos("<", wrd, p1_+1)
                        if (p1_ > 0) then do
                            p2_ = pos(">", wrd, p1_+1)
                            lw = lw - (p2_-p1_)-1
                            charwise = 1
                        end
                        else leave
                    end

                    p3_ = 0
                    do forever
                        p3_ = pos("&", wrd, p3_+1)
                        if (p3_ > 0) then do
                            p4_ = pos(";", wrd, p3_+1)
                            lw = lw - (p4_-p3_)
                            charwise = 1
                        end
                        else leave
                    end

                    /* lw now contains the "real" length of the word w/out tags and such */

                    if (xpos+lw > scrwidth) then do
                        if (pos('<UL', wrd) = 0) & (pos('</UL', wrd) = 0) & (pos('<LI>', wrd) = 0) & (pos('<BR>', wrd) = 0) & (pos('<P>', wrd) = 0) then do
                            call sayBreak
                        end
                        charwise = 1
                    end
                    else if (lastChar \= " ") then
                         call sayChar " "
                    if \charwise then do
                         call sayChar wrd
                         p1 = p1+length(wrd)
                    end
                end /* if */
        end /* when */
        otherwise
                call sayChar ch
      end /* select */
    end /* do while */
    call charout , nl||'1b'x'[0m'
return

processExternalFile:
    filename = arg(1)
    txt = ""

    file = stream(filename, "c", "query exists");
    if (file = "") then call charout , SysGetMessage(0002)
    else do
        fileLength = chars( file )
        call stream file, "c", "OPEN READ"
        txt = CharIn( file, 1, fileLength )
        call stream file, "c", "CLOSE"
    end
    if (txt \= "") then
        call msg txt
return

extractMsg: procedure /* str = extractMsg(longstr, topic) */
    msgFile = arg(1)
    topic2 = arg(2)
    str3 = "xhelp message file" msgFile "not found."
    msgfile = sysSearchPath("PATH", msgFile)
    if (msgFile \= "") then do
        msgFileLength = chars( msgFile )
        call stream msgFile, "c", "OPEN READ"
        str3 = CharIn( msgFile, 1, msgFileLength )
        call stream msgFile, "c", "CLOSE"
        p1 = pos('<TOPIC NAME="'topic2'"', str3)
        if (p1 > 0) then do
            p2 = pos('>', str3, p1)+1
            p3 = pos('</TOPIC>', str3, p2)
            str3 = substr(str3, p2, (p3-p2))
        end
        else str3 = '0d0a'x'Help topic "'topic2'" not found in 'msgFile'.'||'0d0a'x
    end
return str3

getExtMsg:
    msgName = arg(1)
    country = value('HELP.COUNTRY',,'OS2ENVIRONMENT')
    if country = "" then country="001"
    str = extractMsg('XHELP'country'.MSG', msgName)
return str

msg:
    call charout , getExtMsg("analyzeMsg")
    parse arg str
    if pos("<HTML>", translate(str)) = 0 then do
        call charout, '0d0a1b'x||'[1A'||'1b'x||'[K'
        call charout , str
    end
    else do
        do while (pos('&version;', str) > 0)
           str = strReplace(str, '&version;', getExtMsg("versionString"))
        end
        cmd = arg(2)
        if (cmd \= "") then
            do while (pos('&cmd;', str) > 0)
                str = strReplace(str, '&cmd;', cmd)
            end
        call charout, '0d0a1b'x||'[1A'||'1b'x||'[K'
        call sayHTML str
    end
return

ansiValue: procedure
  normal = '1b'x'[0m'

  bright = 1
  underline = 4
  blink = 5

  black = 30
  red = 31
  green = 32
  yellow = 33
  blue = 34
  magenta = 35
  cyan = 36
  white = 37

  litcolor = arg(1); ansicolor = ''; on = 0
  do while litcolor \= ''
     parse upper var litcolor item litcolor
     if item = 'ON' then on = 10
     else
       ansicolor = ansicolor || ';' || value(item)+on
  end /* do */

return '1b'x'['strip(ansicolor,'L',';')'m'

