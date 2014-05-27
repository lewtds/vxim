/*
    Copyright (C) 2000 by  XCIN TEAM

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

    This program is originally a part of XCIN a Chinese Input Method,
    xcin@linux.org.tw, the maintainer Tung-Han Hsieh: thhsieh@linux.org.tw

    Modified to support Vietnamese Telex Input Method
    by Hoang Son Do, dhson@thep.physik.uni-mainz.de.
    
    For any question or suggestion, please Email to xvim mailing-list:
    vietlug@egroups.com, or the maintainer Hoang Son Do:
    dhson@thep.physik.uni-mainz.de
*/      

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "xcintool.h"
#include "module.h"
#include "gen_inp.h"

#ifdef DEBUG
extern int verbose;
#endif

/*----------------------------------------------------------------------------

        gen_inp_init()

----------------------------------------------------------------------------*/

static int
loadtab(gen_inp_conf_t *cf, FILE *fp, char *encoding)
{
    int n, nn, ret=1;
    char modID[MODULE_ID_SIZE];
   
	memset(modID, 0x00, sizeof(modID)); 
    if (fread(modID, sizeof(char), MODULE_ID_SIZE, fp) != MODULE_ID_SIZE ||
        strcmp(modID, "gencin")) {
        perr(XCINMSG_WARNING, N_("gen_inp: %s: invalid tab file.\n"),cf->tabfn);
        return False;
    }

	memset(&cf->header, 0x00, sizeof(cintab_head_t));
    if (fread(&(cf->header), sizeof(cintab_head_t), 1, fp) != 1) {
        perr(XCINMSG_WARNING, N_("gen_inp: %s: reading error.\n"), cf->tabfn);
        return False;
    }

    if (! check_version(GENCIN_VERSION, cf->header.version, 5)) {
        perr(XCINMSG_WARNING, N_("gen_inp: %s: invalid version.\n"), cf->tabfn);
        return False;
    }

    if (strcmp(encoding, cf->header.encoding) != 0) {
        perr(XCINMSG_WARNING, N_("gen_inp: %s: invalid encoding: %s\n"),
             cf->tabfn, cf->header.encoding);
        return False;
    }

    if (! cf->inp_cname)
        cf->inp_cname = cf->header.cname;
    
    n  = cf->header.n_icode;
    nn = cf->ccinfo.total_char;

	cf->icidx = NULL;
    if ((cf->icidx = malloc(sizeof(icode_idx_t) * n)) == NULL) {
		goto errors;
	}

	cf->ichar = NULL;	
    if ((cf->ichar = malloc(sizeof(ichar_t) * nn)) == NULL) {
		goto errors;
	}

	cf->ic1 = NULL;
    if ((cf->ic1 = malloc(sizeof(icode_t) * n)) == NULL) {
		goto errors;
	}

    if (!n || !nn ||
        fread(cf->icidx, sizeof(icode_idx_t), n, fp) != n ||
        fread(cf->ichar, sizeof(ichar_t), nn, fp) != nn ||
        fread(cf->ic1, sizeof(icode_t), n, fp) != n) {

		goto errors;
    }

    if (ret && cf->header.icode_mode == ICODE_MODE2) {
		cf->ic2 = NULL;
        if ((cf->ic2 = malloc(sizeof(icode_t) * n)) == NULL) {
			goto errors;
		}

        if (fread(cf->ic2, sizeof(icode_t), n, fp) != n) {
			goto errors;
        }
    }

	return True;
errors:
	perr(XCINMSG_WARNING, N_("gen_inp: %s: reading error.\n"), cf->tabfn);

	if (cf->icidx != NULL) {
		free(cf->icidx);
		cf->icidx = NULL;
	}

	if (cf->ichar != NULL) {
		free(cf->ichar);
		cf->ichar = NULL;
	}

	if (cf->ic1 != NULL) {
		free(cf->ic1);
		cf->ic1 = NULL;
	}

	if (cf->ic2 != NULL) {
		free(cf->ic2);
		cf->ic2 = NULL;
	}
	
	return False;
}

