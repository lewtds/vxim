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
    
    This program is apart of Xcin xcin@linux.org.tw, the maintainer 
    Tung-Han Hsieh: thhsieh@linux.org.tw
    
    Modified to support Vietnamese Input Method by Do Hoang Son
    dhson@thep.physik.uni-mainz.de
    
    For any question or suggestion, please mail to X-vim mailing-list:
    vietLUG@egroups.com, or the maintainer Do Hoang Son: 
    dhson@thep.physik.uni-maint.de


*/      


#include <string.h>
#include "module.h"

#define N_KEYS  94

static char keymap[128];
static char *ichmap = " 1234567890!@#$%^&*()abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ`-=[];',./\\~-+{}:\"<>?|";

static void
keymap_init(void)
{
    keymap['1'] = 1;
    keymap['2'] = 2;
    keymap['3'] = 3;
    keymap['4'] = 4;
    keymap['5'] = 5;
    keymap['6'] = 6;
    keymap['7'] = 7;
    keymap['8'] = 8;
    keymap['9'] = 9;
    keymap['0'] = 10;

    keymap['!'] = 11;
    keymap['@'] = 12;
    keymap['#'] = 13;
    keymap['$'] = 14;
    keymap['%'] = 15;
    keymap['^'] = 16;
    keymap['&'] = 17;
    keymap['*'] = 18;
    keymap['('] = 19;
    keymap[')'] = 20;

    keymap['a'] = 21;
    keymap['b'] = 22;
    keymap['c'] = 23;
    keymap['d'] = 24;
    keymap['e'] = 25;
    keymap['f'] = 26;
    keymap['g'] = 27;
    keymap['h'] = 28;
    keymap['i'] = 29;
    keymap['j'] = 30;
    keymap['k'] = 31;
    keymap['l'] = 32;
    keymap['m'] = 33;
    keymap['n'] = 34;
    keymap['o'] = 35;
    keymap['p'] = 36;
    keymap['q'] = 37;
    keymap['r'] = 38;
    keymap['s'] = 39;
    keymap['t'] = 40;
    keymap['u'] = 41;
    keymap['v'] = 42;
    keymap['w'] = 43;
    keymap['x'] = 44;
    keymap['y'] = 45;
    keymap['z'] = 46;

    keymap['A'] = 47;
    keymap['B'] = 48;
    keymap['C'] = 49;
    keymap['D'] = 50;
    keymap['E'] = 51;
    keymap['F'] = 52;
    keymap['G'] = 53;
    keymap['H'] = 54;
    keymap['I'] = 55;
    keymap['J'] = 56;
    keymap['K'] = 57;
    keymap['L'] = 58;
    keymap['M'] = 59;
    keymap['N'] = 60;
    keymap['O'] = 61;
    keymap['P'] = 62;
    keymap['Q'] = 63;
    keymap['R'] = 64;
    keymap['S'] = 65;
    keymap['T'] = 66;
    keymap['U'] = 67;
    keymap['V'] = 68;
    keymap['W'] = 69;
    keymap['X'] = 70;
    keymap['Y'] = 71;
    keymap['Z'] = 72;

    keymap['`'] = 73;
    keymap['-'] = 74;
    keymap['='] = 75;
    keymap['['] = 76;
    keymap[']'] = 77;
    keymap[';'] = 78;
    keymap['\''] = 79;
    keymap[','] = 80;
    keymap['.'] = 81;
    keymap['/'] = 82;
    keymap['\\'] = 83;

    keymap['~'] = 84;
    keymap['_'] = 85;
    keymap['+'] = 86;
    keymap['{'] = 87;
    keymap['}'] = 88;
    keymap[':'] = 89;
    keymap['\"'] = 90;
    keymap['<'] = 91;
    keymap['>'] = 92;
    keymap['?'] = 93;
    keymap['|'] = 94;
}


/*------------------------------------------------------------------------*/

int
key2code(int key)
{
    if (! keymap['1']) {
        keymap_init();
	}

    return (key < 128) ? keymap[key] : 0;
}

int
code2key(int code)
{
    return (code <= N_KEYS && code > 0) ? ichmap[code] : 0;
}

#define N_KEY_IN_CODE 4		/* Number of keys in code. */

int
keys2codes(unsigned int *klist, int klist_size, char *keystroke)
{
    int i, j;
    unsigned int k, *kk=klist;
    char *kc = keystroke;
    
    if (! keymap['1']) {
        keymap_init();
	}
    
    *kk = 0;
    for (i=0, j = 0; *kc; i++, kc++) {
        k = keymap[(int) *kc];
        *kk |= (k << (24 - (i - j*N_KEY_IN_CODE) * 8));
        
        if (i % N_KEY_IN_CODE == N_KEY_IN_CODE-1) {
            if (++j >= klist_size) {
                break;
			}

            kk++;
            *kk = 0;
        }
    }

    return j;
}

void
codes2keys(unsigned int *klist, int n_klist, char *keystroke, int keystroke_len)
{
    int i, j, n_ch=0, shift;
    unsigned int mask = 0xff, idx;    
    char *s;

    for (j = 0; j < n_klist; j++) {
        for (i = 0; i < N_KEY_IN_CODE; i++) {
            shift = 24 - i * 8;
            if (n_ch < (keystroke_len - 1)) {
                idx = (klist[j] & (mask << shift)) >> shift;
                keystroke[n_ch] = ichmap[idx];
                n_ch++;
            } else {
                break;
			}
        }
    }

    keystroke[n_ch] = '\0';
    
    if ((s = strchr(keystroke, ' '))) {
        *s = '\0';
	}
}

