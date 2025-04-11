// TODO zkontrolovat pro JS https://github.com/gasman/jsspeccy3/blob/main/runtime/snapshot.js
// Takes a pre-allocated output buffer (memoryBytes) that is 16KB in size
void extractMemoryBlock(const void* data, size_t fileOffset, int isCompressed, size_t unpackedLength, uint8_t* memoryBytes) {
    if (!isCompressed) {
        /* uncompressed; extract directly into provided buffer */
        memcpy(memoryBytes, (uint8_t*)data + fileOffset, unpackedLength);
    } else {
        /* compressed */
        const uint8_t* fileBytes = (uint8_t*)data + fileOffset;
        size_t filePtr = 0;
        size_t memoryPtr = 0;
        
        while (memoryPtr < unpackedLength) {
            /* check for coded ED ED nn bb sequence */
            if (
                unpackedLength - memoryPtr >= 2 && /* at least two bytes left to unpack */
                fileBytes[filePtr] == 0xed &&
                fileBytes[filePtr + 1] == 0xed
            ) {
                /* coded sequence */
                uint8_t count = fileBytes[filePtr + 2];
                uint8_t value = fileBytes[filePtr + 3];
                for (size_t i = 0; i < count; i++) {
                    memoryBytes[memoryPtr++] = value;
                }
                filePtr += 4;
            } else {
                /* plain byte */
                memoryBytes[memoryPtr++] = fileBytes[filePtr++];
            }
        }
    }
}
