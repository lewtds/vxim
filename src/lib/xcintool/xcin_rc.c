/*
    Copyright (C) 1999 by  XCIN TEAM

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    For any question or suggestion, please mail to xcin mailing-list:
    xcin@linux.org.tw, or the maintainer Tung-Han Hsieh: thhsieh@linux.org.tw
*/      

/*
 *  The xcin_rc subsystem is changed to use SIOD (Small-frontpoint 
 *  Implementation Of the Scheme programming language. The format
 *  of the rcfile should be written in LISP/Scheme programming language.
 *
 *  Thanks to the contribution of Yung-Ching Hsiao <yhsiao@cae.wisc.edu>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constant.h"
#include "xcintool.h"
#include "../siod/siod.h"

#define NSTRINGP(x) NTYPEP(x, tc_string)
#define STRINGP(x) TYPEP(x, tc_string)

/* STRINGP(x) == (string? obj) in lisp */
/* NULLP(x) == (null? obj) in lisp */
/* NNULLP(x) == (not (null? obj)) in lisp */

/* in scheme:
 *  (define mysymbol "Hello")
 * in C:
 *  symbol name = "mysymbol"
 *  symbol value = "Hello"
 *  cintern(symbol_name): returns the LISP symbol ``mysymbol''
 *  symbol_value(LISP_symbol, LISP_env): returns the symbol value.
 *   where, LISP_env is the interpreter "context" to evaluate the symbol.
 */

static FILE *rc_fp;

/*
 *  Since here we use scheme language to retrive the rc-value directly,
 *  so we don't need the C function call to retrive. It is left here for
 *  reference.
 *
static LISP get_value(char *symbol_name) 
{
    return symbol_value(cintern(symbol_name), NIL);
}
 */

static void no_puts(char *unused) 
{
/* no operation */
}


static LISP myread() 
{
    if (feof(rc_fp)) 
	return err(NULL , NIL);
    return 
	lreadf(rc_fp);
}


/*---------------------------------------------------------------------------

	XCIN RC interface functions. 

---------------------------------------------------------------------------*/

void
read_resource(char *rcfile)
{
    struct repl_hooks hook;

    if (! rcfile)
	perr(XCINMSG_ERROR, N_("null rcfile name.\n"));

    init_storage();
    init_subrs();
    hook.repl_puts = no_puts;
    hook.repl_read = myread;
    hook.repl_eval = NULL;
    hook.repl_print = NULL;
    siod_verbose(cons(flocons(1), NIL));	/* verbose level = 1 */

    rc_fp = open_file(rcfile, "rt", XCINMSG_ERROR);
    if (repl_driver(0, 0, &hook) != 0)
	perr(XCINMSG_ERROR, N_("rcfile \"%s\" reading error.\n"), rcfile);
    fclose(rc_fp);
}

static char isep=' ';

int
get_resource(char **cmd_list, char *value, int v_size, int n_cmd_list)
{
    char *buf, *s, *vbuf, *v, word[1024];
    int buf_size=1024;

    buf = malloc(buf_size);

    if (n_cmd_list == 1) {
	if (strlen(cmd_list[0]) >= buf_size - 1) {
	    buf_size *= 2;
	    buf = realloc(buf, buf_size);
	}
	strcpy(buf, cmd_list[0]);
    }
    else {
	int i, slen=0, tmplen;
	char tmp[1024];

	buf[0] = '\0';
        for (i=n_cmd_list-1; i>0; i--) {
	    tmplen = snprintf(tmp, 1024, "(cadr (assq '%s ", cmd_list[i]);
	    if (slen + tmplen - 1 > buf_size) {
		buf_size *= 2;
		buf = realloc(buf, buf_size);
		buf[slen] = '\0';
	    }
	    strcat(buf, tmp);
	    slen += tmplen;
	}
	tmplen = (n_cmd_list - 1) * 2;
	if (slen + strlen(cmd_list[0]) - tmplen - 1 > buf_size) {
	    buf_size *= 2;
	    buf = realloc(buf, buf_size);
	    buf[slen] = '\0';
	}
	for (i=0; i<tmplen; i++)
	    tmp[i] = ')';
	tmp[i] = '\0';
	strcat(buf, cmd_list[0]);
	strcat(buf, tmp);
    }
    if (repl_c_string(buf, 0, 0, buf_size) != 0 || ! buf[0]) {
	free(buf);
	return 0;
    }
    s = buf;
    v = vbuf = malloc(buf_size);
    while (get_word(&s, word, 1024, "()")) {
	if (word[0] != '(' && word[0] != ')')
	    v += sprintf(v, "%s%c", word, isep);
    }
    free(buf);

    if (v <= vbuf) {
	free(vbuf);
	return 0;
    }
    *(v-1) = '\0';
    if (! strcmp(vbuf, "**unbound-marker**")) {
	free(vbuf);
	return 0;
    }
    else {
        strncpy(value, vbuf, v_size);
        free(vbuf);
        return 1;
    }
}

int
get_resource_long(char **cmd_list, char *value, int v_size, 
		  int n_cmd_list, int sep)
{
    int r;

    isep = sep;
    r = get_resource(cmd_list, value, v_size, n_cmd_list);
    isep = ' ';
    return r;
}

/*----------------------------------------------------------------------------

	Unified RC reading procedure.

----------------------------------------------------------------------------*/


static void
find_rcfile(char *user_home, char *rcfn, int rcfn_size)
{
    /* User: $HOME/.xcin/xcinrc */
    snprintf(rcfn, rcfn_size, "%s/%s/%s",
        user_home, XCIN_USER_DIR, XCIN_DEFAULT_RC);
    if (check_file_exist(rcfn, FTYPE_FILE))
        return;

    /* User: $HOME/.xcinrc */
    snprintf(rcfn, rcfn_size, "%s/.%s", user_home, XCIN_DEFAULT_RC);
    if (check_file_exist(rcfn, FTYPE_FILE))
        return;

    /* Default: /etc/xcinrc */
    snprintf(rcfn, rcfn_size, "%s/%s", XCIN_DEFAULT_RCDIR, XCIN_DEFAULT_RC);
    if (check_file_exist(rcfn, FTYPE_FILE))
        return;

    /* Cannot find any rcfile. */
    perr(XCINMSG_ERROR, N_("rcfile not found.\n"));
}

char *
read_xcinrc(char *rcfn, char *user_home)
/*
 *  rcfn: can be the one which comes from the shell prompt arguement.
 *  rcfn can be found in "user_home" or "default" places. Where "default"
 *  	is hard coded.
 *  the returned value is the actually path of the rcfile.
 */
{
    static char rcfile[256];
    char *s;

    rcfile[0] = '\0';
    if (rcfn && rcfn[0])
	strncpy(rcfile, rcfn, 256);
    else if ((s = getenv("XCIN_RCFILE")))
	strncpy(rcfile, s, 256);

    if (rcfile[0] && ! check_file_exist(rcfile, FTYPE_FILE)) {
        perr(XCINMSG_WARNING, 
		N_("rcfile \"%s\" does not exist, ignore.\n"), rcfile);
	rcfile[0] = '\0';
    }
    if (! rcfile[0])
        find_rcfile(user_home, rcfile, 256);

    read_resource(rcfile);
    return rcfile;
}
