
/*
 *@@sourcefile audit.c:
 *      various audit code.
 */

/*
 *      Copyright (C) 2000 Ulrich M�ller.
 *      Based on the MWDD32.SYS example sources,
 *      Copyright (C) 1995, 1996, 1997  Matthieu Willm (willm@ibm.net).
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifdef __IBMC__
#pragma strings(readonly)
#endif

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include <limits.h>
#include <string.h>
#include <stdarg.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"
#include "xwpsec32.sys\SecHlp.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

#include "xwpsec32.sys\fnmatch.h"
#include "xwpsec32.sys\ctype.h"

/* From:                                                          */
/* atol.c (emx+gcc) -- Copyright (c) 1990-1993 by Eberhard Mattes */

long __atol (const char *string)
{
  long n;
  char neg;

  n = 0; neg = 0;
  while (*string == ' ' || *string == '\t') ++string;
  if (*string == '+')
    ++string;
  else if (*string == '-')
    {
      ++string; neg = 1;
    }
  while (*string >= '0' && *string <= '9')
    {
      n = n * 10 + (*string - '0');
      ++string;
    }
  return (neg ? -n : n);
}

/* From:                                                           */
/* ctype.c (emx+gcc) -- Copyright (c) 1990-1993 by Eberhard Mattes */

