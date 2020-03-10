#ifndef MPU_H
#define MPU_H

#include <vector>

#include "msp430x_arch.H"
#include "coreutils.h"

class MPU
{
    public:
        MPU(
            uint32_t address_begin,
            uint32_t address_end,
            size_t segment_count,
            ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword> &RB,
            ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM,
            interrupt_handler_t interrupt_handler,
            size_t interrupt_id);

        uint16_t read(uint32_t address) const;
        uint8_t read_byte(uint32_t address) const;
        
        void write(uint32_t address, uint16_t word);
        void write_byte(uint32_t address, uint8_t byte);

        // Syscalls
        void init();
        void unblock(size_t blockid);

    private:
        bool is_in_sram(uint32_t address, size_t &blockid);

        ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword> &RB;
        ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM;
        uint32_t address_begin;
        uint32_t address_end;
        size_t block_size;
        std::vector<bool> segments; // false => unlocked, true => locked
        interrupt_handler_t interrupt_handler;
        size_t interrupt_id;
};

#endif

