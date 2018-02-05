#pragma once

typedef struct
{
        struct
        {
                uint8_t data;
                bool wildcard;
        } nibble[2];
} PatternByte;

//returns: offset to data when found, -1 when not found
size_t patternfind(
        uint8_t *data, //data
        size_t datasize, //size of data
        const char *pattern //pattern to search
);

//returns: offset to data when found, -1 when not found
size_t patternfind_bytes(
        uint8_t *data, //data
        size_t datasize, //size of data
        const uint8_t *pattern, //bytes to search
        size_t patternsize //size of bytes to search
);

//returns: nothing
void patternwrite(
        uint8_t *data, //data
        size_t datasize, //size of data
        const char *pattern //pattern to write
);

//returns: true on success, false on failure
bool patternsnr(
        uint8_t *data, //data
        size_t datasize, //size of data
        const char *searchpattern, //pattern to search
        const char *replacepattern //pattern to write
);

//returns: true on success, false on failure
bool patterntransform(
        const char *patterntext, //pattern string
        PatternByte *pattern, //pattern to feed to patternfind
        size_t patternsize //size of pattern
);

//returns: offset to data when found, -1 when not found
size_t patternfind_pbyte(
        uint8_t *data, //data
        size_t datasize, //size of data
        const PatternByte *pattern, //pattern to search
        size_t searchpatternsize //size of pattern to search
);