unsigned char _ctype[257] =
          {
/* -1 */    0,
/* 00 */    _CNTRL,
/* 01 */    _CNTRL,
/* 02 */    _CNTRL,
/* 03 */    _CNTRL,
/* 04 */    _CNTRL,
/* 05 */    _CNTRL,
/* 06 */    _CNTRL,
/* 07 */    _CNTRL,
/* 08 */    _CNTRL,
/* 09 */    _CNTRL|_SPACE,
/* 0A */    _CNTRL|_SPACE,
/* 0B */    _CNTRL|_SPACE,
/* 0C */    _CNTRL|_SPACE,
/* 0D */    _CNTRL|_SPACE,
/* 0E */    _CNTRL,
/* 0F */    _CNTRL,
/* 10 */    _CNTRL,
/* 11 */    _CNTRL,
/* 12 */    _CNTRL,
/* 13 */    _CNTRL,
/* 14 */    _CNTRL,
/* 15 */    _CNTRL,
/* 16 */    _CNTRL,
/* 17 */    _CNTRL,
/* 18 */    _CNTRL,
/* 19 */    _CNTRL,
/* 1A */    _CNTRL,
/* 1B */    _CNTRL,
/* 1C */    _CNTRL,
/* 1D */    _CNTRL,
/* 1E */    _CNTRL,
/* 1F */    _CNTRL,
/* 20 */    _PRINT|_SPACE,
/* 21 */    _PRINT|_PUNCT,
/* 22 */    _PRINT|_PUNCT,
/* 23 */    _PRINT|_PUNCT,
/* 24 */    _PRINT|_PUNCT,
/* 25 */    _PRINT|_PUNCT,
/* 26 */    _PRINT|_PUNCT,
/* 27 */    _PRINT|_PUNCT,
/* 28 */    _PRINT|_PUNCT,
/* 29 */    _PRINT|_PUNCT,
/* 2A */    _PRINT|_PUNCT,
/* 2B */    _PRINT|_PUNCT,
/* 2C */    _PRINT|_PUNCT,
/* 2D */    _PRINT|_PUNCT,
/* 2E */    _PRINT|_PUNCT,
/* 2F */    _PRINT|_PUNCT,
/* 30 */    _PRINT|_DIGIT|_XDIGIT,
/* 31 */    _PRINT|_DIGIT|_XDIGIT,
/* 32 */    _PRINT|_DIGIT|_XDIGIT,
/* 33 */    _PRINT|_DIGIT|_XDIGIT,
/* 34 */    _PRINT|_DIGIT|_XDIGIT,
/* 35 */    _PRINT|_DIGIT|_XDIGIT,
/* 36 */    _PRINT|_DIGIT|_XDIGIT,
/* 37 */    _PRINT|_DIGIT|_XDIGIT,
/* 38 */    _PRINT|_DIGIT|_XDIGIT,
/* 39 */    _PRINT|_DIGIT|_XDIGIT,
/* 3A */    _PRINT|_PUNCT,
/* 3B */    _PRINT|_PUNCT,
/* 3C */    _PRINT|_PUNCT,
/* 3D */    _PRINT|_PUNCT,
/* 3E */    _PRINT|_PUNCT,
/* 3F */    _PRINT|_PUNCT,
/* 40 */    _PRINT|_PUNCT,
/* 41 */    _PRINT|_UPPER|_XDIGIT,
/* 42 */    _PRINT|_UPPER|_XDIGIT,
/* 43 */    _PRINT|_UPPER|_XDIGIT,
/* 44 */    _PRINT|_UPPER|_XDIGIT,
/* 45 */    _PRINT|_UPPER|_XDIGIT,
/* 46 */    _PRINT|_UPPER|_XDIGIT,
/* 47 */    _PRINT|_UPPER,
/* 48 */    _PRINT|_UPPER,
/* 49 */    _PRINT|_UPPER,
/* 4A */    _PRINT|_UPPER,
/* 4B */    _PRINT|_UPPER,
/* 4C */    _PRINT|_UPPER,
/* 4D */    _PRINT|_UPPER,
/* 4E */    _PRINT|_UPPER,
/* 4F */    _PRINT|_UPPER,
/* 50 */    _PRINT|_UPPER,
/* 51 */    _PRINT|_UPPER,
/* 52 */    _PRINT|_UPPER,
/* 53 */    _PRINT|_UPPER,
/* 54 */    _PRINT|_UPPER,
/* 55 */    _PRINT|_UPPER,
/* 56 */    _PRINT|_UPPER,
/* 57 */    _PRINT|_UPPER,
/* 58 */    _PRINT|_UPPER,
/* 59 */    _PRINT|_UPPER,
/* 5A */    _PRINT|_UPPER,
/* 5B */    _PRINT|_PUNCT,
/* 5C */    _PRINT|_PUNCT,
/* 5D */    _PRINT|_PUNCT,
/* 5E */    _PRINT|_PUNCT,
/* 5F */    _PRINT|_PUNCT,
/* 60 */    _PRINT|_PUNCT,
/* 61 */    _PRINT|_LOWER|_XDIGIT,
/* 62 */    _PRINT|_LOWER|_XDIGIT,
/* 63 */    _PRINT|_LOWER|_XDIGIT,
/* 64 */    _PRINT|_LOWER|_XDIGIT,
/* 65 */    _PRINT|_LOWER|_XDIGIT,
/* 66 */    _PRINT|_LOWER|_XDIGIT,
/* 67 */    _PRINT|_LOWER,
/* 68 */    _PRINT|_LOWER,
/* 69 */    _PRINT|_LOWER,
/* 6A */    _PRINT|_LOWER,
/* 6B */    _PRINT|_LOWER,
/* 6C */    _PRINT|_LOWER,
/* 6D */    _PRINT|_LOWER,
/* 6E */    _PRINT|_LOWER,
/* 6F */    _PRINT|_LOWER,
/* 70 */    _PRINT|_LOWER,
/* 71 */    _PRINT|_LOWER,
/* 72 */    _PRINT|_LOWER,
/* 73 */    _PRINT|_LOWER,
/* 74 */    _PRINT|_LOWER,
/* 75 */    _PRINT|_LOWER,
/* 76 */    _PRINT|_LOWER,
/* 77 */    _PRINT|_LOWER,
/* 78 */    _PRINT|_LOWER,
/* 79 */    _PRINT|_LOWER,
/* 7A */    _PRINT|_LOWER,
/* 7B */    _PRINT|_PUNCT,
/* 7C */    _PRINT|_PUNCT,
/* 7D */    _PRINT|_PUNCT,
/* 7E */    _PRINT|_PUNCT,
/* 7F */    _CNTRL,
/* 80 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* A0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* B0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* C0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* D0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* E0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* F0 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
          };

/* From:                                                             */
/* fnmatch.c (emx+gcc) -- Copyright (c) 1994-1995 by Eberhard Mattes */

INLINE int _nls_tolower (int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 'a' - 'A';
    else
        return c;
}

/* In OS/2 and DOS styles, both / and \ separate components of a path.
   This macro returns true iff C is a separator. */

#define IS_OS2_COMP_SEP(C)  ((C) == '/' || (C) == '\\')


/* This macro returns true if C is at the end of a component of a
   path. */

#define IS_OS2_COMP_END(C)  ((C) == 0 || IS_OS2_COMP_SEP (C))


/* Return a pointer to the next component of the path SRC, for OS/2
   and DOS styles.  When the end of the string is reached, a pointer
   to the terminating null character is returned. */

static const unsigned char *skip_comp_os2 (const unsigned char *src)
{
  /* Skip characters until hitting a separator or the end of the
     string. */

  while (!IS_OS2_COMP_END (*src))
    ++src;

  /* Skip the separator if we hit a separator. */

  if (*src != 0)
    ++src;
  return src;
}


/* Compare a single component (directory name or file name) of the
   paths, for OS/2 and DOS styles.  MASK and NAME point into a
   component of the wildcard and the name to be checked, respectively.
   Comparing stops at the next separator.  The FLAGS argument is the
   same as that of fnmatch().  HAS_DOT is true if a dot is in the
   current component of NAME.  The number of dots is not restricted,
   even in DOS style.  Return _FNM_MATCH iff MASK and NAME match.
   Note that this function is recursive. */

static int match_comp_os2 (const unsigned char *mask,
                           const unsigned char *name,
                           unsigned flags, int has_dot)
{
  int rc;

  for (;;)
    switch (*mask)
      {
      case 0:

        /* There must be no extra characters at the end of NAME when
           reaching the end of MASK unless _FNM_PATHPREFIX is set: in
           that case, NAME may point to a separator. */

        if (*name == 0)
          return _FNM_MATCH;
        if ((flags & _FNM_PATHPREFIX) && IS_OS2_COMP_SEP (*name))
          return _FNM_MATCH;
        return FNM_NOMATCH;

      case '/':
      case '\\':

        /* Separators match separators. */

        if (IS_OS2_COMP_SEP (*name))
          return _FNM_MATCH;

        /* If _FNM_PATHPREFIX is set, a trailing separator in MASK
           is ignored at the end of NAME. */

        if ((flags & _FNM_PATHPREFIX) && mask[1] == 0 && *name == 0)
          return _FNM_MATCH;

        /* Stop comparing at the separator. */

        return FNM_NOMATCH;

      case '?':

        /* A question mark matches one character.  It does not match a
           dot.  At the end of the component (and before a dot), it
           also matches zero characters. */

        if (*name != '.' && !IS_OS2_COMP_END (*name))
          ++name;
        ++mask;
        break;

      case '*':

        /* An asterisk matches zero or more characters.  In DOS mode,
           dots are not matched. */

        do
          {
            ++mask;
          } while (*mask == '*');
        for (;;)
          {
            rc = match_comp_os2 (mask, name, flags, has_dot);
            if (rc != FNM_NOMATCH)
              return rc;
            if (IS_OS2_COMP_END (*name))
              return FNM_NOMATCH;
            if (*name == '.' && (flags & _FNM_STYLE_MASK) == _FNM_DOS)
              return FNM_NOMATCH;
            ++name;
          }

      case '.':

        /* A dot matches a dot.  It also matches the implicit dot at
           the end of a dot-less NAME. */

        ++mask;
        if (*name == '.')
          ++name;
        else if (has_dot || !IS_OS2_COMP_END (*name))
          return FNM_NOMATCH;
        break;

      default:

        /* All other characters match themselves. */

        if (flags & _FNM_IGNORECASE)
          {
            if (_nls_tolower (*mask) != _nls_tolower (*name))
              return FNM_NOMATCH;
          }
        else
          {
            if (*mask != *name)
              return FNM_NOMATCH;
          }
        ++mask; ++name;
        break;
      }
}


/* Compare a single component (directory name or file name) of the
   paths, for all styles which need component-by-component matching.
   MASK and NAME point to the start of a component of the wildcard and
   the name to be checked, respectively.  Comparing stops at the next
   separator.  The FLAGS argument is the same as that of fnmatch().
   Return _FNM_MATCH iff MASK and NAME match. */

static int match_comp (const unsigned char *mask, const unsigned char *name,
                       unsigned flags)
{
  const unsigned char *s;

  switch (flags & _FNM_STYLE_MASK)
    {
    case _FNM_OS2:
    case _FNM_DOS:

      /* For OS/2 and DOS styles, we add an implicit dot at the end of
         the component if the component doesn't include a dot. */

      s = name;
      while (!IS_OS2_COMP_END (*s) && *s != '.')
        ++s;
      return match_comp_os2 (mask, name, flags, *s == '.');

    default:
      return _FNM_ERR;
    }
}



/* In Unix styles, / separates components of a path.  This macro
   returns true iff C is a separator. */

#define IS_UNIX_COMP_SEP(C)  ((C) == '/')


/* This macro returns true if C is at the end of a component of a
   path. */

#define IS_UNIX_COMP_END(C)  ((C) == 0 || IS_UNIX_COMP_SEP (C))


/* Match complete paths for Unix styles.  The FLAGS argument is the
   same as that of fnmatch().  COMP points to the start of the current
   component in NAME.  Return _FNM_MATCH iff MASK and NAME match.  The
   backslash character is used for escaping ? and * unless
   FNM_NOESCAPE is set. */

static int match_unix (const unsigned char *mask, const unsigned char *name,
                       unsigned flags, const unsigned char *comp)
{
  unsigned char c1, c2;
  char invert, matched;
  const unsigned char *start;
  int rc;

  for (;;)
    switch (*mask)
      {
      case 0:

        /* There must be no extra characters at the end of NAME when
           reaching the end of MASK unless _FNM_PATHPREFIX is set: in
           that case, NAME may point to a separator. */

        if (*name == 0)
          return _FNM_MATCH;
        if ((flags & _FNM_PATHPREFIX) && IS_UNIX_COMP_SEP (*name))
          return _FNM_MATCH;
        return FNM_NOMATCH;

      case '?':

        /* A question mark matches one character.  It does not match
           the component separator if FNM_PATHNAME is set.  It does
           not match a dot at the start of a component if FNM_PERIOD
           is set. */

        if (*name == 0)
          return FNM_NOMATCH;
        if ((flags & FNM_PATHNAME) && IS_UNIX_COMP_SEP (*name))
          return FNM_NOMATCH;
        if (*name == '.' && (flags & FNM_PERIOD) && name == comp)
          return FNM_NOMATCH;
        ++name; ++mask;
        break;

      case '*':

        /* An asterisk matches zero or more characters.  It does not
           match the component separator if FNM_PATHNAME is set.  It
           does not match a dot at the start of a component if
           FNM_PERIOD is set. */

        if (*name == '.' && (flags & FNM_PERIOD) && name == comp)
          return FNM_NOMATCH;
        do
          {
            ++mask;
          } while (*mask == '*');
        for (;;)
          {
            rc = match_unix (mask, name, flags, comp);
            if (rc != FNM_NOMATCH)
              return rc;
            if (*name == 0)
              return FNM_NOMATCH;
            if ((flags & FNM_PATHNAME) && IS_UNIX_COMP_SEP (*name))
              return FNM_NOMATCH;
            ++name;
          }

      case '/':

        /* Separators match only separators.  If _FNM_PATHPREFIX is set, a
           trailing separator in MASK is ignored at the end of
           NAME. */

        if (!(IS_UNIX_COMP_SEP (*name)
              || ((flags & _FNM_PATHPREFIX) && *name == 0
                  && (mask[1] == 0
                      || (!(flags & FNM_NOESCAPE) && mask[1] == '\\'
                          && mask[2] == 0)))))
          return FNM_NOMATCH;

        ++mask;
        if (*name != 0) ++name;

        /* This is the beginning of a new component if FNM_PATHNAME is
           set. */

        if (flags & FNM_PATHNAME)
          comp = name;
        break;

      case '[':

        /* A set of characters.  Always case-sensitive. */

        if (*name == 0)
          return FNM_NOMATCH;
        if ((flags & FNM_PATHNAME) && IS_UNIX_COMP_SEP (*name))
          return FNM_NOMATCH;
        if (*name == '.' && (flags & FNM_PERIOD) && name == comp)
          return FNM_NOMATCH;

        invert = 0; matched = 0;
        ++mask;

        /* If the first character is a ! or ^, the set matches all
           characters not listed in the set. */

        if (*mask == '!' || *mask == '^')
          {
            ++mask; invert = 1;
          }

        /* Loop over all the characters of the set.  The loop ends if
           the end of the string is reached or if a ] is encountered
           unless it directly follows the initial [ or [-. */

        start = mask;
        while (!(*mask == 0 || (*mask == ']' && mask != start)))
          {
            /* Get the next character which is optionally preceded by
               a backslash. */

            c1 = *mask++;
            if (!(flags & FNM_NOESCAPE) && c1 == '\\')
              {
                if (*mask == 0)
                  break;
                c1 = *mask++;
              }

            /* Ranges of characters are written as a-z.  Don't forget
               to check for the end of the string and to handle the
               backslash.  If the character after - is a ], it isn't a
               range. */

            if (*mask == '-' && mask[1] != ']')
              {
                ++mask;         /* Skip the - character */
                if (!(flags & FNM_NOESCAPE) && *mask == '\\')
                  ++mask;
                if (*mask == 0)
                  break;
                c2 = *mask++;
              }
            else
              c2 = c1;

            /* Now check whether this character or range matches NAME. */

            if (c1 <= *name && *name <= c2)
              {
                if (!invert)
                  matched = 1;
              }
            else
              {
                if (invert)
                  matched = 1;
              }
          }

        /* If the end of the string is reached before a ] is found,
           back up to the [ and compare it to NAME. */

        if (*mask == 0)
          {
            if (*name != '[')
              return FNM_NOMATCH;
            ++name;
            mask = start;
            if (invert)
              --mask;
          }
        else
          {
            if (!matched)
              return FNM_NOMATCH;
            ++mask;             /* Skip the ] character */
            if (*name != 0) ++name;
          }
        break;

      case '\\':
        ++mask;
        if (flags & FNM_NOESCAPE)
          {
            if (*name != '\\')
              return FNM_NOMATCH;
            ++name;
          }
        else if (*mask == '*' || *mask == '?')
          {
            if (*mask != *name)
              return FNM_NOMATCH;
            ++mask; ++name;
          }
        break;

      default:

        /* All other characters match themselves. */

        if (flags & _FNM_IGNORECASE)
          {
            if (_nls_tolower (*mask) != _nls_tolower (*name))
              return FNM_NOMATCH;
          }
        else
          {
            if (*mask != *name)
              return FNM_NOMATCH;
          }
        ++mask; ++name;
        break;
      }
}


/* Check whether the path name NAME matches the wildcard MASK.  Return
   0 (_FNM_MATCH) if it matches, _FNM_NOMATCH if it doesn't.  Return
   _FNM_ERR on error. The operation of this function is controlled by
   FLAGS.  This is an internal function, with unsigned arguments. */


static int _fnmatch_unsigned (const unsigned char *mask,
                              const unsigned char *name,
                              unsigned flags)
{
  int m_drive, n_drive, rc;

  /* Match and skip the drive name if present. */

  m_drive = ((isalpha (mask[0]) && mask[1] == ':') ? mask[0] : -1);
  n_drive = ((isalpha (name[0]) && name[1] == ':') ? name[0] : -1);

  if (m_drive != n_drive)
    {
      if (m_drive == -1 || n_drive == -1)
        return FNM_NOMATCH;
      if (!(flags & _FNM_IGNORECASE))
        return FNM_NOMATCH;
      if (_nls_tolower (m_drive) != _nls_tolower (n_drive))
        return FNM_NOMATCH;
    }

  if (m_drive != -1) mask += 2;
  if (n_drive != -1) name += 2;

  /* Colons are not allowed in path names, except for the drive name,
     which was skipped above. */

  if (strchr (mask, ':') != NULL)
    return _FNM_ERR;
  if (strchr (name, ':') != NULL)
    return FNM_NOMATCH;

  /* The name "\\server\path" should not be matched by mask
     "\*\server\path".  Ditto for /. */

  switch (flags & _FNM_STYLE_MASK)
    {
    case _FNM_OS2:
    case _FNM_DOS:

      if (IS_OS2_COMP_SEP (name[0]) && IS_OS2_COMP_SEP (name[1]))
        {
          if (!(IS_OS2_COMP_SEP (mask[0]) && IS_OS2_COMP_SEP (mask[1])))
            return FNM_NOMATCH;
          name += 2;
          mask += 2;
        }
      break;

    case _FNM_POSIX:

      if (name[0] == '/' && name[1] == '/')
        {
          int i;

          name += 2;
          for (i = 0; i < 2; ++i)
            if (mask[0] == '/')
              ++mask;
            else if (mask[0] == '\\' && mask[1] == '/')
              mask += 2;
            else
              return FNM_NOMATCH;
        }

      /* In Unix styles, treating ? and * w.r.t. components is simple.
         No need to do matching component by component. */

      return match_unix (mask, name, flags, name);
    }

  /* Now compare all the components of the path name, one by one.
     Note that the path separator must not be enclosed in brackets. */

  while (*mask != 0 || *name != 0)
    {

      /* If _FNM_PATHPREFIX is set, the names match if the end of MASK
         is reached even if there are components left in NAME. */

      if (*mask == 0 && (flags & _FNM_PATHPREFIX))
        return _FNM_MATCH;

      /* Compare a single component of the path name. */

      rc = match_comp (mask, name, flags);
      if (rc != _FNM_MATCH)
        return rc;

      /* Skip to the next component or to the end of the path name. */

      mask = skip_comp_os2 (mask);
      name = skip_comp_os2 (name);
    }

  /* If we reached the ends of both strings, the names match. */

  if (*mask == 0 && *name == 0)
    return _FNM_MATCH;

  /* The names do not match. */

  return FNM_NOMATCH;
}


/* This is the public entrypoint, with signed arguments. */
int DH32ENTRY __fnmatch (const char *mask, const char *name, int flags)
{
  return _fnmatch_unsigned (mask, name, flags);
}

/*
 *@@ strhncpy0:
 *      like strncpy, but always appends a 0 character.
 */

ULONG strhncpy0(PSZ pszTarget,
                const char *pszSource,
                ULONG cbSource)
{
    ULONG ul = 0;
    PSZ pTarget = pszTarget,
        pSource = (PSZ)pszSource;

    for (ul = 0; ul < cbSource; ul++)
        if (*pSource)
            *pTarget++ = *pSource++;
        else
            break;
    *pTarget = 0;

    return (ul);
}

char * DH32ENTRY __strpbrk (const char *string1, const char *string2)
{
  char table[256];

  memset (table, 1, 256);
  while (*string2 != 0)
    table[(unsigned char)*string2++] = 0;
  table[0] = 0;
  while (table[(unsigned char)*string1])
    ++string1;
  return (*string1 == 0 ? NULL : (char *)string1);
}

char *__strtok (char *string1, const char *string2)
{
  char table[256];
  unsigned char *p, *result;
    static unsigned char *next = NULL;
  if (string1 != NULL)
    p = (unsigned char *)string1;
  else
    {
      if (next == NULL)
        return NULL;
      p = next;
    }
  memset (table, 0, 256);
  while (*string2 != 0)
    table[(unsigned char)*string2++] = 1;
  table[0] = 0;
  while (table[*p])
    ++p;
  result = p;
  table[0] = 1;
  while (!table[*p])
    ++p;
  if (*p == 0)
    {
      if (p == result)
        {
          next = NULL;
          return NULL;
        }
    }
  else
    *p++ = 0;
  next = p;
  return (char *)result;
}

char *__strupr (char *string)
{
  unsigned char *p;

  for (p = (unsigned char *)string; *p != 0; ++p)
    if (islower (*p))
      *p = (unsigned char)_toupper (*p);
  return string;
}

long DH32ENTRY __strtol (const char *string, char **end_ptr, int radix)
{
  const unsigned char *s;
  char neg;
  long result;

  s = string;
  while (isspace (*s))
    ++s;

  neg = 0;
  if (*s == '-')
    {
      neg = 1; ++s;
    }
  else if (*s == '+')
    ++s;

  if ((radix == 0 || radix == 16) && s[0] == '0'
      && (s[1] == 'x' || s[1] == 'X'))
    {
      radix = 16; s += 2;
    }
  if (radix == 0)
    radix = (s[0] == '0' ? 8 : 10);

  result = 0;                   /* Keep the compiler happy */
  if (radix >= 2 && radix <= 36)
    {
      unsigned long n, max1, max2, lim;
      enum {no_number, ok, overflow} state;
      unsigned char c;

      lim = (neg ? -(unsigned long)LONG_MIN : LONG_MAX);
      max1 = lim / radix;
      max2 = lim - max1 * radix;
      n = 0; state = no_number;
      for (;;)
        {
          c = *s;
          if (c >= '0' && c <= '9')
            c = c - '0';
          else if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 10;
          else if (c >= 'a' && c <= 'z')
            c = c - 'a' + 10;
          else
            break;
          if (c >= radix)
            break;
          if (n >= max1 && (n > max1 || (unsigned long)c > max2))
            state = overflow;
          if (state != overflow)
            {
              n = n * radix + (unsigned long)c;
              state = ok;
            }
          ++s;
        }
      switch (state)
        {
        case no_number:
          result = 0;
          s = string;
          /* Don't set errno!? */
          break;
        case ok:
          result = (neg ? -n : n);
          break;
        case overflow:
          result = (neg ? LONG_MIN : LONG_MAX);
          break;
        }
    }
  else
    {
      result = 0;
    }
  if (end_ptr != NULL)
    *end_ptr = (char *)s;
  return result;
}

/*
 *  From:
 *
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#ifdef OS2
#define VA_START(ap, last) ap = ((va_list)__StackToFlat(&last)) + __nextword(last)
#endif

static int skip_atoi(const char **s);
static char * number(char * str, long num, int base, int size, int precision, int type);
int sprintf(char * buf, const char *fmt, ...);

#if !defined(MINIFSD) && !defined(MWDD32) && !defined(FSD32)
/*
 * Routines accessed in interrupt context (here through the 'printk' call)
 */
#pragma alloc_text(EXT2_FIXED_CODE, sprintf)
#pragma alloc_text(EXT2_FIXED_CODE, vsprintf)
#pragma alloc_text(EXT2_FIXED_CODE, number)
#pragma alloc_text(EXT2_FIXED_CODE, skip_atoi)
#endif

size_t strnlen(const char * s, size_t count)
{
        const char *sc;

        for (sc = s; *sc != '\0' && count--; ++sc)
                /* nothing */;
        return sc - s;
}
#ifdef XXXFSD32
unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
        unsigned long result = 0,value;

        if (!base) {
                base = 10;
                if (*cp == '0') {
                        base = 8;
                        cp++;
                        if ((*cp == 'x') && isxdigit(cp[1])) {
                                cp++;
                                base = 16;
                        }
                }
        }
        while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
            ? toupper(*cp) : *cp)-'A'+10) < base) {
                result = result*base + value;
                cp++;
        }
        if (endp)
                *endp = (char *)cp;
        return result;
}
#endif
/* we use this so that we can do without the ctype library */
#define is_digit(c)     ((c) >= '0' && (c) <= '9')

