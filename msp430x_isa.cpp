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
    unsigned int V, SCG1, SCG0, OSCOFF, CPUOFF, GIE, N, Z, C;

    sr_flags_t(
        ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB)
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
};

enum addressing_mode_e
{
    AM_REGISTER = 0,
    AM_INDEXED = 1,
    AM_INDIRECT_REG = 2,
    AM_INDIRECT_INCR = 3,
    AM_INVALID
};

static uint16_t doubleop_source(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    ac_reg<unsigned> &ac_pc,
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
            int16_t x = DM.read(ac_pc + 2);
            if(bw)
                operand = DM.read_byte(RB[rsrc] + x);
            else
                operand = DM.read(RB[rsrc] + x);
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
    ac_pc = RB[REG_PC];

    return operand;
}

static uint16_t doubleop_dest_operand(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    const ac_reg<unsigned> &ac_pc,
    uint16_t ad, uint16_t bw, uint16_t rdst)
{
    switch(ad)
    {
        case AM_REGISTER:
            return RB[rdst];

        case AM_INDEXED:
        {
            int16_t x = DM.read(ac_pc + 2);
            if(bw)
                return DM.read_byte(RB[rdst] + x);
            else
                return DM.read(RB[rdst] + x);
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
    ac_reg<unsigned> &ac_pc,
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
            int16_t x = DM.read(ac_pc + 2);
            if(bw)
                DM.write_byte(RB[rdst] + x, operand);
            else
                DM.write(RB[rdst] + x, operand);
            RB[REG_PC] += 2;
            break;
        }

        default:
            // Oops
            break;
    }
    ac_pc = RB[REG_PC];
}

//!Behavior executed before simulation begins.
void ac_behavior( begin ){}

//!Behavior executed after simulation ends.
void ac_behavior( end ){}

//!Generic instruction behavior method.
void ac_behavior( instruction )
{
    std::cout << "pc=" << std::hex << ac_pc << std::endl;
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
    uint16_t operand = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);

    doubleop_dest(DM, RB, ac_pc, operand, ad, bw, rdst);
}

//!Instruction ADD behavior method.
void ac_behavior( ADD )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);

    operand_dst += operand_src;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction ADDC behavior method.
void ac_behavior( ADDC )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);
    sr_flags_t sr(RB);

    operand_dst += operand_src + sr.C;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction SUB behavior method.
void ac_behavior( SUB )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);

    operand_dst += ~operand_src + 1;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction SUBC behavior method.
void ac_behavior( SUBC )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);
    sr_flags_t sr(RB);

    operand_dst += ~operand_src + sr.C;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction CMP behavior method.
void ac_behavior( CMP )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);
    uint16_t tmp = operand_dst;

    operand_dst -= operand_src;

    // Do not change the value
    doubleop_dest(DM, RB, ac_pc, tmp, ad, bw, rdst);
}

//!Instruction DADD behavior method.
void ac_behavior( DADD )
{
    std::cerr << "oops" << std::endl;
}

//!Instruction BIT behavior method.
void ac_behavior( BIT )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);
    uint16_t tmp = operand_dst;

    operand_dst &= operand_src;

    // Do not change the value
    doubleop_dest(DM, RB, ac_pc, tmp, ad, bw, rdst);
}

//!Instruction BIC behavior method.
void ac_behavior( BIC )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);

    operand_dst &= ~operand_src;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction BIS behavior method.
void ac_behavior( BIS )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);

    operand_dst |= operand_src;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction XOR behavior method.
void ac_behavior( XOR )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);

    operand_dst ^= operand_src;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction AND behavior method.
void ac_behavior( AND )
{
    uint16_t operand_src = doubleop_source(DM, RB, ac_pc, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(DM, RB, ac_pc, ad, bw, rdst);

    operand_dst &= operand_src;

    doubleop_dest(DM, RB, ac_pc, operand_dst, ad, bw, rdst);
}

//!Instruction RRC behavior method.
void ac_behavior( RRC ){}

//!Instruction RRA behavior method.
void ac_behavior( RRA ){}

//!Instruction PUSH behavior method.
void ac_behavior( PUSH ){}

//!Instruction SWPB behavior method.
void ac_behavior( SWPB ){}

//!Instruction CALL behavior method.
void ac_behavior( CALL ){}

//!Instruction RETI behavior method.
void ac_behavior( RETI ){}

//!Instruction SXT behavior method.
void ac_behavior( SXT ){}

//!Instruction JZ behavior method.
void ac_behavior( JZ ){}

//!Instruction JNZ behavior method.
void ac_behavior( JNZ ){}

//!Instruction JC behavior method.
void ac_behavior( JC ){}

//!Instruction JNC behavior method.
void ac_behavior( JNC ){}

//!Instruction JN behavior method.
void ac_behavior( JN ){}

//!Instruction JGE behavior method.
void ac_behavior( JGE ){}

//!Instruction JL behavior method.
void ac_behavior( JL ){}

//!Instruction JMP behavior method.
void ac_behavior( JMP ){}

//!Instruction PUSHPOPM behavior method.
void ac_behavior( PUSHPOPM ){}