static unsigned int 
setup_kremap(gen_inp_conf_t *cf, char *value)
{
    int i=0;
    unsigned int num;
    char *s = value, *sp;
    
    while (*s) {
        if (*s == ';')
            i++;
        s++;
    }
    cf->n_kremap = i;
    if ((cf->kremap = malloc(sizeof(kremap_t) * i)) == NULL) {
		perr(XCINMSG_ERROR, N_("%s %d malloc: failed\n"), __FILE__, __LINE__);
		return False;
   	}
 
    s = sp = value;
    for (i=0; i<cf->n_kremap; i++) {
        while (*s != ':')
            s++;
        *s = '\0';
        strncpy(cf->kremap[i].keystroke, sp, INP_CODE_LENGTH+1);
        
        s++;
        sp = s;
        while (*s != ';')
            s++;
        *s = '\0';
        cf->kremap[i].wch.wch = (wchar_t)0;
        if (*sp == '0' && *(sp+1) == 'x') {
            num = strtol(sp+2, NULL, 16);
            cf->kremap[i].wch.s[0] = (num & 0xff00) >> 8;
            cf->kremap[i].wch.s[1] = (num & 0x00ff);
        }
        else
            strncpy((char *)cf->kremap[i].wch.s, sp, WCH_SIZE);
        
        s++;
        sp = s;
    }

	return True;
}

static void
gen_inp_resource(gen_inp_conf_t *cf, char *objname)
{
    char *cmd[2], value[256];
    
    cmd[0] = objname;
    
    cmd[1] = "INP_CNAME";				/* inp_names */
    if (get_resource(cmd, value, 256, 2)) {
        if (cf->inp_cname)
            free(cf->inp_cname);
        cf->inp_cname = (char *)strdup(value);
    }
    cmd[1] = "AUTO_COMPOSE";				/* auto compose */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_AUTOCOMPOSE, 0);
    
    cmd[1] = "AUTO_UPCHAR";				/* auto_up char */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_AUTOUPCHAR, 0);
    cmd[1] = "SPACE_AUTOUP";				/* space key auto_up */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_SPACEAUTOUP, 0);
    cmd[1] = "SELKEY_SHIFT";				/* selkey shift */
    if (get_resource(cmd, value, 256, 2))	
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_SELKEYSHIFT, 0);
    
    cmd[1] = "AUTO_FULLUP";				/* auto full_up */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_AUTOFULLUP, 0);
    cmd[1] = "SPACE_IGNORE";				/* space ignore */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_SPACEIGNOR, 0);
    
    cmd[1] = "AUTO_RESET";				/* auto reset error */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_AUTORESET, 0);
    cmd[1] = "SPACE_RESET";				/* space reset error */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_SPACERESET, 0);
    
    cmd[1] = "SINMD_IN_LINE1";				/* sinmd in line1 mode*/
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_SINMDLINE1, 0);
    cmd[1] = "WILD_ENABLE";				/* wild key enable */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_WILDON, 0);
    cmd[1] = "BEEP_WRONG";				/* beep mode: wrong */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_BEEPWRONG, 0);
    cmd[1] = "BEEP_DUPCHAR";				/* beep mode: dupchar */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_BEEPDUP, 0);
    cmd[1] = "QPHRASE_MODE";				/* qphrase mode */
    if (get_resource(cmd, value, 256, 2))
        cf->modesc = (ubyte_t)(atoi(value) % 256);
    
    cmd[1] = "DISABLE_SEL_LIST";			/* disable selkey */
    if (get_resource(cmd, value, 256, 2)) {
        if (cf->disable_sel_list)
            free(cf->disable_sel_list);
        if (strcmp(value, "NONE"))
            cf->disable_sel_list = (char *)strdup(value);
	else
	    cf->disable_sel_list = NULL;
    }
    cmd[1] = "KEYSTROKE_REMAP";
    if (get_resource(cmd, value, 256, 2)) {
        if (cf->kremap)
            free(cf->kremap);
        if (strcmp(value, "NONE") == 0) {
            cf->kremap = NULL;
            cf->n_kremap = 0;
        }
        else
            setup_kremap(cf, value);
    }

    cmd[1] = "END_KEY";					/* end key enable */
    if (get_resource(cmd, value, 256, 2))
        set_data(&(cf->mode), RC_IFLAG, value, INP_MODE_ENDKEY, 0);
}