static int skip_atoi(const char **s)
{
        int i=0;

        while (is_digit(**s))
                i = i*10 + *((*s)++) - '0';
        return i;
}

#define ZEROPAD 1               /* pad with zero */
#define SIGN    2               /* unsigned/signed long */
#define PLUS    4               /* show plus */
#define SPACE   8               /* space if plus */
#define LEFT    16              /* left justified */
#define SPECIAL 32              /* 0x */
#define LARGE   64              /* use 'ABCDEF' instead of 'abcdef' */

#ifndef OS2
#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })
#endif

static char * number(char * str, long num, int base, int size, int precision
        ,int type)
{
        char c,sign,tmp[66];
        const char *digits="0123456789abcdefghijklmnopqrstuvwxyz";
        int i;

        if (type & LARGE)
                digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        if (type & LEFT)
                type &= ~ZEROPAD;
        if (base < 2 || base > 36)
                return 0;
        c = (type & ZEROPAD) ? '0' : ' ';
        sign = 0;
        if (type & SIGN) {
                if (num < 0) {
                        sign = '-';
                        num = -num;
                        size--;
                } else if (type & PLUS) {
                        sign = '+';
                        size--;
                } else if (type & SPACE) {
                        sign = ' ';
                        size--;
                }
        }
        if (type & SPECIAL) {
                if (base == 16)
                        size -= 2;
                else if (base == 8)
                        size--;
        }
        i = 0;
        if (num == 0)
                tmp[i++]='0';
        else while (num != 0) {
                unsigned long __res;
                __res = ((unsigned long) num) % (unsigned long) base;
                num = ((unsigned long) num) / (unsigned long) base;
                tmp[i++] = digits[__res];
             }
        if (i > precision)
                precision = i;
        size -= precision;
        if (!(type&(ZEROPAD+LEFT)))
                while(size-->0)
                        *str++ = ' ';
        if (sign)
                *str++ = sign;
        if (type & SPECIAL)
                if (base==8)
                        *str++ = '0';
                else if (base==16) {
                        *str++ = '0';
                        *str++ = digits[33];
                }
        if (!(type & LEFT))
                while (size-- > 0)
                        *str++ = c;
        while (i < precision--)
                *str++ = '0';
        while (i-- > 0)
                *str++ = tmp[i];
        while (size-- > 0)
                *str++ = ' ';
        return str;
}

