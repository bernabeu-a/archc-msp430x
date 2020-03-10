#include <iostream>

#include "mpu.h"


MPU::MPU(
    uint32_t address_begin,
    uint32_t address_end,
    size_t segment_count,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword> &RB,
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM,
    interrupt_handler_t interrupt_handler,
    size_t interrupt_id):

    RB(RB),
    DM(DM),
    address_begin(address_begin),
    address_end(address_end),
    block_size((address_end-address_begin) / segment_count),
    segments(segment_count, false), // Every segment is unlocked by default
    interrupt_handler(interrupt_handler),
    interrupt_id(interrupt_id)
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
    size_t blockid;
    if(is_in_sram(address, blockid) && segments[blockid])
        interrupt_handler(RB, DM, interrupt_id);
    DM.write(address, word);
}

void MPU::write_byte(uint32_t address, uint8_t byte)
{
    size_t blockid;
    if(is_in_sram(address, blockid) && segments[blockid])
        interrupt_handler(RB, DM, interrupt_id);
    DM.write_byte(address, byte);
}

void MPU::init()
{
    // Lock all sections
    for(auto it = segments.begin(); it != segments.end(); ++it)
        *it = true;
}

void MPU::unblock(size_t blockid)
{
    if(blockid < segments.size())
        segments[blockid] = false;
    else
        std::cerr << "Oops MPU::unblock out-of-range (" << std::dec << blockid << ")"
                  << std::endl;
}

bool MPU::is_in_sram(uint32_t address, size_t &blockid)
{
    if(address >= address_begin && address < address_end)
    {
        blockid = (address - address_begin) / block_size;
        return true;
    }
    return false;
}

