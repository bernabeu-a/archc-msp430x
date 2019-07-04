#include <iostream>

#include  "msp430x_isa.H"
#include  "msp430x_isa_init.cpp"
#include  "msp430x_bhv_macros.H"

#include "sytare-syscalls/syscalls.h"

#define REG_PC  0
#define REG_SP  1
#define REG_SR  2
#define REG_CG1 REG_SR
#define REG_CG2 3

//!'using namespace' statement to allow access to all msp430x-specific datatypes
using namespace msp430x_parms;

struct sr_flags_t
{
    // Never write to those fields firectly. Reading is fine.
    unsigned int V, SCG1, SCG0, OSCOFF, CPUOFF, GIE, N, Z, C;
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB;

    sr_flags_t(
        ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB):
        RB(RB)
    {
        uint16_t sr = RB[REG_SR];
        V      = ((sr >> 8) & 1);
        SCG1   = ((sr >> 7) & 1);
        SCG0   = ((sr >> 6) & 1);
        OSCOFF = ((sr >> 5) & 1);
        CPUOFF = ((sr >> 4) & 1);
        GIE    = ((sr >> 3) & 1);
        N      = ((sr >> 2) & 1);
        Z      = ((sr >> 1) & 1);
        C      = ((sr     ) & 1);
    }

    void set_V(unsigned int value)
    {
        V = (value ? 1 : 0);
        update_register();
    }

    void set_N(unsigned int value)
    {
        N = (value ? 1 : 0);
        update_register();
    }

    void set_Z(unsigned int value)
    {
        Z = (value ? 1 : 0);
        update_register();
    }

    void set_C(unsigned int value)
    {
        C = (value ? 1 : 0);
        update_register();
    }

    void set_GIE(unsigned int value)
    {
        GIE = (value ? 1 : 0);
        update_register();
    }

    void update_register(void)
    {
        RB[REG_SR] = (V      << 8)
                   | (SCG1   << 7)
                   | (SCG0   << 6)
                   | (OSCOFF << 5)
                   | (CPUOFF << 4)
                   | (GIE    << 3)
                   | (N      << 2)
                   | (Z      << 1)
                   | (C          );
    }
};

enum extension_state_e
{
    EXT_NONE,
    EXT_RDY,
    EXT_RUN,
    EXT_ERROR
};

struct extension_t
{
    uint16_t payload_h, payload_l, al;
    extension_state_e state;

    extension_t():
        state(EXT_NONE)
    {
    }

    void tick()
    {
        if(state == EXT_RDY)
            state = EXT_RUN;
        else if(state == EXT_RUN)
        {
            state = EXT_ERROR;
            std::cerr << "Extension state error (Oops)" << std::endl;
        }
    }
};

enum addressing_mode_e
{
    AM_REGISTER = 0,
    AM_INDEXED = 1,
    AM_INDIRECT_REG = 2,
    AM_INDIRECT_INCR = 3,
    AM_INVALID
};

static extension_t extension;
static platform_t platform;
static Syscalls *syscalls;
static size_t cycles;
static struct
{
    bool measuring;
    size_t cycles;
    size_t current;
} former_current_point;

static unsigned int negative16(uint16_t x)
{
    return x >> 15;
}

static unsigned int negative8(uint8_t x)
{
    return x >> 7;
}

static unsigned int carry16(uint32_t x)
{
    return x & (1 << 16);
}

static unsigned int carry8(uint32_t x)
{
    return x & (1 << 8);
}

static unsigned int overflow16(uint16_t op1, uint16_t op2, uint16_t result)
{
    return (~(op1 ^ op2) & (result ^ op1)) >> 15;
}

static unsigned int overflow8(uint8_t op1, uint8_t op2, uint8_t result)
{
    return (~(op1 ^ op2) & (result ^ op1)) >> 7;
}

static int16_t u10_to_i16(uint16_t u10)
{
    uint16_t tmp = (u10 & 0x01ff);
    if(u10 & (1 << 9))
        tmp |= 0xfe00;
    return tmp;
}