static int
gen_inp_init(void *conf, char *objname, xcin_rc_t *xc)
{
    gen_inp_conf_t *cf = (gen_inp_conf_t *)conf, cfd;
    char *s, value[128], truefn[256];
    objenc_t objenc;
    FILE *fp;
    int ret;

    memset(&cfd, 0, sizeof(gen_inp_conf_t));
    if (get_objenc(objname, &objenc) == -1)
        return False;
    gen_inp_resource(&cfd, "gen_inp_default");
    gen_inp_resource(&cfd, objenc.objloadname);
/*
 *  Resource setup.
 */
    cf->inp_ename = (char *)strdup(objenc.objname);
    cf->inp_cname = cfd.inp_cname;
    if ((cfd.mode & INP_MODE_AUTOCOMPOSE))
        cf->mode |= INP_MODE_AUTOCOMPOSE;
    if ((cfd.mode & INP_MODE_AUTOUPCHAR)) {
        cf->mode |= INP_MODE_AUTOUPCHAR;
        if ((cfd.mode & INP_MODE_SPACEAUTOUP))
            cf->mode |= INP_MODE_SPACEAUTOUP;
        if ((cfd.mode & INP_MODE_SELKEYSHIFT))
            cf->mode |= INP_MODE_SELKEYSHIFT;
    }
    if ((cfd.mode & INP_MODE_AUTOFULLUP)) {
        cf->mode |= INP_MODE_AUTOFULLUP;
        if ((cfd.mode & INP_MODE_SPACEIGNOR))
            cf->mode |= INP_MODE_SPACEIGNOR;
    }
    if ((cfd.mode & INP_MODE_AUTORESET))
        cf->mode |= INP_MODE_AUTORESET;
    else if ((cfd.mode & INP_MODE_SPACERESET))
        cf->mode |= INP_MODE_SPACERESET;
    if ((cfd.mode & INP_MODE_SINMDLINE1))
        cf->mode |= INP_MODE_SINMDLINE1;
    if ((cfd.mode & INP_MODE_WILDON))
        cf->mode |= INP_MODE_WILDON;
    if ((cfd.mode & INP_MODE_BEEPWRONG))
        cf->mode |= INP_MODE_BEEPWRONG;
    if ((cfd.mode & INP_MODE_BEEPDUP))
        cf->mode |= INP_MODE_BEEPDUP;
    cf->modesc = cfd.modesc;
    cf->disable_sel_list = cfd.disable_sel_list;
    cf->kremap = cfd.kremap;
    cf->n_kremap = cfd.n_kremap;
    /*
 *  Read the IM tab file.
 */
    ccode_info(&(cf->ccinfo));
    if (! (s = strrchr(cf->inp_ename, '.')) || strcmp(s+1, "tab"))
        snprintf(value, 50, "%s.tab", cf->inp_ename);
    if ((fp = open_data(value, "rb", NULL, truefn, 256, XCINMSG_WARNING))) {
        cf->tabfn = (char *)strdup(truefn);
        ret = loadtab(cf, fp, objenc.encoding);
        fclose(fp);
    }
    else
        return False;

    if (cf->header.n_endkey) {
        if ((cfd.mode & INP_MODE_ENDKEY))
            cf->mode |= INP_MODE_ENDKEY;
    }
    return  ret;
}