int DH32ENTRY __vsprintf(char *buf, const char *fmt, va_list args)
{
        int len;
        unsigned long num;
        int i, base;
        char * str;
        char *s;

        int flags;              /* flags to number() */

        int field_width;        /* width of output field */
        int precision;          /* min. # of digits for integers; max
                                   number of chars for from string */
        int qualifier;          /* 'h', 'l', or 'L' for integer fields */

        for (str=buf ; *fmt ; ++fmt) {
                if (*fmt != '%') {
                        *str++ = *fmt;
                        continue;
                }

                /* process flags */
                flags = 0;
                repeat:
                        ++fmt;          /* this also skips first '%' */
                        switch (*fmt) {
                                case '-': flags |= LEFT; goto repeat;
                                case '+': flags |= PLUS; goto repeat;
                                case ' ': flags |= SPACE; goto repeat;
                                case '#': flags |= SPECIAL; goto repeat;
                                case '0': flags |= ZEROPAD; goto repeat;
                                }

                /* get field width */
                field_width = -1;
                if (is_digit(*fmt))
                        field_width = skip_atoi(__StackToFlat(&fmt));
                else if (*fmt == '*') {
                        ++fmt;
                        /* it's the next argument */
                        field_width = va_arg(args, int);
                        if (field_width < 0) {
                                field_width = -field_width;
                                flags |= LEFT;
                        }
                }

                /* get the precision */
                precision = -1;
                if (*fmt == '.') {
                        ++fmt;
                        if (is_digit(*fmt))
                                precision = skip_atoi(__StackToFlat(&fmt));
                        else if (*fmt == '*') {
                                ++fmt;
                                /* it's the next argument */
                                precision = va_arg(args, int);
                        }
                        if (precision < 0)
                                precision = 0;
                }

                /* get the conversion qualifier */
                qualifier = -1;
                if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
                        qualifier = *fmt;
                        ++fmt;
                }

                /* default base */
                base = 10;

                switch (*fmt) {
                case 'c':
                        if (!(flags & LEFT))
                                while (--field_width > 0)
                                        *str++ = ' ';
                        *str++ = (unsigned char) va_arg(args, int);
                        while (--field_width > 0)
                                *str++ = ' ';
                        continue;

                case 's':
                        s = va_arg(args, char *);
                        if (!s)
                                s = "<NULL>";

                        len = strnlen(s, precision);

                        if (!(flags & LEFT))
                                while (len < field_width--)
                                        *str++ = ' ';
                        for (i = 0; i < len; ++i)
                                *str++ = *s++;
                        while (len < field_width--)
                                *str++ = ' ';
                        continue;

                case 'p':
                        if (field_width == -1) {
                                field_width = 2*sizeof(void *);
                                flags |= ZEROPAD;
                        }
                        str = number(str,
                                (unsigned long) va_arg(args, void *), 16,
                                field_width, precision, flags);
                        continue;


                case 'n':
                        if (qualifier == 'l') {
                                long * ip = va_arg(args, long *);
                                *ip = (str - buf);
                        } else {
                                int * ip = va_arg(args, int *);
                                *ip = (str - buf);
                        }
                        continue;

                /* integer number formats - set up the flags and "break" */
                case 'o':
                        base = 8;
                        break;

                case 'X':
                        flags |= LARGE;
                case 'x':
                        base = 16;
                        break;

                case 'd':
                case 'i':
                        flags |= SIGN;
                case 'u':
                        break;

                default:
                        if (*fmt != '%')
                                *str++ = '%';
                        if (*fmt)
                                *str++ = *fmt;
                        else
                                --fmt;
                        continue;
                }
                if (qualifier == 'l')
                        num = va_arg(args, unsigned long);
                else if (qualifier == 'h')
                        if (flags & SIGN)
                                num = va_arg(args, short);
                        else
                                num = va_arg(args, unsigned short);
                else if (flags & SIGN)
                        num = va_arg(args, int);
                else
                        num = va_arg(args, unsigned int);
                str = number(str, num, base, field_width, precision, flags);
        }
        *str = '\0';
        return str-buf;
}


