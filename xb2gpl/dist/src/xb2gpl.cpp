//
// (C) 2011 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this software.
// All rights not explicity granted here are reserved by the
// author.
//
// You may redistribute this software provided the original
// archive is UNCHANGED and a link back to my web page,
// http://harmlesslion.com, is provided as the author's site.
// It is acceptable to link directly to a subpage at harmlesslion.com
// provided that page offers a URL for that purpose
//
// Source code, if available, is provided for educational purposes
// only. You are welcome to read it, learn from it, mock
// it, and hack it up - for your own use only.
//
// Please contact me before distributing derived works or
// ports so that we may work out terms. I don't mind people
// using my code but it's been outright stolen before. In all
// cases the code must maintain credit to the original author(s).
//
// Unless you have explicit written permission from me in advance,
// this code may never be used in any situation that changes these
// license terms. For instance, you may never include GPL code in
// this project because that will change all the code to be GPL.
// You may not remove these terms or any part of this comment
// block or text file from any derived work.
//
// -COMMERCIAL USE- Contact me first. I didn't make
// any money off it - why should you? ;) If you just learned
// something from this, then go ahead. If you just pinched
// a routine or two, let me know, I'll probably just ask
// for credit. If you want to derive a commercial tool
// or use large portions, we need to talk. ;)
//
// Commercial use means ANY distribution for payment, whether or
// not for profit.
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//

// xb2gpl.cpp : Defines the entry point for the console application.
// Let's seeif this can be made to work, then...
// just supporting as much as I actually use... :)

#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

FILE *fp;
unsigned char grom[8192];
int nBase, nOffset;
int nScratchPad;
int nLineOffset[65536];
int nLine;
bool bFirstFmt = true;
int fmtRow=0,fmtCol=0;
struct {
	char szName[32];
	int nAddress;
} vars[32];
int nVars;
int nBranchFixes[65536];		// by address - holds line number to target
int nAbsoluteFixes[65536];		// by address

// jump table for processing keywords
struct LINKS {
	char szName[32];
	void (*fctn)(char *);
};

void doRem(char *buf);
void doCall(char *buf);
void doFmt(char *buf);
void doGosub(char *buf);
void doIf(char *buf);
void doGoto(char *buf);
void doEnd(char *buf);
void doReturn(char *buf);
void doAorg(char *buf);
void doBss(char *buf);
void doMove(char *buf);
void doInit(char *buf);
void doCharset(char *buf);
void doChar(char *buf);
void doKey(char *buf);
void doLoad(char *buf);
void doPeek(char *buf);
void doClear(char *buf);
void doLet(char *buf);

struct LINKS keys[] = {
	{	"rem",		doRem		},
	{	"call",		doCall		},
	{	"display",	doFmt		},
	{	"gosub",	doGosub		},
	{	"if",		doIf		},
	{	"goto",		doGoto		},
	{	"stop",		doEnd		},
	{	"end",		doEnd		},
	{	"return",	doReturn	},
	{	"let",		doLet		},

	{	"*",		NULL		}
};

struct LINKS gpl[] = {
	{	"aorg",		doAorg		},
	{	"bss",		doBss		},
	{	"move",		doMove		},

	{	"*",		NULL		}
};

struct LINKS call[] = {
	{	"init",		doInit		},
	{	"charset",	doCharset	},
	{	"char",		doChar		},
	{	"key",		doKey		},
	{	"hchar",	doFmt		},
	{	"vchar",	doFmt		},
	{	"load",		doLoad		},
	{	"peek",		doPeek		},
	{	"clear",	doClear		},

	{	"*",		NULL		}
};

int getVar(char *szName) {
	int ret = -1;

	for (int i=0; i<nVars; i++) {
		if (_stricmp(vars[i].szName, szName) == 0) {
			ret = vars[i].nAddress;
			break;
		}
	}

	return ret;
}