/*----------------------------------------------------------------------------
  
  gen_inp_xim_init()

  ----------------------------------------------------------------------------*/

static int
gen_inp_xim_init(void *conf, inpinfo_t *inpinfo)
{
    gen_inp_conf_t *cf = (gen_inp_conf_t *)conf;
    gen_inp_iccf_t *iccf;
    int i;

    if ((inpinfo->iccf = calloc(1, sizeof(gen_inp_iccf_t))) == NULL) {
		perr(XCINMSG_ERROR,N_("%s %d calloc failed\n"));
		return False;
	}

    iccf = (gen_inp_iccf_t *)inpinfo->iccf;
    
    inpinfo->inp_cname = cf->inp_cname;
    inpinfo->inp_ename = cf->inp_ename;
    inpinfo->area3_len = cf->header.n_max_keystroke * 2 + 1;
    inpinfo->guimode = ((cf->mode & INP_MODE_SINMDLINE1)) ? 
        GUIMOD_SINMDLINE1 : 0;

    inpinfo->keystroke_len = 0;
    if ((inpinfo->s_keystroke = 
			calloc(INP_CODE_LENGTH+1, sizeof(wch_t))) == NULL) {
		perr(XCINMSG_ERROR, N_("%s %d calloc failed\n"), __FILE__, __LINE__);
		return False;
	}

    if ((inpinfo->suggest_skeystroke = 
			calloc(INP_CODE_LENGTH+1, sizeof(wch_t))) == NULL) {
		perr(XCINMSG_ERROR,N_("%s %d calloc failed\n"), __FILE__, __LINE__);
		return False;
	}
    
    if (! (cf->mode & INP_MODE_SELKEYSHIFT)) {
        inpinfo->n_selkey = cf->header.n_selkey;
        inpinfo->s_selkey = calloc(inpinfo->n_selkey, sizeof(wch_t));
        for (i=0; i<SELECT_KEY_LENGTH && i<cf->header.n_selkey; i++)
            inpinfo->s_selkey[i].s[0] = cf->header.selkey[i];
    }
    else {
        inpinfo->n_selkey = cf->header.n_selkey+1;
        inpinfo->s_selkey = calloc(inpinfo->n_selkey, sizeof(wch_t));
        for (i=0; i<SELECT_KEY_LENGTH && i<cf->header.n_selkey; i++)
	    inpinfo->s_selkey[i+1].s[0] = cf->header.selkey[i];
    }
    inpinfo->n_mcch = 0;
    inpinfo->mcch = calloc(inpinfo->n_selkey, sizeof(wch_t));
    inpinfo->mcch_grouping = NULL;
    inpinfo->mcch_pgstate = MCCH_ONEPG;
    
    inpinfo->n_lcch = 0;
    inpinfo->lcch = NULL;
    inpinfo->lcch_grouping = NULL;
    inpinfo->cch_publish.wch = (wchar_t)0;
    
    return  True;
}

static unsigned int
gen_inp_xim_end(void *conf, inpinfo_t *inpinfo)
{
    gen_inp_iccf_t *iccf = (gen_inp_iccf_t *)inpinfo->iccf;
    
    if (iccf->n_mcch_list)
        free(iccf->mcch_list);
    free(inpinfo->iccf);
    free(inpinfo->s_keystroke);
    free(inpinfo->suggest_skeystroke);
    free(inpinfo->s_selkey);
    free(inpinfo->mcch);
    inpinfo->iccf = NULL;
    inpinfo->s_keystroke = NULL;
    inpinfo->suggest_skeystroke = NULL;
    inpinfo->s_selkey = NULL;
    inpinfo->mcch = NULL;
    return IMKEY_ABSORB;
}


/*----------------------------------------------------------------------------

        gen_inp_keystroke(), gen_inp_show_keystroke

----------------------------------------------------------------------------*/

static unsigned int
return_wrong(gen_inp_conf_t *cf)
{
    return ((cf->mode & INP_MODE_BEEPWRONG)) ? IMKEY_BELL : IMKEY_ABSORB;
}


