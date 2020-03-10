#include "mpu.h"


MPU::MPU(uint32_t address_begin, uint32_t address_end, size_t segment_count, ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM):
    DM(DM),
    segments(segment_count)
{
}

uint16_t MPU::read(uint32_t address) const
{
    return DM.read(address);
}

uint8_t MPU::read_byte(uint32_t address) const
{
    return DM.read_byte(address);
}

void MPU::write(uint32_t address, uint16_t word)
{
    // TODO check permissions
    DM.write(address, word);
}

void MPU::write_byte(uint32_t address, uint8_t byte)
{
    // TODO check permissions
    DM.write_byte(address, byte);
}