static uint16_t doubleop_source(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    uint16_t as, uint16_t bw, uint16_t rsrc)
{
    uint16_t operand;

    switch(as)
    {
        case AM_REGISTER:
            if(rsrc == REG_CG2)
                operand = 0;
            else
            {
                operand = RB[rsrc];
                if(bw)
                    operand &= 0xff;
            }
            break;

        case AM_INDEXED:
            if(rsrc == REG_CG2)
                operand = 0x1;
            /*
            else if(rsrc == REG_CG1)
            {
                operand = DM.read(RB[rsrc]);
                std::cerr << "Oops As=1, src=r2" << std::endl;
            }
            */
            else
            {
                uint16_t x = DM.read(RB[REG_PC]);
                if(bw)
                    operand = DM.read_byte(x);
                else
                    operand = DM.read(x);
                RB[REG_PC] += 2;
            }
            break;

        case AM_INDIRECT_REG:
            if(rsrc == REG_CG2)
                operand = 0x2;
            else if(rsrc == REG_CG1)
                operand = 0x4;
            else
            {
                if(bw)
                    operand = DM.read_byte(RB[rsrc]);
                else
                    operand = DM.read(RB[rsrc]);
            }
            break;

        case AM_INDIRECT_INCR:
            if(rsrc == REG_CG2)
                operand = 0xffff;
            else if(rsrc == REG_CG1)
                operand = 0x8;
            else
            {
                operand = DM.read(RB[rsrc]);
                // /!\ Here, pc may change if rsrc==0, which is the expected behavior
                // TODO: 20bit address mode?
                if(rsrc == REG_PC || !bw)
                    RB[rsrc] += 2;
                else
                    RB[rsrc] += 1;
            }
            break;

        default:
            // Oops
            break;
    }

    return operand;
}

static uint16_t doubleop_dest_operand(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    uint16_t ad, uint16_t bw, uint16_t rdst)
{
    switch(ad)
    {
        case AM_REGISTER:
            return RB[rdst];

        case AM_INDEXED:
        {
            uint16_t x = DM.read(RB[REG_PC]);
            if(bw)
                return DM.read_byte(x);
            else
                return DM.read(x);
        }

        default:
            // Oops
            break;
    }
    return -1;
}

static void doubleop_dest(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    uint16_t operand,
    uint16_t ad, uint16_t bw, uint16_t rdst)
{
    switch(ad)
    {
        case AM_REGISTER:
            RB[rdst] = operand;
            break;

        case AM_INDEXED:
        {
            uint16_t x = DM.read(RB[REG_PC]);
            if(bw)
                DM.write_byte(x, operand);
            else
                DM.write(x, operand);
            RB[REG_PC] += 2;
            break;
        }

        default:
            // Oops
            break;
    }
}

static size_t doubleop_cycles(uint8_t as, uint8_t ad, uint8_t rsrc, uint8_t rdst, bool shorter)
{
    switch(as)
    {
        case AM_REGISTER: // src = Rn
            if(ad == AM_REGISTER) // dst = Rm or dst = PC
                return rdst == REG_PC ? 3 : 1;
            return shorter ? 3 : 4;

        case AM_INDEXED: // src = ADDR or src = &ADDR or src = X(Rn)
            if(ad == AM_REGISTER) // dst = Rm or dst = PC
                return rdst == REG_PC ? 5 : 3;
            return shorter ? 5 : 6;

        case AM_INDIRECT_REG: // src = @Rn
            if(ad == AM_REGISTER) // dst = Rm or dst = PC
                return rdst == REG_PC ? 4 : 2;
            return shorter ? 4 : 5;

        case AM_INDIRECT_INCR:
            if(rsrc == REG_PC) // src = #N
            {
                if(ad == AM_REGISTER) // dst = Rm or dst = PC
                    return rdst == REG_PC ? 3 : 2;
            }
            else // src = @Rn+
            {
                if(ad == AM_REGISTER) // dst = Rm or dst = PC
                    return rdst == REG_PC ? 4 : 2;
            }
            return shorter ? 4 : 5;

        default:
            // Oops
            break;
    }
    return 0;
}

static void extension_to_repeat(
    const extension_t &extension,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    uint16_t &zc,
    uint16_t &al,
    uint16_t &count)
{
    zc = (extension.payload_h >> 1) & 1;
    al = extension.al;

    if(extension.payload_h & 1) // #
    {
        uint16_t rn = extension.payload_l & 0xf;
        count = 1 + (RB[rn] & 0xf);
    }
    else
        count = 1 + (extension.payload_l & 0xf);
}