static void
reset_keystroke(inpinfo_t *inpinfo, gen_inp_iccf_t *iccf)
{
    inpinfo->s_keystroke[0].wch = (wchar_t)0;
    inpinfo->keystroke_len = 0;
    inpinfo->n_mcch = 0;
    iccf->keystroke[0] = '\0';
    iccf->mode = 0;
    inpinfo->mcch_pgstate = MCCH_ONEPG;
    if (iccf->n_mcch_list) {
        free(iccf->mcch_list);
        iccf->n_mcch_list = 0;
    }
}


/*------------------------------------------------------------------------*/

static int
cmp_icvalue(icode_t *ic1, icode_t *ic2, unsigned int idx,
            icode_t icode1, icode_t icode2, int mode)
{
    if (ic1[idx] > icode1)
        return 1;
    else if (ic1[idx] < icode1)
        return -1;
    else {
        if (! mode)
            return 0;
        else if (ic2[idx] > icode2)
            return 1;
        else if (ic2[idx] < icode2)
            return -1;
        else
            return 0;
    }
}

static int 
bsearch_char(icode_t *ic1, icode_t *ic2, 
	     icode_t icode1, icode_t icode2, int size, int mode, int wild)
{
    int head, middle, end, ret;
    
    head   = 0;
    middle = size / 2;
    end    = size;
    while ((ret=cmp_icvalue(ic1, ic2, middle, icode1, icode2, mode))) {
        if (ret > 0)
            end = middle;
        else
            head = middle + 1;
        middle = (end + head) / 2;
        if (middle == head && middle == end)
            break;
    }
    if (ret == 0) {
        while(middle > 0 &&
              ! cmp_icvalue(ic1, ic2, middle-1, icode1, icode2, mode)) 
	    middle --;
        return middle;
    }
    else
        return (wild) ? middle : -1;
}


static int
match_keystroke_normal(gen_inp_conf_t *cf, 
                       inpinfo_t *inpinfo, gen_inp_iccf_t *iccf)
{
    icode_t icode[2];
    unsigned int size, md, n_ich=0, mcch_size;
    int idx;
    wch_t *mcch;

    size = cf->header.n_icode;
    md = (cf->header.icode_mode == ICODE_MODE2) ? 1 : 0;
    icode[0] = icode[1] = 0;
    
    /*
     *  Search for the first char.
     */
    keys2codes(icode, 2, iccf->keystroke);
    if ((idx = bsearch_char(cf->ic1, cf->ic2, 
                            icode[0], icode[1], size, md, 0)) == -1)
        return 0;
    
    /*
     *  Search for all the chars with the same keystroke.
     */
    mcch_size = inpinfo->n_selkey;
    if ((mcch = malloc(mcch_size * sizeof(wch_t))) == NULL) {
		perr(XCINMSG_ERROR, N_("%s %d malloc failed\n"), __FILE__, __LINE__);
		return False;
	}

    do {
        if (mcch_size <= n_ich) {
            mcch_size *= 2;
            if ((mcch = realloc(mcch, mcch_size * sizeof(wch_t))) == NULL) {
				perr(XCINMSG_ERROR,N_("%s %d realloc failed\n"), 
										__FILE__, __LINE__);
				return False;
			}
        }
        if (! ccode_to_char(cf->icidx[idx], mcch[n_ich].s, WCH_SIZE))
            return 0;
        n_ich ++;
        idx ++;
    } while (idx < size &&
             ! cmp_icvalue(cf->ic1, cf->ic2, idx, icode[0], icode[1], md));
    
    /*
     *  Prepare mcch for display.
     */
    for (idx=0; idx<inpinfo->n_selkey && idx<n_ich; idx++)
        inpinfo->mcch[idx].wch = mcch[idx].wch;
    inpinfo->n_mcch = idx;
    
    if (idx >= n_ich) {
        inpinfo->mcch_pgstate = MCCH_ONEPG;
        free(mcch);
    }
    else {
        inpinfo->mcch_pgstate = MCCH_BEGIN;
        if (iccf->n_mcch_list)
            free(iccf->mcch_list);
        iccf->mcch_list = mcch;
        iccf->n_mcch_list = n_ich;
        iccf->mcch_hidx = 0;	//  refer to index of iccf->mcch_list 
    }
    return 1;
}

