#ifndef _UNI_CHR_H
#define _UNI_CHR_H

int Uni2Char(unsigned short uni, unsigned char *out);
int Char2Uni(const unsigned char *rawstring, unsigned short *uni);
unsigned char Char2Upper(unsigned char c);

#endif