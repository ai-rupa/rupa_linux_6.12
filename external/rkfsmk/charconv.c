#include "charconv.h"
#include <iconv.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static iconv_t iconv_init_codepage(const char* codepage)
{
    iconv_t result;
    result = iconv_open(nl_langinfo(CODESET), codepage);
    if (result == (iconv_t) - 1)
        perror(codepage);
    return result;
}

static iconv_t dos_to_local;

/*
 * Initialize conversion from codepage.
 * codepage = -1 means default codepage.
 * Returns 0 on success, non-zero on failure
 */
static int init_conversion(const char* codepage)
{
    static int initialized = -1;
    if (initialized < 0) {
        initialized = 1;
        if (NULL == codepage)
            codepage = DEFAULT_DOS_CODEPAGE;
        setlocale(LC_ALL, "");  /* initialize locale */
        dos_to_local = iconv_init_codepage(codepage);
        if (dos_to_local == (iconv_t) - 1 && strcmp(codepage, DEFAULT_DOS_CODEPAGE) != 0) {
            printf("Trying to set fallback DOS codepage %s\n",
                   DEFAULT_DOS_CODEPAGE);
            dos_to_local = iconv_init_codepage(DEFAULT_DOS_CODEPAGE);
            if (dos_to_local == (iconv_t) - 1)
                initialized = 0;    /* no conversion available */
        }
    }
    return initialized;
}

/*
 * Set the DOS codepage to use for conversion.
 * Returns 0 on success, non-zero on failure
 */
int SetDosCodepage(const char* codepage)
{
    return init_conversion(codepage);
}

/*
 * Convert a single DOS character to a printable character.
 * Returns 0 on success, non-zero on failure
 */
int DosCharToPrintable(char **p, unsigned char c)
{
    char in[1] = { c };
    char *pin = in;
    size_t bytes_in = 1;
    size_t bytes_out = 4;
    if (!init_conversion(NULL))
        return 0;
    return iconv(dos_to_local, &pin, &bytes_in, p, &bytes_out) != -1;
}