static int 
match_keystroke(gen_inp_conf_t *cf, inpinfo_t *inpinfo, gen_inp_iccf_t *iccf)
     /* return: 1: success, 0: false */
{
    int ret;
    inpinfo->n_mcch = 0;
    ret = match_keystroke_normal(cf, inpinfo, iccf);
    return ret;
}

/*------------------------------------------------------------------------*/

static void
commit_char(inpinfo_t *inpinfo, gen_inp_iccf_t *iccf, wch_t *cch)
{
    static char cch_s[WCH_SIZE+1];
    inpinfo->cch = cch_s;
    strncpy(cch_s, (char *)cch->s, WCH_SIZE);
    cch_s[WCH_SIZE] = '\0';
}
// DEBUG0
static unsigned int
commit_keystroke(gen_inp_conf_t *cf, inpinfo_t *inpinfo, gen_inp_iccf_t *iccf)
/* return: the IMKEY state */
{
    if (cf->n_kremap > 0) {
        int i;
        for (i=0; i<cf->n_kremap; i++) {
            if (strcmp(iccf->keystroke, cf->kremap[i].keystroke) == 0) {
                commit_char(inpinfo, iccf, &(cf->kremap[i].wch));
                return IMKEY_COMMIT;
            }
        }
    }
    
    if (match_keystroke(cf, inpinfo, iccf)) {
            commit_char(inpinfo, iccf, inpinfo->mcch);
            return IMKEY_COMMIT;
    }
    else {
        if ((cf->mode & INP_MODE_AUTORESET))
            reset_keystroke(inpinfo, iccf);
        else
            iccf->mode |= INPINFO_MODE_WRONG;
        return return_wrong(cf);
    }
}


/*------------------------------------------------------------------------*/

