#include "stdafx.h"
#include "patternfind.h"

static inline bool isHex(char ch)
{
        return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

static inline int hexchtoint(char ch)
{
        if ( ch >= '0' && ch <= '9' )
                return ch - '0';
        else if ( ch >= 'A' && ch <= 'F' )
                return ch - 'A' + 10;
        else if ( ch >= 'a' && ch <= 'f' )
                return ch - 'a' + 10;
        return -1;
}

static inline size_t formathexpattern(const char *patterntext, char *formattext, size_t formattextsize)
{
        size_t len = strlen(patterntext);
        size_t result = 0;
        for ( size_t i = 0; i < len; i++ ) {
                if ( patterntext[i] == '?' || hexchtoint(patterntext[i]) != -1 ) {
                        if ( formattext && result + 1 < formattextsize )
                                formattext[result] = patterntext[i];

                        result++;
                }
        }
        if ( result % 2 ) { //not a multiple of 2
                if ( formattext && result + 1 < formattextsize )
                        formattext[result] = '?';

                result++;
        }
        if ( formattext ) {
                if ( result <= formattextsize )
                        formattext[result] = '\0';
                else
                        formattext[0] = '\0';
        }
        return result;
}

bool patterntransform(const char *patterntext, PatternByte *pattern, size_t patternsize)
{
        memset(pattern, 0, patternsize * (sizeof *pattern));
        size_t len = formathexpattern(patterntext, NULL, 0);

        if ( !len || len / 2 > patternsize )
                return false;

        size_t size = len + 1;
        char *formattext = malloc(size);
        formathexpattern(patterntext, formattext, size);
        PatternByte newByte;

        for ( size_t i = 0, j = 0, k = 0; i < len && k <= patternsize; i++ ) {
                if ( formattext[i] == '?' ) { //wildcard
                        newByte.nibble[j].wildcard = true; //match anything
                } else { //hex
                        newByte.nibble[j].wildcard = false;
                        newByte.nibble[j].data = hexchtoint(formattext[i]) & 0xF;
                }

                j++;
                if ( j == 2 ) { //two nibbles = one byte
                        j = 0;
                        pattern[k++] = newByte;
                }
        }
        free(formattext);
        return true;
}

static inline bool patternmatchbyte(uint8_t byte, const PatternByte pbyte)
{
        int matched = 0;

        uint8_t n1 = (byte >> 4) & 0xF;
        if ( pbyte.nibble[0].wildcard )
                matched++;
        else if ( pbyte.nibble[0].data == n1 )
                matched++;

        uint8_t n2 = byte & 0xF;
        if ( pbyte.nibble[1].wildcard )
                matched++;
        else if ( pbyte.nibble[1].data == n2 )
                matched++;

        return (matched == 2);
}

__declspec(noinline)
size_t patternfind(uint8_t *data, size_t datasize, const char *pattern)
{
        size_t searchpatternsize = formathexpattern(pattern, NULL, 0) / 2;
        PatternByte *searchpattern = calloc(searchpatternsize, sizeof(PatternByte));

        size_t result = -1;
        if ( patterntransform(pattern, searchpattern, searchpatternsize) )
                result = patternfind_pbyte(data, datasize, searchpattern, searchpatternsize);

        free(searchpattern);
        return result;
}

__declspec(noinline)
size_t patternfind_bytes(uint8_t *data, size_t datasize, const uint8_t *pattern, size_t patternsize)
{
        if ( patternsize > datasize )
                patternsize = datasize;
        for ( size_t i = 0, pos = 0; i < datasize; i++ ) {
                if ( data[i] == pattern[pos] ) {
                        pos++;
                        if ( pos == patternsize )
                                return i - patternsize + 1;
                } else if ( pos > 0 ) {
                        i -= pos;
                        pos = 0; //reset current pattern position
                }
        }
        return -1;
}

static inline void patternwritebyte(uint8_t *byte, const PatternByte pbyte)
{
        uint8_t n1 = (*byte >> 4) & 0xF;
        uint8_t n2 = *byte & 0xF;

        if ( !pbyte.nibble[0].wildcard )
                n1 = pbyte.nibble[0].data;
        if ( !pbyte.nibble[1].wildcard )
                n2 = pbyte.nibble[1].data;
        *byte = ((n1 << 4) & 0xF0) | (n2 & 0xF);
}

void patternwrite(uint8_t *data, size_t datasize, const char *pattern)
{
        size_t writepatternsize = formathexpattern(pattern, NULL, 0) / 2;
        PatternByte *writepattern = calloc(writepatternsize, sizeof(PatternByte));

        if ( patterntransform(pattern, writepattern, writepatternsize) ) {
                DWORD OldProtect;
                BOOL result = VirtualProtect(data, writepatternsize, PAGE_READWRITE, &OldProtect);
                if ( writepatternsize > datasize )
                        writepatternsize = datasize;
                for ( size_t i = 0; i < writepatternsize; i++ )
                        patternwritebyte(&data[i], writepattern[i]);
                result = VirtualProtect(data, writepatternsize, OldProtect, &OldProtect);
        }

        free(writepattern);
}

bool patternsnr(uint8_t *data, size_t datasize, const char *searchpattern, const char *replacepattern)
{
        size_t found = patternfind(data, datasize, searchpattern);
        if ( found == -1 )
                return false;
        patternwrite(data + found, found - datasize, replacepattern);
        return true;
}

size_t patternfind_pbyte(uint8_t *data, size_t datasize, const PatternByte *pattern, size_t searchpatternsize)
{
        for ( size_t i = 0, pos = 0; i < datasize; i++ ) { //search for the pattern
                if ( patternmatchbyte(data[i], pattern[pos]) ) { //check if our pattern matches the current byte
                        pos++;
                        if ( pos == searchpatternsize ) //everything matched
                                return i - searchpatternsize + 1;
                } else if ( pos > 0 ) { //fix by Computer_Angel
                        i -= pos;
                        pos = 0; //reset current pattern position
                }
        }
        return -1;
}
