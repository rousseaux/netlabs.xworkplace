
/*
 *@@sourcefile progref-entities.h:
 *      string replacements for PROGREF.INF
 *
 * This file is a workaround to permit progref.ipf to be generated
 * using h2i.exe without having to rewrite h2i fix its shortcomings.
 *
 * Most notably, it fails to replace &quot; and &gt; which then
 * causes errors in ipfc.exe.  Defining these two entities seems
 * to workaround the problem.  However, defining &lt; here as well
 * just causes errors in h2i.
 *
 *@@added V1.0.9 (2011-10-12) [rwalsh]: needed to create progref.ipf using h2i.exe
 */

#define quot "\""
#define gt ">"