static unsigned int if_vowel(KeySym keysymtest)
{
    KeySym keysym;

    keysym=keysymtest;
    
    if (keysym==XK_space||keysym==XK_b||keysym==XK_c||keysym==XK_g
        ||keysym==XK_h ||keysym==XK_k||keysym==XK_l||keysym==XK_m||keysym==XK_n
        ||keysym==XK_p  ||keysym==XK_q||keysym==XK_t||keysym==XK_v || keysym==XK_z
        ||keysym==XK_B||keysym==XK_C||keysym==XK_G
        ||keysym==XK_H ||keysym==XK_K||keysym==XK_L||keysym==XK_M||keysym==XK_N
        ||keysym==XK_P  ||keysym==XK_Q||keysym==XK_T||keysym==XK_V || keysym==XK_Z ) 
        return 1;
    else
        return 0;
}
static unsigned int
gen_inp_keystroke(void *conf, inpinfo_t *inpinfo, keyinfo_t *keyinfo)
{
    gen_inp_conf_t *cf = (gen_inp_conf_t *)conf;
    gen_inp_iccf_t *iccf = (gen_inp_iccf_t *)inpinfo->iccf;
    char ticcf;
	char tchar;
    
    KeySym keysym = keyinfo->keysym;
    char *keystr = keyinfo->keystr;
    int len, max_len;
    
    len = inpinfo->keystroke_len;
    max_len = cf->header.n_max_keystroke;

    if(keysym ==XK_Return || keysym == XK_BackSpace){
        reset_keystroke(inpinfo, iccf);
        inpinfo->cch_publish.wch = (wchar_t)0;
        inpinfo->mcch_pgstate = MCCH_ONEPG;
        inpinfo->guimode &= ~(GUIMOD_SELKEYSPOT);
        return IMKEY_IGNORE;
    } else if (keysym == XK_Delete && len) {
		iccf->keystroke[len-1] = '\0';
		inpinfo->s_keystroke[len-1].wch = (wchar_t)0;
		inpinfo->keystroke_len --;
		inpinfo->n_mcch = 0;
		inpinfo->cch_publish.wch = (wchar_t)0;
		inpinfo->mcch_pgstate = MCCH_ONEPG;
		inpinfo->guimode &= ~(GUIMOD_SELKEYSPOT);
		iccf->mode = 0;
            
		if (len-1 > 0 && (cf->mode & INP_MODE_AUTOCOMPOSE)) {
			match_keystroke(cf, inpinfo, iccf);
		}

    	return IMKEY_ABSORB;
	} else if ((keysym == XK_Escape) && len ) {
		reset_keystroke(inpinfo, iccf);
		inpinfo->cch_publish.wch = (wchar_t)0;
		inpinfo->mcch_pgstate = MCCH_ONEPG;
		inpinfo->guimode &= ~(GUIMOD_SELKEYSPOT);

		return IMKEY_ABSORB;
	} else if (keyinfo->keystr_len == 1) {
 		int selkey_idx;
		char keycode, *s;
		wch_t wch;
                    
		inpinfo->cch_publish.wch = (wchar_t)0;
		keycode = key2code(keystr[0]);
		wch.wch = cf->header.keyname[(int)keycode].wch;
		selkey_idx = ((s = strchr(cf->header.selkey, keystr[0]))) ? 
                    		(int)(s - cf->header.selkey) : -1;
		iccf->keystroke[len] = keystr[0];
		iccf->keystroke[len+1] = '\0';
		len ++;
		
		inpinfo->s_keystroke[len].wch = wch.wch;
		inpinfo->s_keystroke[len+1].wch = (wchar_t)0;
		inpinfo->keystroke_len ++;
                    
		if (if_vowel(keysym) == 1){
			// Ne^'u la` phu. a^m thi` commit phu. a^m va` xoa' buffer.
			reset_keystroke(inpinfo, iccf);
			inpinfo->cch_publish.wch = (wchar_t)0;
			inpinfo->mcch_pgstate = MCCH_ONEPG;
			inpinfo->guimode &= ~(GUIMOD_SELKEYSPOT);

			return IMKEY_IGNORE;
		} else {
			// nguye^n a^m = vowel+ char la`m da^'u :-)
			// Ne^'u la` nguye^n a^m va` len=1 thi` commit composed char
			//  va` store char va`o buffer dde^? xu+? ly' tie^'p
			// Ne^'u 3 > len > 1 thi` commit backspace dde^? xoa' char cu~, 
			// commit composed char mo+'i, lu+u char va`o buffer.
			// Ne^'u len =3 thi` commit backspace, commit composed char, 
			// xoa' buffer.
                        
			if(match_keystroke(cf, inpinfo, iccf)){
				if(len==1){
					// commit char, keep buffer.
					commit_keystroke(cf, inpinfo, iccf);
					return (IMKEY_IGNORE);
 				} else if((len < max_len)&& (len > 1)){
					// commit backspace, commit char, keep buffer.
					commit_keystroke(cf, inpinfo, iccf);
                                    
					return (IMKEY_BACKSPACE);
				} else {
					// len=3, commit BackSpace, commit char, 
					// xoa' buffer
					commit_keystroke(cf, inpinfo, iccf);
					reset_keystroke(inpinfo, iccf);
 					inpinfo->cch_publish.wch = (wchar_t)0;
					inpinfo->mcch_pgstate = MCCH_ONEPG;
					inpinfo->guimode &= ~(GUIMOD_SELKEYSPOT);

					return (IMKEY_BACKSPACE);
				}
			} else {
				// Ne^'u nguye^n a^m nha^.n va`o kho^ng match thi` 
  				// commit keystroke
				// xoa' buffer lu+u keystroke ta.o buffer mo+'i.
				ticcf = iccf->keystroke[len-1];
				tchar = inpinfo->s_keystroke[len-1].wch;
				reset_keystroke(inpinfo, iccf);
				inpinfo->cch_publish.wch = (wchar_t)0;
				inpinfo->mcch_pgstate = MCCH_ONEPG;
				inpinfo->guimode &= ~(GUIMOD_SELKEYSPOT);
				// Then reassign the last un-match vowel keystroke 
				// for the new buffer
				iccf->keystroke[0] = ticcf;
				iccf->keystroke[1] = '\0';
				inpinfo->s_keystroke[0].wch = tchar;
				inpinfo->s_keystroke[1].wch = (wchar_t)0;
				inpinfo->keystroke_len =1;

				return IMKEY_IGNORE;
			}
		}
	}
	
	return IMKEY_IGNORE;    
}

