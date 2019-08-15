#include "sci/Kernel/FarData.h"
#include "sci/Kernel/Resource.h"

static uint GetFarData(uint moduleNum, uint entryNum, void *buffer);

char *GetFarStr(uint moduleNum, uint entryNum, char *buffer)
{
    uint length;

    length         = GetFarData(moduleNum, entryNum, buffer);
    buffer[length] = '\0';

    return (length != 0) ? buffer : NULL;
}

char *GetFarText(uint moduleNum, uint offset, char *buffer)
{
    char *text;

    // Find the script, get the handle to its far text, find the
    // string in the script, then copy the string to the buffer.
    text = (char *)ResLoad(RES_TEXT, moduleNum);
    if (text == NULL) {
        // shouldn't get back here...but just in case
        *buffer = '\0';
    } else {
        // Scan for the nth string.
        while (offset-- != 0) {
            text += strlen(text) + 1;
        }

        // Then copy the string.
        strcpy(buffer, text);
    }

    return buffer;
}

static uint GetFarData(uint moduleNum, uint entryNum, void *buffer)
{
    uint16_t *data;
    uint      offset, length;

    // Get a handle to the appropriate resource.
    data = (uint16_t *)ResLoad(RES_VOCAB, moduleNum);

    // Value check the requested entryNum (first word of resource contains the
    // maximum directory entry).
    if (entryNum > (uint)*data) {
        return 0;
    }

    // Get a pointer to the data by looking up its offset in the directory.
    offset = *(data + entryNum + 1);
    data   = (uint16_t *)((uint8_t *)data + offset);

    // Get the length of the data.
    length = *data++;

    // Copy the data to its destination.
    memcpy(buffer, data, length);

    // Return the length of the data.
    return length;
}