// assumes 8-bit variables!
void addVar(char *szName, int nAddress) {
	int i;
	if (nAddress > 0x8300) nAddress-=0x8300;
	for (i=0; i<nVars; i++) {
		if (_stricmp(vars[i].szName, szName) == 0) {
			if (nAddress != -1) {
				// if a forced address was specified, warn if it's not the same!
				if (vars[i].nAddress != nAddress) {
					printf("* Warning - moving '%s' from >%04X to >%04X\n", szName, vars[i].nAddress, nAddress);
					vars[i].nAddress = nAddress;
				}
			}
			break;
		}
	}
	if (i >= nVars) {
		// not found, add it
		if (nAddress == -1) {
			vars[nVars].nAddress = nScratchPad++;
		} else {
			vars[nVars].nAddress = nAddress;
		}
		strcpy(vars[nVars].szName, szName);
		printf("* Mapping '%s' to >%04X\n", szName, vars[nVars].nAddress+0x8300);
		nVars++;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	printf("xb2gpl <input text file> <grom base address in hex> <output text file>\n");
	if (argc < 4) return 0;

	nBase=0;

	fp=fopen(argv[1], "r");
	if (NULL == fp) {
		printf("Can't open input file.\n");
		return -1;
	}
	if (1 != sscanf(argv[2], "%x", &nBase)) {
		printf("Can't parse address.\n");
		return -1;
	}

	nOffset=0;
	nScratchPad = 0;
	nVars=0;
	memset(nLineOffset, 0, sizeof(nLineOffset));
	memset(nBranchFixes, 0, sizeof(nBranchFixes));
	memset(nAbsoluteFixes, 0, sizeof(nAbsoluteFixes));

	while (!feof(fp)) {
		char buf[256];
		char tmp[256];
		int nCurrentOffset = nOffset;

		if (NULL == fgets(buf, 256, fp)) break;
		printf("*%s", buf);
		int nPos=0;
		if ((2 != sscanf(buf, "%d %s", &nLine, tmp)) || (nLine < 1) || (nLine > 65535)) {
			printf("  Invalid line number or no keyword?\n");
			continue;
		}
		nLineOffset[nLine] = nOffset;

		// find and process
		_strlwr(tmp);

		struct LINKS *pLink = keys;
		while (pLink->fctn != NULL) {
			if (strcmp(pLink->szName, tmp) == 0) {
				pLink->fctn(buf);
				break;
			} else {
				pLink++;
			}
		}
		if (pLink->fctn == NULL) {
			// check for LET, since it's optional
			if (strchr(buf, '=')) {
				doLet(buf);
			} else {
				printf("  No match found for command '%s'\n", tmp);
			}
		}

		if (nOffset > nCurrentOffset) {
			// dump hex
			for (int i1=nCurrentOffset; i1<nOffset; i1+=8) {
				printf("* ");
				for (int i2=0; (i2<8)&&(i1+i2<nOffset); i2++) {
					printf("%02X,", grom[i1+i2]);
				}
				printf("\n");
			}
			printf("\n");
		}
	}

	fclose(fp);
	printf("\n\n");

	// Time to do fixups
	printf("* Absolute fixups:\n");
	for (int i=0; i<65536; i++) {
		if (nAbsoluteFixes[i] != 0) {
			int nFix = nLineOffset[nAbsoluteFixes[i]]+nBase;
			printf("*  >%04X (>%04X): G@L%d = G@>%04X\n", i, i+nBase, nAbsoluteFixes[i], nFix);
			grom[i] = (nFix&0xff00)>>8;
			grom[i+1] = nFix&0xff;
		}
	}

	printf("\n* Relative fixups:\n");
	for (int i=0; i<65536; i++) {
		if (nBranchFixes[i] != 0) {
			int nFix = nLineOffset[nBranchFixes[i]]+nBase;
			printf("*  >%04X (>%04X): G@L%d = G@>%04X\n", i, i+nBase, nBranchFixes[i], nFix);
			grom[i] |= (nFix&0x1f00)>>8;
			grom[i+1] = nFix&0xff;
		}
	}

	fp=fopen(argv[3], "wb");
	fwrite(grom, 1, nOffset, fp);
	fclose(fp);

	return 0;
}

void unputstr(char *str) {
	// put the string back, hopefully it all fits!
	for (int i=strlen(str)-1; i>=0; i--) {
		ungetc(str[i], fp);
	}
}

void doRem(char *buf) {
	char tmp[256];

	// rem matters only if it's rem GPL
	_strlwr(buf);
	
	char *p = strstr(buf, "rem gpl ");
	if (NULL == p) return;

	// find out what it is
	p+=8;
	if (1 != sscanf(p, "%s", tmp)) {
		printf("  Can't parse cmd for REM GPL\n");
		return;
	}

	struct LINKS *pLink = gpl;
	while (pLink->fctn != NULL) {
		if (strcmp(pLink->szName, tmp) == 0) {
			pLink->fctn(buf);
			break;
		} else {
			pLink++;
		}
	}
	if (pLink->fctn == NULL) {
		printf("  No match found for GPL command '%s'\n", tmp);
	}
}

void doCall(char *buf) {
	char tmp[256];

	_strlwr(buf);
	
	char *p = strstr(buf, "call ");
	if (NULL == p) return;

	// find out what it is
	p+=5;
	
	char *pBracket = strchr(p, '(');
	if (NULL != pBracket) *pBracket='\0';

	if (1 != sscanf(p, "%s(", tmp)) {
		printf("  Can't parse cmd for CALL\n");
		return;
	}

	if (NULL != pBracket) *pBracket='(';

	struct LINKS *pLink = call;
	while (pLink->fctn != NULL) {
		if (strcmp(pLink->szName, tmp) == 0) {
			pLink->fctn(buf);
			break;
		} else {
			pLink++;
		}
	}
	if (pLink->fctn == NULL) {
		printf("  No match found for CALL '%s'\n", tmp);
	}
}

void doRowCol(int nRow, int nCol) {
	if (bFirstFmt) {
		// set absolute positions rather than assuming
		printf("  %04X:   COL %d\n", nOffset+nBase, nCol);
		grom[nOffset++] = 0xff;
		grom[nOffset++] = nCol-1;
		printf("  %04X:   ROW %d\n", nOffset+nBase, nRow);
		grom[nOffset++] = 0xfe;
		grom[nOffset++] = nRow-1;
		fmtRow=nRow;
		fmtCol=nCol;
		bFirstFmt = false;
		return;
	}
	if (nCol != fmtCol) {
		int nDiff=nCol-fmtCol;
		if (nDiff < 0) {
			nDiff+=32;
			fmtRow++;
		}
		printf("  %04X:   COL+ %d\n", nOffset+nBase, nDiff);
		grom[nOffset++] = 0x80 + nDiff - 1;
		fmtCol = nCol;
	}
	if (nRow != fmtRow) {
		int nDiff=nRow-fmtRow;
		if (nDiff < 0) nDiff+=24;
		printf("  %04X:   ROW+ %d\n", nOffset+nBase, nDiff);
		grom[nOffset++] = 0xa0 + nDiff - 1;
		fmtRow = nRow;
	}
}

void doDisplay(char *buf) {
	int nRow, nCol;
	char *p = strchr(buf, '(');
	if (NULL == p) {
		printf("  Can't find open parenthesis in DISPLAY AT command\n");
		return;
	}
	if (2 != sscanf(p, "(%d,%d)", &nRow, &nCol)) {
		printf("  Can't parse coordinates in DISPLAY AT command\n");
		return;
	}
	nCol+=2;
	doRowCol(nRow, nCol);
	p=strchr(buf, '\"');
	if (NULL == p) {
		printf("  Can't find first quote in DISPLAY AT command\n");
		return;
	}
	// parse the rest of the string, removing double-quotes
	char *q = ++p;
	while (*q) {
		if (*q == '\"') {
			if (*(q+1)=='\"') {
				memmove(q, q+1, strlen(q));
			} else {
				break;
			}
		}
		q++;
	}
	*q='\0';
	printf("  %04X:   HTEX %d \"%s\"\n", nOffset+nBase, q-p, p);
	grom[nOffset++] = 0x00 + (q-p) - 1;
	memcpy(&grom[nOffset], p, q-p);
	nOffset+=q-p;
	fmtCol+=q-p;
	while (fmtCol > 32) {
		fmtCol-=32;
		fmtRow++;
		while (fmtRow > 24) {
			fmtRow-=24;
		}
	}
}

int doRepChar(char *buf, int nCode) {
	int nRow, nCol, nChar, nCnt;
	
	char *p = strchr(buf, '(');
	if (NULL == p) {
		printf("  Can't find open parenthesis in RepChar command\n");
		return 0;
	}
	nCnt=1;
	if (4 != sscanf(p, "(%d,%d,%d,%d)", &nRow, &nCol, &nChar, &nCnt)) {
		if (3 != sscanf(p, "(%d,%d,%d)", &nRow, &nCol, &nChar)) {
			printf("  Can't parse coordinates in RepChar command\n");
			return 0;
		}
	}
	doRowCol(nRow, nCol);
	printf("  %04X:   %cCHAR %d,%d\n", nOffset+nBase, nCode==0x40 ? 'H':'V', nChar, nCnt);
	grom[nOffset++]=nCode + nCnt - 1;
	grom[nOffset++]=nChar;
	return nCnt;
}

void doHchar(char *buf) {
	fmtCol+=doRepChar(buf, 0x40);
	while (fmtCol > 32) {
		fmtCol-=32;
		fmtRow++;
		while (fmtRow > 24) {
			fmtRow-=24;
		}
	}
}
void doVchar(char *buf) {
	fmtRow+=doRepChar(buf, 0x60);
	while (fmtRow > 24) {
		fmtRow-=24;
		fmtCol++;
		while (fmtCol > 32) {
			fmtCol-=32;
		}
	}
}

void doFmt(char *buf) {
	char orig[256];
	// generic block that handles DISPLAY AT, CALL HCHAR and CALL VCHAR in blocks
	printf("  %04X: FMT\n", nOffset+nBase);
	bFirstFmt = true;
	grom[nOffset++] = 0x08;
	fmtRow=1;
	fmtCol=1;
	while (!feof(fp)) {
		strcpy(orig, buf);		// in case it's display at, we don't want to ruin the string
		_strlwr(buf);
		char *p=strstr(buf, "display at");
		if (NULL != p) {
			doDisplay(orig);
		} else {
			p=strstr(buf, "call hchar");
			if (NULL != p) {
				doHchar(buf);
			} else {
				p=strstr(buf, "call vchar");
				if (NULL != p) {
					doVchar(buf);
				} else {
					unputstr(orig);
					break;
				}
			}
		}
		if (NULL == fgets(buf, 256, fp)) break;
		printf("*%s", buf);
	}
	printf("  %04X: ENDFMT\n", nOffset+nBase);
	grom[nOffset++] = 0xfb;
}

void doGosub(char *buf) {
	_strlwr(buf);
	char *p = strstr(buf, "gosub");
	p+=6;
	int nLine = atoi(p);
	if (nLine == 0) {
		printf("  Can't parse line number for GOSUB\n");
		return;
	}
	printf("  %04X: CALL G@L%d\n", nOffset+nBase, nLine);
	grom[nOffset++] = 0x06;
	nAbsoluteFixes[nOffset]=nLine;
	nOffset+=2;
}

void doGoto(char *buf) {
	_strlwr(buf);
	char *p = strstr(buf, "goto");
	p+=5;
	int nLine = atoi(p);
	if (nLine == 0) {
		printf("  Can't parse line number for GOTO\n");
		return;
	}
	printf("  %04X: B G@L%d\n", nOffset+nBase, nLine);
	grom[nOffset++] = 0x05;
	nAbsoluteFixes[nOffset]=nLine;
	nOffset+=2;
}

void doEnd(char *buf) {
	printf("  %04X: EXIT\n", nOffset+nBase);
	grom[nOffset++] = 0x0b;
}

void doReturn(char *buf) {
	printf("  %04X: RTN\n", nOffset+nBase);
	grom[nOffset++] = 0x00;
}

int ParseValue(char *p) {
	int nVal;

	if (*p=='>') {
		if (1 != sscanf(p+1, "%x", &nVal)) {
			printf("  Can't parse hex value!\n");
			return 0;
		}
	} else {
		if (1 != sscanf(p, "%d", &nVal)) {
			printf("  Can't parse decimal value!\n");
			return 0;
		}
	}

	if ((nVal > 0xffff)||(nVal < 0)) {
		printf(" Value out of range!\n");
	}

	return nVal;
}

void doAorg(char *buf) {
	_strlwr(buf);
	char *p = strstr(buf, "aorg");
	if (NULL == p) {
		printf("  Can't find AORG for REM GPL\n");
		return;
	}
	p+=5;
	nBase = ParseValue(p);
	printf("        AORG >%04X\n", nBase);
}

void doBss(char *buf) {
	int nSkip;
	_strlwr(buf);
	char *p = strstr(buf, "bss");
	if (NULL == p) {
		printf("  Can't find BSS for REM GPL\n");
		return;
	}
	p+=4;
	nSkip = ParseValue(p);
	nScratchPad+=nSkip;
	printf("* Skipping %d bytes in scratchpad, reserving >%04X to >%04X\n", nSkip, 0x8300+nScratchPad-nSkip, 0x8300+nScratchPad);
}

void doInit(char *buf) {
	// does nothing here
}

void doCharset(char *buf) {
	unsigned char dat[] = {
		0xbf, 0x4a, 0x09, 0x00,
		0x06, 0x00, 0x18
	};

	// hard coded data for this one
	printf("  %04X: DST @>834A,>0900\n", nOffset+nBase);
	printf("  %04X: CALL GROM@>0018\n", nOffset+nBase+4);
	memcpy(&grom[nOffset], dat, sizeof(dat));
	nOffset+=sizeof(dat);
}

void doChar(char *buf) {
	unsigned char data[2048];
	int nCnt = 0;
	int nLastChar = 0;
	int nFirstChar = 0;

	while (!feof(fp)) {
		_strlwr(buf);
		char *p = strchr(buf, '(');
		if (NULL == p) {
			printf("  Can't find open bracket for CALL CHAR\n");
		} else {
			int nChar;
			char str[256];
			if (2 != sscanf(p+1, "%d,%s", &nChar, str)) {
				printf("  Can't parse CALL CHAR command\n");
			} else {
				if ((nLastChar == 0) || (nChar == nLastChar+1)) {
					// we can continue!
					if (nFirstChar == 0) nFirstChar = nChar;
					p=str;
					if (*p == '\"') p++;
					while ((*p)&&(*p != '\"')) {
						int nByte=0;
						if (1 != sscanf(p, "%1X", &nByte)) {
							printf("  Can't parse CALL CHAR command - bad hex byte\n");
						} else {
							nByte<<=4;
							p++;
							if ((*p)&&(*p != '\"')) {
								int tmp;
								if (1 == sscanf(p, "%1X", &tmp)) {
									nByte|=tmp;
								}
								p++;
							}
							data[nCnt++]=nByte;
						}
					}
					while (nCnt%8) {
						data[nCnt++]=0;
					}
					nLastChar=nFirstChar+(nCnt/8)-1;
				} else {
					unputstr(buf);
					break;
				}
			}
			if (NULL == fgets(buf, 256, fp)) break;
			printf("*%s", buf);
			char szTmp[256];
			strcpy(szTmp, buf);
			_strlwr(szTmp);
			if (NULL == strstr(szTmp, "call char")) {
				unputstr(buf);
				break;
			}
			_strlwr(buf);
		}
	}

	// we've got all we're getting!
	int nVDPAddress = 0x0800 + (nFirstChar*8);
	int nSrc = nBase+nOffset+10;
	printf("  %04X: MOVE >%04X TO VDP@>%04X FROM GROM@>%04X\n", nOffset+nBase, nCnt, nVDPAddress, nSrc);
	grom[nOffset++] = 0x31;
	grom[nOffset++] = (nCnt>>8)&0xff;
	grom[nOffset++] = nCnt&0xff;
	grom[nOffset++] = 0xa0+((nVDPAddress>>8)&0xff);
	grom[nOffset++] = nVDPAddress&0xff;
	grom[nOffset++] = (nSrc>>8)&0xff;
	grom[nOffset++] = nSrc&0xff;
	int nJump = nOffset+3+nCnt+nBase;
	printf("  %04X: B GROM@>%04X\n", nOffset+nBase, nJump);
	grom[nOffset++] = 0x05;
	grom[nOffset++] = (nJump>>8)&0xff;
	grom[nOffset++] = nJump&0xff;
	for (int idx=0; idx<nCnt; idx+=8) {
		printf("  %04X: BYTE ", nOffset+nBase+idx);
		for (int idx2=0; (idx2<8)&&(idx+idx2<nCnt); idx2++) {
			printf(">%02X,", data[idx+idx2]);
		}
		printf("\n");
	}
	memcpy(&grom[nOffset], data, nCnt);
	nOffset+=nCnt;
}

void doKey(char *buf) { 
	int nMode;
	char var1[256], var2[256];

	_strlwr(buf);
	char *p = strchr(buf, '(');
	if (NULL == p) {
		printf("  Can't parse CALL KEY\n");
		return;
	}
	
	if (2 != sscanf(p+1, "%d,%s", &nMode, var1)) {
		printf("  Can't parse arguments for CALL KEY\n");
		return;
	}

	p=strchr(var1,',');
	if (NULL == p) {
		printf("  Can't parse S argument for CALL KEY\n");
		return;
	}
	strcpy(var2, p+1);
	*p='\0';
	p=strchr(var2,')');
	if (NULL != p) *p='\0';

	printf("  %04X: ST @>8374,>%02X\n", nOffset+nBase, nMode);
	grom[nOffset++] = 0xbe;
	grom[nOffset++] = 0x74;
	grom[nOffset++] = nMode&0xff;

	printf("  %04X: SCAN\n", nOffset+nBase);
	grom[nOffset++] = 0x03;

	// check if the next line tests 'S' - S is otherwise not available for testing!
	char tmp[256];
	if (NULL != fgets(tmp, 256, fp)) {
		char orig[256];
		printf("*%s", tmp);
		strcpy(orig, tmp);
		_strlwr(tmp);
		_strlwr(var2);
		if ((strstr(tmp, "if ") == NULL) || (strstr(tmp, var2) == NULL)) {
			printf("* (Warning: Status variable not available later in program)\n");
			unputstr(orig);
		} else {
			char *p = strstr(tmp, var2);
			p+=strlen(var2);
			while (*p == ' ') p++;
			char sign=*p;
			while (strchr("=<>",*p)) p++;
			int nNum=0;
			if (1 != sscanf(p, "%d", &nNum)) {
				printf("  Only simple equality tests supported for testing KEY status.\n");
			}
			int nLine=0;
			p=strstr(p, "then ");
			if (NULL == p) {
				printf("  Can't parse IF test for CALL KEY status test\n");
			} else {
				if (1 != sscanf(p+5, "%d", &nLine)) {
					printf("  Can't parse line number after THEN for CALL KEY status test\n");
				}
			}
			if (sign != '=') {
				nNum=!nNum;
			}
			if (nNum) {
				// BS
				printf("  %04X: BS G@L%d\n", nOffset+nBase, nLine);
				nBranchFixes[nOffset] = nLine;
				grom[nOffset++] = 0x60;
				nOffset++;
			} else {
				printf("  %04X: BR G@L%d\n", nOffset+nBase, nLine);
				nBranchFixes[nOffset] = nLine;
				grom[nOffset++] = 0x40;
				nOffset++;
			}
		}
	}

	addVar(var1, 0x8375);
}

// only for VDP and CPU addresses! Only MOVE can do GROM.
void WriteGplAddress(int ad, bool bVdp, bool bIndirect, bool bIndexed) {
	// writes a formatted GPL address, note that certain formats in MOVE are written directly, not through here!
	// for indexed, you have to write the index after this function call
	if (ad < 0x0fff) {
		unsigned char c = 0x80;
		if (bVdp) c|=0x20;
		if (bIndirect) c|=0x10;
		if (bIndexed) c|=0x40;
		c|=(ad&0x0f00)>>8;
		grom[nOffset++]=c;
		grom[nOffset++]=ad&0xff;
		return;
	}
	if ((!bVdp)&&(ad>=0x8300)&&(ad<=0x837f)) {
		grom[nOffset++] = ad&0xff;
		return;
	}

	unsigned char c = 0x8f;
	if (bVdp) c|=0x20;
	if (bIndirect) c|=0x10;
	if (bIndexed) c|=0x40;
	grom[nOffset++]=c;
	grom[nOffset++]=(ad&0xff00)>>8;
	grom[nOffset++]=ad&0xff;
	return;
}

void doLoad(char *buf) {
	// load data to a CPU memory address -- may be more than one target byte
	_strlwr(buf);
	char *p=strchr(buf,'(');
	if (NULL == p) {
		printf("  Can't find arguments for CALL LOAD\n");
		return;
	}
	int ad;
	if (1 != sscanf(p+1, "%d", &ad)) {
		printf("  Can't parse address for CALL LOAD\n");
		return;
	}
	if (ad < 0) ad+=65536;
	while (NULL != (p=strchr(p+1, ','))) {
		p++;
		// number or variable
		int val;
		if (1 == sscanf(p, "%d", &val)) {
			// poke a value
			printf("  %04X: ST @>%04X,>%02X\n", nOffset+nBase, ad, val);
			grom[nOffset++] = 0xbe;
			WriteGplAddress(ad, false, false, false);
			grom[nOffset++] = val;
		} else {
			// probably a variable then
			char *pNew=strchr(p,',');
			char nOld;
			if (NULL == pNew) pNew=strchr(p,')');
			if (NULL == pNew) {
				printf("  Can't find end of arguments in CALL LOAD\n");
				return;
			}
			nOld=*pNew;
			*pNew='\0';
			addVar(p, -1);
			val = getVar(p);
			*pNew=nOld;
			if (-1 == val) {
				printf("  Can't get variable '%s' for CALL LOAD\n");
				return;
			}
			printf("  %04X: ST @>%04X,@>%04X\n", nOffset+nBase, ad, val+0x8300);
			grom[nOffset++] = 0xbc;
			WriteGplAddress(ad, false, false, false);
			WriteGplAddress(val+0x8300, false, false, false);
		}
		ad++;
	}
}

void doPeek(char *buf) {
	// load data from a CPU memory address -- may be more than one target byte
	// very similar to load! Just the other way around!
	_strlwr(buf);
	char *p=strchr(buf,'(');
	if (NULL == p) {
		printf("  Can't find arguments for CALL PEEK\n");
		return;
	}
	int ad;
	if (1 != sscanf(p+1, "%d", &ad)) {
		printf("  Can't parse address for CALL PEEK\n");
		return;
	}
	if (ad < 0) ad+=65536;
	while (NULL != (p=strchr(p+1, ','))) {
		p++;
		// variable only!
		int val;
		char nOld;
		char *pNew=strchr(p,',');
		if (NULL == pNew) pNew=strchr(p,')');
		if (NULL == pNew) {
			printf("  Can't find end of arguments in CALL PEEK\n");
			return;
		}
		nOld=*pNew;
		*pNew='\0';
		addVar(p, -1);
		val = getVar(p);
		*pNew=nOld;
		if (-1 == val) {
			printf("  Can't get variable '%s' for CALL PEEK\n");
			return;
		}
		printf("  %04X: ST @>%04X,@>%04X\n", nOffset+nBase, val+0x8300, ad);
		grom[nOffset++] = 0xbc;
		WriteGplAddress(val+0x8300, false, false, false);
		WriteGplAddress(ad, false, false, false);
		ad++;
	}
}

void doClear(char *buf) {
	printf("  %04X: ALL >20\n", nOffset+nBase);
	grom[nOffset++] = 0x07;
	grom[nOffset++] = 0x20;
}

// numerics only!
// supports literal value, variable, "NOT" variable, and variable +/- variable or literal
// no order of operations though! Not tested on complex functions
void doLet(char *buf) {
	char var2[256];
	int var1Ad, var2Ad;
	int nFunction = 0;		// -1=not set, 0=store, 1=add, 2=subtract, 3=NOT

	_strlwr(buf);
	char *p=strchr(buf,'=');
	if (NULL == p) {
		printf("  Can't find equals sign in LET\n");
		return;
	}
	// work backwards to find the assigned variable
	char *v=p-1;
	while ((v>buf)&&(isspace(*v))) v--;	// whitespace (shouldn't be any though)
	while ((v>buf)&&(!isspace(*v))) v--;	// name, hopefully!
	v++;
	*p='\0';
	addVar(v, -1);
	var1Ad=getVar(v);
	if (-1 == var1Ad) {
		printf("  Can't locate '%s' in LET\n", v);
		return;
	}
	// work out the assignment
	p++;

	// parse till end of string
	while (*p != '\0') {
		while ((*p != '\0')&&(isspace(*p))) p++;
		if (*p == '\0') break;

		if (isdigit(*p)) {
			if (nFunction == -1) {
				printf("  Syntax error - looking for operation\n");
				return;
			}
			if (1 != sscanf(p, "%d", &var2Ad)) {
				printf("  Error parsing number\n");
				return;
			}
			if (var2Ad > 255) {
				printf("  Number exceeds 8 bit limit\n");
				return;
			}
			switch (nFunction) {
				case 3:	// not (dummy, do it yourself!)
					var2Ad = (~var2Ad) & 0xff;
					// fall through
				case 0:	// store
					printf("  %04X: ST @>%04X,>%02X\n", nOffset+nBase, var1Ad+0x8300, var2Ad);
					grom[nOffset++] = 0xbe;
					WriteGplAddress(var1Ad+0x8300, false, false, false);
					grom[nOffset++] = var2Ad;
					break;

				case 1:	// add
					printf("  %04X: ADD @>%04X,>%02X\n", nOffset+nBase, var1Ad+0x8300, var2Ad);
					grom[nOffset++] = 0xA2;
					WriteGplAddress(var1Ad+0x8300, false, false, false);
					grom[nOffset++] = var2Ad;
					break;

				case 2:	// subtract
					printf("  %04X: SUB @>%04X,>%02X\n", nOffset+nBase, var1Ad+0x8300, var2Ad);
					grom[nOffset++] = 0xA6;
					WriteGplAddress(var1Ad+0x8300, false, false, false);
					grom[nOffset++] = var2Ad;
					break;
			}
			nFunction=-1;
			while (isdigit(*p)) p++;
		} else {
			// symbol or string
			if (nFunction == -1) {
				// want a symbol - NOT is not legal in this state (can't say B + NOT A)
				switch (*p) {
					case '+': nFunction = 1; break;
					case '-': nFunction = 2; break;
					default:
						printf("  Unknown symbol\n");
						return;
				}
				p++;
			} else {
				// variable (or 'NOT')
				int x=0;
				while (isalnum(*p)) {
					var2[x++]=*(p++);
				}
				var2[x]='\0';
				_strlwr(var2);
				if (0 == strcmp(var2, "not")) {
					if (nFunction != 0) {
						printf("  NOT is only valid as the only operation on the line.\n");
						return;
					}
					nFunction = 3;
				} else {
					addVar(var2, -1);
					var2Ad = getVar(var2);
					if (-1 == var2Ad) {
						printf("  Can't get variable '%s'\n", var2);
						return;
					}
					switch (nFunction) {
						case 0:	// store
							if (var1Ad != var2Ad) {
								printf("  %04X: ST @>%04X,@>%04X\n", nOffset+nBase, var1Ad+0x8300, var2Ad+0x8300);
								grom[nOffset++] = 0xbc;
								WriteGplAddress(var1Ad+0x8300, false, false, false);
								WriteGplAddress(var2Ad+0x8300, false, false, false);
							}
							break;

						case 1:	// add
							printf("  %04X: ADD @>%04X,@>%04X\n", nOffset+nBase, var1Ad+0x8300, var2Ad+0x8300);
							grom[nOffset++] = 0xA0;
							WriteGplAddress(var1Ad+0x8300, false, false, false);
							WriteGplAddress(var2Ad+0x8300, false, false, false);
							break;

						case 2:	// subtract
							printf("  %04X: SUB @>%04X,@>%04X\n", nOffset+nBase, var1Ad+0x8300, var2Ad+0x8300);
							grom[nOffset++] = 0xA4;
							WriteGplAddress(var1Ad+0x8300, false, false, false);
							WriteGplAddress(var2Ad+0x8300, false, false, false);
							break;

						case 3:	// not
							if (var1Ad != var2Ad) {
								printf("  %04X: ST @>%04X,@>%04X\n", nOffset+nBase, var1Ad+0x8300, var2Ad+0x8300);
								grom[nOffset++] = 0xbc;
								WriteGplAddress(var1Ad+0x8300, false, false, false);
								WriteGplAddress(var2Ad+0x8300, false, false, false);
							}
							printf("  %04X: INV @>%04X\n", nOffset+nBase, var1Ad+0x8300);
							grom[nOffset++] = 0x84;
							WriteGplAddress(var1Ad+0x8300, false, false, false);
							break;
					}
					nFunction=-1;
				}
			}
		}
	}
}

// only supports IF variable (>/</=/<>) number/variable THEN line_number
// note no >= or <=
// unsigned 8-bit compares only
void doIf(char *buf) {
	char var[256];
	int nVar1, nVar2, nLine, nSign;
	bool isVar=false;
	_strlwr(buf);

	char *p = strstr(buf, "if ");
	if (NULL == p) {
		printf("  Can't find IF clause!\n");
		return;
	}

	p+=3;
	while ((*p)&&(isspace(*p))) p++;
	char *t=p;
	char nOld;
	while (isalnum(*t)) t++;
	nOld=*t;
	*t='\0';
	strcpy(var, p);
	addVar(var, -1);
	nVar1=getVar(var);
	if (-1 == nVar1) {
		printf("  Can't get variable '%s'\n", var);
		return;
	}
	*t=nOld;
	p=t;

	while ((*p)&&(isspace(*p))) p++;
	switch (*p) {
		case '>': nSign='>'; break;
		case '<': if (*(p+1) == '>') { p++; nSign='!'; } else { nSign='<'; } break;
		case '=': nSign='='; break;
		default: printf("  Bad comparison operator\n"); return;
	}
	p++;

	while ((*p)&&(isspace(*p))) p++;
	if (1 != sscanf(p, "%d", &nVar2)) {
		// variable?
		t=p;
		while (isalnum(*t)) t++;
		nOld=*t;
		*t='\0';
		strcpy(var, p);
		addVar(var,-1);
		nVar2=getVar(var);
		if (-1 == nVar2) {
			printf("  Can't get variable '%s'\n", var);
			return;
		}
		*t=nOld;
		p=t;
		isVar=true;
	}

	p=strstr(buf, " then ");
	if (NULL == p) {
		printf("  Can't find THEN clause\n");
		return;
	}
	p+=6;
	if (1 != sscanf(p, "%d", &nLine)) {
		printf("  Can't parse line number\n");
		return;
	}

	if ((nSign=='=')||(nSign=='!')) {
		if (isVar) {
			printf("  %04X: CEQ @>%04X,@>%04X\n", nOffset+nBase, nVar1+0x8300, nVar2+0x8300);
			grom[nOffset++] = 0xd4;
			WriteGplAddress(nVar1+0x8300, false, false, false);
			WriteGplAddress(nVar2+0x8300, false, false, false);
		} else {
			printf("  %04X: CEQ @>%04X,>%02X\n", nOffset+nBase, nVar1+0x8300, nVar2);
			grom[nOffset++] = 0xd6;
			WriteGplAddress(nVar1+0x8300, false, false, false);
			grom[nOffset++] = nVar2&0xff;
		}
		if (nSign=='=') {
			printf("  %04X: BS G@L%d\n", nOffset+nBase, nLine);
			nBranchFixes[nOffset] = nLine;
			grom[nOffset++] = 0x60;
			nOffset++;
		} else {
			printf("  %04X: BR G@L%d\n", nOffset+nBase, nLine);
			nBranchFixes[nOffset] = nLine;
			grom[nOffset++] = 0x40;
			nOffset++;
		}
	} else if (nSign == '>') {
		// destination is higher than source, destination comes first!
		if (isVar) {
			printf("  %04X: CH @>%04X,@>%04X\n", nOffset+nBase, nVar1+0x8300, nVar2+0x8300);
			grom[nOffset++] = 0xc4;
			WriteGplAddress(nVar1+0x8300, false, false, false);
			WriteGplAddress(nVar2+0x83002, false, false, false);
		} else {
			printf("  %04X: CH @>%04X,>%02X\n", nOffset+nBase, nVar1+0x8300, nVar2);
			grom[nOffset++] = 0xc6;
			WriteGplAddress(nVar1+0x8300, false, false, false);
			grom[nOffset++] = nVar2&0xff;
		}
		printf("  %04X: BS G@L%d\n", nOffset+nBase, nLine);
		nBranchFixes[nOffset] = nLine;
		grom[nOffset++] = 0x60;
		nOffset++;
	} else {
		// less than, then!
		if (isVar) {
			printf("  %04X: CHE @>%04X,@>%04X\n", nOffset+nBase, nVar1+0x8300, nVar2+0x8300);
			grom[nOffset++] = 0xc8;
			WriteGplAddress(nVar1+0x8300, false, false, false);
			WriteGplAddress(nVar2+0x8300, false, false, false);
		} else {
			printf("  %04X: CHE @>%04X,>%02X\n", nOffset+nBase, nVar1+0x8300, nVar2);
			grom[nOffset++] = 0xca;
			WriteGplAddress(nVar1+0x8300, false, false, false);
			grom[nOffset++] = nVar2&0xff;
		}
		printf("  %04X: BR G@L%d\n", nOffset+nBase, nLine);
		nBranchFixes[nOffset] = nLine;
		grom[nOffset++] = 0x40;
		nOffset++;
	}
}

void ParseGplAddress(char *p) {
	int nVal;
	// parse and write the value for the number
	// note that GROM values are /only/ valid in MOVEs!
	// anywhere else they will be misinterpreted.
	if (*p == 'g') {
		// handle GROM (no indexed mode!)
		p++;
		if (*p != '@') {
			printf("  Can't find GROM address\n");
			return;
		}
		p++;
		if (*p == '>') {
			// read hex
			p++;
			if (1 != sscanf(p, "%x", &nVal)) {
				printf("  Failed to parse hexadecimal value\n");
				return;
			}
		} else {
			if (1 != sscanf(p, "%d", &nVal)) {
				printf("  Failed to parse value\n");
				return;
			}
		}
		printf("G@>%04X", nVal);
		grom[nOffset++] = (nVal&0xff00)>>8;
		grom[nOffset++] = nVal&0xff;
	} else if (*p == 'r') {
		// handle VDP register
		p++;
		if (*p != '@') {
			printf("  Can't find VDP Register\n");
			return;
		}
		p++;
		if (*p == '>') {
			// read hex
			p++;
			if (1 != sscanf(p, "%x", &nVal)) {
				printf("  Failed to parse hexadecimal value\n");
				return;
			}
		} else {
			if (1 != sscanf(p, "%d", &nVal)) {
				printf("  Failed to parse value\n");
				return;
			}
		}
		if ((nVal < 0) || (nVal > 7)) {
			printf("  VDP Register out of range!\n");
			// but do it anyway
		}
		printf("R@>%02X", nVal&0xff);
		grom[nOffset++] = nVal&0xff;
	} else {
		bool bVdp=false;
		bool bIndirect=false;
		bool bIndexed=false;

		// everyone else is normal
		if (*p == 'v') {
			bVdp=true;
			p++;
			printf("V");
		}
		if (*p == '*') {
			bIndirect = true;
			printf("*");
		} else if (*p != '@') {
			printf("  Address format (*/@) not found!\n");
			return;
		} else {
			printf("@");
		}

		p++;
		char *t=p;
		while ((*t)&&(!isspace(*t))) {
			if (*t=='(') {
				bIndexed=true;
				break;
			}
			t++;
		}
		if (*p == '>') {
			// read hex
			p++;
			if (1 != sscanf(p, "%x", &nVal)) {
				printf("  Failed to parse hexadecimal value\n");
				return;
			}
		} else {
			if (1 != sscanf(p, "%d", &nVal)) {
				printf("  Failed to parse value\n");
				return;
			}
		}
		printf(">%04X", nVal);
		WriteGplAddress(nVal, bVdp, bIndirect, bIndexed);
		if (bIndexed) {
			p=strchr(p, '(');
			if (NULL == p) {
				printf("  Can't find indexed clause\n");
				return;
			}
			p++;
			if (*p != '@') {
				printf("  Only CPU scratchpad is legal for indexed\n");
				return;
			}
			p++;
			if (*p == '>') {
				// read hex
				p++;
				if (1 != sscanf(p, "%x", &nVal)) {
					printf("  Failed to parse hexadecimal value\n");
					return;
				}
			} else {
				if (1 != sscanf(p, "%d", &nVal)) {
					printf("  Failed to parse value\n");
					return;
				}
			}
			if ((nVal < 0x8300) || (nVal > 0x837f)) {
				printf("  Index must be >8300->837f\n");
				return;
			}
			printf("(@>%04X)", nVal);
			WriteGplAddress(nVal, false, false, false);
		}
	}
}

// GPL - this is the only way to write to GROM, sadly!
void doMove(char *buf) {
	unsigned char c = 0x35;		// base MOVE command for CPU to CPU with immediate count
	_strlwr(buf);
	char *p = strstr(buf, "gpl move ");
	if (NULL == p) {
		printf("  Can't find GPL MOVE string\n");
		return;
	}
	p+=9;
	while ((*p)&&(isspace(*p))) p++;

	// we may alter this later!
	int nCmdOffset = nOffset;
	grom[nOffset++] = c;
	printf("  %04X: MOVE ", nOffset+nBase);

	// count (can be anything but GROM?)
	int nCount;
	if ((*p == 'v') || (*p == '@')) {
		grom[nCmdOffset]&=~0x01;	// not immediate
		ParseGplAddress(p);
	} else {
		// read immediate value
		if (*p == '>') {
			// read hex
			p++;
			if (1 != sscanf(p, "%x", &nCount)) {
				printf("  Failed to parse hexadecimal count\n");
				return;
			}
		} else {
			if (1 != sscanf(p, "%d", &nCount)) {
				printf("  Failed to parse count\n");
				return;
			}
		}
		printf(">%04X", nCount);
		grom[nOffset++]=(nCount&0xff00)>>8;
		grom[nOffset++]=nCount&0xff;
	}

	// destination
	p=strstr(p, " to ");
	if (NULL == p) {
		printf("  Can't find TO clause\n");
		return;
	}
	p+=4;
	while ((*p)&&(isspace(*p))) p++;
	if (*p == 'g') {
		// GROM destination
		grom[nCmdOffset]&=~0x10;
	} else if (*p == 'r') {
		// VDP register destination
		grom[nCmdOffset]|=0x08;
	}
	printf(" TO ");
	ParseGplAddress(p);

	// source
	p=strstr(p, " from ");
	if (NULL == p) {
		printf("  Can't find FROM clause\n");
		return;
	}
	p+=6;
	while ((*p)&&(isspace(*p))) p++;

	if (*p == 'g') {
// I don't want to figure out indexed GROM right now
//		if (NULL != strchr(p, '(')) {
//			// indexed GROM source
//			grom[nCmdOffset]|=0x02;
//		} else {
			// just GROM
			grom[nCmdOffset]&=~0x04;
//		}
	}
	printf(" FROM ");
	ParseGplAddress(p);
	printf("\n");
}