//!Behavior executed before simulation begins.
void ac_behavior( begin )
{
    syscalls = new Syscalls(platform);
    syscalls->print();

    cycles = 0;
    former_current_point.measuring = false;
}

//!Behavior executed after simulation ends.
void ac_behavior( end )
{
    delete syscalls;
}

//!Generic instruction behavior method.
void ac_behavior( instruction )
{
    extension.tick();

    if(platform.energy.is_measuring())
    {
        size_t current = platform.current();

        if(!former_current_point.measuring)
            std::cout << "> " << std::dec << cycles << ", " << current << std::endl;
        else if(former_current_point.current != current)
        {
            std::cout << "> " << std::dec << former_current_point.cycles << ", " << former_current_point.current << std::endl
                      << "> " << std::dec << cycles << ", " << current << std::endl;
        }

        former_current_point.measuring = true;
        former_current_point.cycles = cycles;
        former_current_point.current = current;
    }
    else
        former_current_point.measuring = false;

    //std::cout << std::endl;
    //std::cout << "pc=" << std::hex << ac_pc << std::endl;
    //std::cout << "sp=" << std::hex << RB[REG_SP] << std::endl;

    ac_pc += 2;
    RB[REG_PC] = ac_pc;
}
 
//! Instruction Format behavior methods.
void ac_behavior( Type_DoubleOp ) {}
void ac_behavior( Type_SimpleOp ) {}
void ac_behavior( Type_Jump ) {}
void ac_behavior( Type_PushPopM ){}
void ac_behavior( Type_Extension ){}
 
//!Instruction MOV behavior method.
void ac_behavior( MOV )
{
    uint16_t operand = doubleop_source(DM, RB, as, bw, rsrc);

    doubleop_dest(DM, RB, operand, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, true);
}

//!Instruction ADD behavior method.
void ac_behavior( ADD )
{
    uint16_t zc = 0;
    uint16_t al = 1;
    uint16_t count = 1;

    if(extension.state == EXT_RUN)
    {
        std::cout << "Extended ADD" << std::endl;

        if(as == 0 && ad == 0)
        {
            extension_to_repeat(extension, RB, zc, al, count);
            std::cout << " " << std::dec << count << " times" << std::endl;
        }
        else
            std::cerr << "Oops, extension not supported yet." << std::endl;
    }

    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    for(size_t i = count; i--; )
        operand_dst += operand_src;

    sr.set_Z(operand_dst == 0);
    if(bw)
    {
        sr.set_N(negative8(operand_dst));
        sr.set_V(overflow8(operand_src, operand_tmp, operand_dst));
    }
    else
    {
        sr.set_N(negative16(operand_dst));
        sr.set_V(overflow16(operand_src, operand_tmp, operand_dst));
    }

    uint32_t promoted_src = operand_src;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];
    extension.state = EXT_NONE;

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction ADDC behavior method.
void ac_behavior( ADDC )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_dst += operand_src + sr.C;

    sr.set_Z(operand_dst == 0);
    if(bw)
    {
        sr.set_N(negative8(operand_dst));
        sr.set_V(overflow8(operand_src, operand_tmp, operand_dst));
    }
    else
    {
        sr.set_N(negative16(operand_dst));
        sr.set_V(overflow16(operand_src, operand_tmp, operand_dst));
    }

    uint32_t promoted_src = operand_src + sr.C;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction SUB behavior method.
void ac_behavior( SUB )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_dst += ~operand_src + 1;

    sr.set_Z(operand_dst == 0);
    if(bw)
    {
        sr.set_N(negative8(operand_dst));
        sr.set_V(overflow8(operand_src, operand_tmp, operand_dst));
    }
    else
    {
        sr.set_N(negative16(operand_dst));
        sr.set_V(overflow16(operand_src, operand_tmp, operand_dst));
    }

    uint32_t promoted_src = ~operand_src + 1;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction SUBC behavior method.
void ac_behavior( SUBC )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_dst += ~operand_src + sr.C;

    sr.set_Z(operand_dst == 0);
    if(bw)
    {
        sr.set_N(negative8(operand_dst));
        sr.set_V(overflow8(operand_src, operand_tmp, operand_dst));
    }
    else
    {
        sr.set_N(negative16(operand_dst));
        sr.set_V(overflow16(operand_src, operand_tmp, operand_dst));
    }

    uint32_t promoted_src = ~operand_src + sr.C;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction CMP behavior method.
