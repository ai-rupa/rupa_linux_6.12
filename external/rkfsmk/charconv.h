#ifndef _CHARCONV_H
#define _CHARCONV_H

#define DEFAULT_DOS_CODEPAGE "UTF-8"

int SetDosCodepage(const char* codepage);
int DosCharToPrintable(char **p, unsigned char c);

#endif
