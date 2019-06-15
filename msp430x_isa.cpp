#include <iostream>

#include  "msp430x_isa.H"
#include  "msp430x_isa_init.cpp"
#include  "msp430x_bhv_macros.H"

#define REG_PC 0
#define REG_SP 1
#define REG_SR 2

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

union alu_value_u
{
    uint16_t u;
    int16_t i;
};

enum addressing_mode_e
{
    AM_REGISTER = 0,
    AM_INDEXED = 1,
    AM_INDIRECT_REG = 2,
    AM_INDIRECT_INCR = 3,
    AM_INVALID
};

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
            operand = RB[rsrc];
            if(bw)
                operand &= 0xff;
            break;

        case AM_INDEXED:
        {
            uint16_t x = DM.read(RB[REG_PC]);
            std::cout << "@" << std::hex << x << std::endl;
            if(bw)
                operand = DM.read_byte(x);
            else
                operand = DM.read(x);
            RB[REG_PC] += 2;
            break;
        }

        case AM_INDIRECT_REG:
            if(bw)
                operand = DM.read_byte(RB[rsrc]);
            else
                operand = DM.read(RB[rsrc]);
            break;

        case AM_INDIRECT_INCR:
            operand = DM.read(RB[rsrc]);
            // /!\ Here, pc may change if rsrc==0, which is the expected behavior
            // TODO: 20bit address mode?
            if(rsrc == REG_PC || !bw)
                RB[rsrc] += 2;
            else
                RB[rsrc] += 1;
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
    std::cout << " -> ";

    switch(ad)
    {
        case AM_REGISTER:
            std::cout << "r" << std::dec << rdst;
            RB[rdst] = operand;
            break;

        case AM_INDEXED:
        {
            uint16_t x = DM.read(RB[REG_PC]);
            std::cout << std::hex << x;
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

    std::cout << std::endl;
}

//!Behavior executed before simulation begins.
void ac_behavior( begin ){}

//!Behavior executed after simulation ends.
void ac_behavior( end ){}

//!Generic instruction behavior method.
void ac_behavior( instruction )
{
    std::cout << std::endl;
    std::cout << "pc=" << std::hex << ac_pc << std::endl;
    std::cout << "sp=" << std::hex << RB[REG_SP] << std::endl;

    ac_pc += 2;
    RB[REG_PC] = ac_pc;
}
 
//! Instruction Format behavior methods.
void ac_behavior( Type_DoubleOp ) {}
void ac_behavior( Type_SimpleOp ) {}
void ac_behavior( Type_Jump ) {}
void ac_behavior( Type_PushPopM ){}
 
//!Instruction MOV behavior method.
void ac_behavior( MOV )
{
    std::cout << "MOV" << std::endl
              << " " << std::dec << "as=" << (int)as << std::endl
              << " " << "ad=" << (int)ad << std::endl;

    uint16_t operand = doubleop_source(DM, RB, as, bw, rsrc);

    std::cout << std::hex << operand;

    doubleop_dest(DM, RB, operand, ad, bw, rdst);
    ac_pc = RB[REG_PC];
}

//!Instruction ADD behavior method.
void ac_behavior( ADD )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

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
}

//!Instruction DADD behavior method.
void ac_behavior( DADD )
{
    std::cerr << "oops (DADD)" << std::endl;
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
}

//!Instruction BIC behavior method.
void ac_behavior( BIC )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);

    operand_dst &= ~operand_src;

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];
}

//!Instruction BIS behavior method.
void ac_behavior( BIS )
{
    uint16_t operand_src = doubleop_source(DM, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ad, bw, rdst);

    operand_dst |= operand_src;

    doubleop_dest(DM, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];
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
}

//!Instruction RRC behavior method.
void ac_behavior( RRC )
{
    std::cerr << "oops (RRC)" << std::endl;
}

//!Instruction RRA behavior method.
void ac_behavior( RRA )
{
    std::cerr << "oops (RRA)" << std::endl;
}

//!Instruction PUSH behavior method.
void ac_behavior( PUSH )
{
    std::cerr << "oops (PUSH)" << std::endl;
}

//!Instruction SWPB behavior method.
void ac_behavior( SWPB )
{
    std::cerr << "oops (SWPB)" << std::endl;
}

//!Instruction CALL behavior method.
void ac_behavior( CALL )
{
    uint16_t address = doubleop_source(DM, RB, ad, 0, rdst);
    RB[REG_SP] -= 2;
    DM.write(RB[REG_SP], RB[REG_PC]);
    RB[REG_PC] = address;
    ac_pc = RB[REG_PC];

    printf("CALL:\n Rdst=%d\n Ad=%d\n\n", rdst, ad);
}

//!Instruction RETI behavior method.
void ac_behavior( RETI )
{
    std::cout << "oops (RETI)" << std::endl;
}

//!Instruction SXT behavior method.
void ac_behavior( SXT )
{
    std::cout << "oops (SXT)" << std::endl;
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
}

//!Instruction JMP behavior method.
void ac_behavior( JMP )
{
    int16_t signed_offset = 2 * u10_to_i16(offset);
    RB[REG_PC] += signed_offset;
    ac_pc = RB[REG_PC];
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
}