void ac_behavior( CMP )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_dst += ~operand_src + 1;

    sr.set_Z(operand_dst == 0);
    if(bw)
    {
        sr.set_N(negative8(operand_dst));
        sr.set_V(overflow8(operand_src, operand_tmp, operand_dst));
    }
    else
    {
        sr.set_N(negative16(operand_dst));
        sr.set_V(overflow16(operand_src, operand_tmp, operand_dst));
    }

    uint32_t promoted_src = ~operand_src + 1;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    // Do not change the value
    doubleop_dest(DM, RB, operand_tmp, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, true);
}

//!Instruction DADD behavior method.
void ac_behavior( DADD )
{
    std::cerr << "Oops (DADD)" << std::endl;
}

//!Instruction BIT behavior method.
void ac_behavior( BIT )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_dst &= operand_src;

    sr.set_Z(operand_dst == 0);
    sr.set_C(operand_dst != 0);
    sr.set_V(0);
    if(bw)
        sr.set_N(negative8(operand_dst));
    else
        sr.set_N(negative16(operand_dst));

    // Do not change the value
    doubleop_dest(DM, RB, tmp, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, true);
}

//!Instruction BIC behavior method.
void ac_behavior( BIC )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);

    operand_dst &= ~operand_src;

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction BIS behavior method.
void ac_behavior( BIS )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);

    operand_dst |= operand_src;

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction XOR behavior method.
void ac_behavior( XOR )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_dst ^= operand_src;

    sr.set_Z(operand_dst == 0);
    sr.set_C(operand_dst != 0);
    if(bw)
    {
        sr.set_N(negative8(operand_dst));
        sr.set_V(negative8(operand_src) && negative8(operand_tmp));
    }
    else
    {
        sr.set_N(negative16(operand_dst));
        sr.set_V(negative16(operand_src) && negative16(operand_tmp));
    }

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction AND behavior method.
void ac_behavior( AND )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    sr_flags_t sr(RB);

    operand_dst &= operand_src;

    sr.set_Z(operand_dst == 0);
    sr.set_C(operand_dst != 0);
    sr.set_V(0);
    if(bw)
        sr.set_N(negative8(operand_dst));
    else
        sr.set_N(negative16(operand_dst));

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    cycles += doubleop_cycles(as, ad, rsrc, rdst, false);
}

//!Instruction RRC behavior method.
void ac_behavior( RRC )
{
    std::cerr << "Oops (RRC)" << std::endl;
}

//!Instruction RRA behavior method.
void ac_behavior( RRA )
{
    std::cerr << "Oops (RRA)" << std::endl;
}

//!Instruction PUSH behavior method.
void ac_behavior( PUSH )
{
    std::cerr << "Oops (PUSH)" << std::endl;
}

//!Instruction SWPB behavior method.
void ac_behavior( SWPB )
{
    std::cerr << "Oops (SWPB)" << std::endl;
}

//!Instruction CALL behavior method.
void ac_behavior( CALL )
{
    uint16_t address = doubleop_source(DM, RB, ad, 0, rdst);
    if(syscalls->is_syscall(address)) // Syscall: run symbolically
    {
        std::cout << "SYSCALL: " << syscalls->get_name(address) << std::endl;
        syscalls->run(address, RB);
    }
    else // Actually run the call
    {
        RB[REG_SP] -= 2;
        DM.write(RB[REG_SP], RB[REG_PC]);
        RB[REG_PC] = address;
    }

    ac_pc = RB[REG_PC];

    if(ad == AM_REGISTER)
        cycles += 4;
    else if(rdst == REG_SR)
        cycles += 6;
    else
        cycles += 5;
}

//!Instruction RETI behavior method.
void ac_behavior( RETI )
{
    std::cout << "Oops (RETI)" << std::endl;
}

//!Instruction SXT behavior method.
void ac_behavior( SXT )
{
    std::cout << "Oops (SXT)" << std::endl;
}

