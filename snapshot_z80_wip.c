typedef struct {
    uint8_t  a, f;
    uint8_t a_, f_;
    uint8_t i, r;

    uint16_t ix, iy;
    uint16_t sp, pc;
    uint16_t bc, de, hl;
    uint16_t bc_, de_, hl_;

    uint8_t iff1, iff2, im, out_ula;
} snap_state_t;

static void read_snapshot_z80_header(uint8_t *buffer, snap_state_t *snap_state, bool *compressed) {
    snap_state->a = buffer[0];
    snap_state->f = buffer[1];
    snap_state->bc = buffer[2] + (buffer[3] << 8);
    snap_state->hl = buffer[4] + (buffer[5] << 8);
    snap_state->pc = buffer[6] + (buffer[7] << 8);
    snap_state->sp = buffer[8] + (buffer[9] << 8);
    snap_state->i = buffer[10];
    snap_state->r =  (buffer[11] & 0x7f)  +         // Refresh register (bit 7 is not significant)
			         ((buffer[12] & 0x01) << 7 );   // bit0 = R bit 7)
    snap_state->out_ula = buffer[12] = (buffer[12] & 0x0e) >> 1; // bits1..3 = border
    snap_state->de = buffer[13] + (buffer[14] << 8);
    snap_state->bc_ = buffer[15] + (buffer[16] << 8);
    snap_state->de_ = buffer[17] + (buffer[18] << 8);
    snap_state->hl_ = buffer[19] + (buffer[20] << 8);
    snap_state->a_ = buffer[21];
    snap_state->f_ = buffer[22];
    snap_state->iy = buffer[23] + (buffer[24] << 8);
    snap_state->ix = buffer[25] + (buffer[26] << 8);
    snap_state->iff1 = buffer[27] ? 1 : 0;
    snap_state->iff2 = buffer[28] ? 1 : 0;
    snap_state->im = buffer[29] & 0x03;
    if (snap_state->pc == 0) {
        // z80 snapshot header version >-= 2
        uint16_t extra_header_length = buffer[30] + (buffer[31] << 8);
        snap_state->pc = buffer[31] + (buffer[32] << 8);
        *compressed = (buffer[12] & 0x20) ? 1 : 0;
    } else {
        // z80 snapshot header v1
        *compressed = 1;
    }
    // LIBSPECTRUM_Z80_HEADER_LENGTH = 30
    // TODO (*data) = buffer + LIBSPECTRUM_Z80_HEADER_LENGTH + 2 + extra_length; // >= v2
    // TODO (*data) = buffer + LIBSPECTRUM_Z80_HEADER_LENGTH; // v1
    /*
    Hereafter a number of memory blocks follow, each containing the compressed data of a 16K block. The compression is according to the old scheme, except for the end-marker, which is now absent. The structure of a memory block is:

        Byte    Length  Description
        ---------------------------
        0       2       Length of compressed data (without this 3-byte header)
                        If length=0xffff, data is 16384 bytes long and not compressed
        2       1       Page number of block
        3       [0]     Data

        https://worldofspectrum.org/faq/reference/z80format.htm
        In 48K mode, pages 4,5 and 8 are saved. 

        Compression
        v1  compressed if bit 5 of byte 12 is set.buffer
        v2  always compressed (?)
    */

   // TODO https://github.com/speccytools/libspectrum/blob/8ba1310586e906fb4bff202c59c8714b180430f4/z80.c#L207
   // TODO read_blocks https://github.com/speccytools/libspectrum/blob/8ba1310586e906fb4bff202c59c8714b180430f4/z80.c#L675

}