static int 
gen_inp_show_keystroke(void *conf, simdinfo_t *simdinfo)
{
    gen_inp_conf_t *cf = (gen_inp_conf_t *)conf;
    int i, idx;
    wchar_t tmp;
    char *k, keystroke[INP_CODE_LENGTH+1];
    static wch_t keystroke_list[INP_CODE_LENGTH+1];
    
    if ((i = ccode_to_idx(&(simdinfo->cch_publish))) == -1)
        return False;
    
    if ((idx = cf->ichar[i]) == ICODE_IDX_NOTEXIST)
        return False;
    if (cf->header.icode_mode == ICODE_MODE1)
        codes2keys(&(cf->ic1[idx]), 1, keystroke, SELECT_KEY_LENGTH+1);
    else if (cf->header.icode_mode == ICODE_MODE2) {
        unsigned int klist[2];
        
        klist[0] = cf->ic1[idx];
        klist[1] = cf->ic2[idx];
        codes2keys(klist, 2, keystroke, SELECT_KEY_LENGTH+1);
    }
    for (i=0, k=keystroke; i<INP_CODE_LENGTH && *k; i++, k++) {
        idx = key2code(*k);
        if ((tmp = cf->header.keyname[idx].wch))
            keystroke_list[i].wch = tmp;
        else {
            keystroke_list[i].wch = (wchar_t)0;
            keystroke_list[i].s[0] = '?';
        }
    }
    keystroke_list[i].wch = (wchar_t)0;
    simdinfo->s_keystroke = keystroke_list;
    
    return (i) ? True : False;
}

/*----------------------------------------------------------------------------

  Definition of general input method module (template).
  
  ----------------------------------------------------------------------------*/

static char *gen_inp_valid_objname[] = { "*", NULL };

static char gen_inp_comments[] = N_(
                                    "This is the general input method module. It is the most generic\n"
                                    "module which can read the IM table (\".tab\" file) and perform\n"
                                    "the correct operations of the specific IM. It also can accept a\n"
                                    "lot of options from the \"xcinrc\" file to detailly control the\n"
                                    "behavior of the IM. We hope that this module could match most of\n"
                                    "your Chinese input requirements.\n\n"
                                    "This module is free software, as part of xcin system.\n");

#ifdef USE_DYNAMIC
module_t module_ptr = {
#else
    module_t gen_inp_module_ptr = {
#endif
        "gen_inp",					/* name */
        MODULE_VERSION,				/* version */
        gen_inp_comments,				/* comments */
        gen_inp_valid_objname,			/* valid_objname */
        MOD_CINPUT,					/* module_type */
        sizeof(gen_inp_conf_t),			/* conf_size */
        gen_inp_init,				/* init */
        gen_inp_xim_init,				/* xim_init */
        gen_inp_xim_end,				/* xim_end */
        gen_inp_keystroke,				/* keystroke */
        gen_inp_show_keystroke,			/* show_keystroke */
        NULL					/* terminate */
    };
    