//!Instruction JZ behavior method.
void ac_behavior( JZ )
{
    sr_flags_t sr(RB);
    if(sr.Z)
    {
        int16_t signed_offset = 2 * u10_to_i16(offset);
        RB[REG_PC] += signed_offset;
        // TODO: Check whether PC gets incremented by 2 at the end (should be true)
    }
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction JNZ behavior method.
void ac_behavior( JNZ )
{
    sr_flags_t sr(RB);
    if(!sr.Z)
    {
        int16_t signed_offset = 2 * u10_to_i16(offset);
        RB[REG_PC] += signed_offset;
        // TODO: Check whether PC gets incremented by 2 at the end (should be true)
    }
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction JC behavior method.
void ac_behavior( JC )
{
    sr_flags_t sr(RB);
    if(sr.C)
    {
        int16_t signed_offset = 2 * u10_to_i16(offset);
        RB[REG_PC] += signed_offset;
        // TODO: Check whether PC gets incremented by 2 at the end (should be true)
    }
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction JNC behavior method.
void ac_behavior( JNC )
{
    sr_flags_t sr(RB);
    if(!sr.C)
    {
        int16_t signed_offset = 2 * u10_to_i16(offset);
        RB[REG_PC] += signed_offset;
        // TODO: Check whether PC gets incremented by 2 at the end (should be true)
    }
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction JN behavior method.
void ac_behavior( JN )
{
    sr_flags_t sr(RB);
    if(sr.N)
    {
        int16_t signed_offset = 2 * u10_to_i16(offset);
        RB[REG_PC] += signed_offset;
        // TODO: Check whether PC gets incremented by 2 at the end (should be true)
    }
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction JGE behavior method.
void ac_behavior( JGE )
{
    sr_flags_t sr(RB);
    if(!(sr.N ^ sr.V))
    {
        int16_t signed_offset = 2 * u10_to_i16(offset);
        RB[REG_PC] += signed_offset;
        // TODO: Check whether PC gets incremented by 2 at the end (should be true)
    }
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction JL behavior method.
void ac_behavior( JL )
{
    sr_flags_t sr(RB);
    if(sr.N ^ sr.V)
    {
        int16_t signed_offset = 2 * u10_to_i16(offset);
        RB[REG_PC] += signed_offset;
        // TODO: Check whether PC gets incremented by 2 at the end (should be true)
    }
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction JMP behavior method.
void ac_behavior( JMP )
{
    int16_t signed_offset = 2 * u10_to_i16(offset);
    RB[REG_PC] += signed_offset;
    ac_pc = RB[REG_PC];

    cycles += 2;
}

//!Instruction PUSHPOPM behavior method.
void ac_behavior( PUSHPOPM )
{
    uint16_t n = 1 + n1;
    uint16_t rdst = rdst1 + n1;

    if(!(subop & 0x1))
        std::cerr << "PUSHPOPM: address mode not supported." << std::endl;

    std::cout << "PUSHPOP:" << std::endl
              << " n=" << std::dec << n << std::endl
              << " rdst=" << rdst << std::endl
              << " before: SP=" << std::hex << RB[REG_SP] << std::endl;

    if(!(subop & 0x2)) // PUSHM
    {
        std::cout << " It's a pushm!" << std::endl;
        for(; n; --n, --rdst)
        {
            std::cout << "  r" << std::dec << rdst << std::endl;
            RB[REG_SP] -= 2;
            DM.write(RB[REG_SP], RB[rdst]);
        }
    }
    else // POPM
    {
        std::cout << " It's a popm!" << std::endl;
        for(; n; --n, ++rdst)
        {
            std::cout << "  r" << std::dec << rdst << std::endl;
            RB[rdst] = DM.read(RB[REG_SP]);
            RB[REG_SP] += 2;
        }
    }
    ac_pc = RB[REG_PC];

    std::cout << " after: SP=" << std::hex << RB[REG_SP] << std::endl
              << std::endl;

    cycles += 3 + n1;
}

//!Instruction EXT behavior method.
void ac_behavior( EXT )
{
    extension.payload_h = payload_h;
    extension.payload_l = payload_l;
    extension.al        = al;
    if(extension.state != EXT_NONE)
        std::cerr << "Bad extension state (Oops)" << std::endl;
    extension.state = EXT_RDY;

    std::cout << "Extension!" << std::endl;
}

