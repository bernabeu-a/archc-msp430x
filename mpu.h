#ifndef MPU_H
#define MPU_H

#include <vector>

#include "msp430x_arch.H"

class MPU
{
    public:
        MPU(uint32_t address_begin, uint32_t address_end, size_t segment_count, ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM);

        uint16_t read(uint32_t address) const;
        uint8_t read_byte(uint32_t address) const;
        
        void write(uint32_t address, uint16_t word);
        void write_byte(uint32_t address, uint8_t byte);

    private:
        ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM;
        std::vector<bool> segments;
};

#endif

