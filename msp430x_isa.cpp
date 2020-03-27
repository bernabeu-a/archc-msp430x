#include <iostream>

#include  "msp430x_isa.H"
#include  "msp430x_isa_init.cpp"
#include  "msp430x_bhv_macros.H"

#include "sytare-syscalls/syscalls.h"
#include "energy_manager.h"
#include "energy_logger.h"
#include "commands.h"
#include "options.h"

#define REG_PC  0
#define REG_SP  1
#define REG_SR  2
#define REG_CG1 REG_SR
#define REG_CG2 3

const uint16_t SRAM_BEGIN  = 0x1c00;
const uint16_t SRAM_END    = 0x2000;

const uint16_t PERIPH_BEGIN = 0x0000;
const uint16_t PERIPH_END   = 0x1000;

//!'using namespace' statement to allow access to all msp430x-specific datatypes
using namespace msp430x_parms;

struct arch_context_t
{
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM;
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB;
    ac_reg<unsigned> &ac_pc;
    std::function<void()> ac_annul;
    uint16_t pc;

    arch_context_t(
        ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword> &DM,
        ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
        ac_reg<unsigned> &ac_pc,
        std::function<void()> ac_annul):
        DM(DM),
        RB(RB),
        ac_pc(ac_pc),
        ac_annul(ac_annul)
    {
    }
};

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

    void on_interrupt(void)
    {
        RB[REG_SR] &= (SCG0 << 6);
        V      = 0;
        SCG1   = 0;
        OSCOFF = 0;
        CPUOFF = 0;
        GIE    = 0;
        N      = 0;
        Z      = 0;
        C      = 0;

        // TODO out of LPM
    }

    void dump(void) const
    {
        std::cout << "# SR" << std::endl
                  << "  + V = " << V << std::endl
                  << "  + N = " << V << std::endl
                  << "  + Z = " << V << std::endl
                  << "  + C = " << V << std::endl;
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

struct extension_repeat_t
{
    uint16_t zc, al, count;
};

enum addressing_mode_e
{
    AM_REGISTER = 0,
    AM_INDEXED = 1,
    AM_INDIRECT_REG = 2,
    AM_INDIRECT_INCR = 3,
    AM_INVALID
};

static arch_context_t *context = nullptr;

static extension_t extension;
static platform_t *platform;
static Syscalls *syscalls;
static EnergyLogger elogger(std::cout);
static PowerSupply *supply;
static EnergyManager *emanager;
static Commands *commands;
static MPU *mpu;

static inline size_t ESTIMATE_PIPELINE(size_t ncycles)
{
    if(ncycles)
        return ncycles - 1;
    return ncycles;
}

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
    MPU *mpu,
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
                operand = RB[rsrc] - (rsrc == REG_PC ? 2 : 0);
                if(bw)
                    operand &= 0xff;
            }
            break;

        case AM_INDEXED:
            if(rsrc == REG_CG2)
                operand = 0x1;
            else
            {
                uint16_t x = mpu->read(RB[REG_PC]);
                if(rsrc != REG_CG1)
                    x += RB[rsrc];

                if(bw)
                    operand = mpu->read_byte(x);
                else
                    operand = mpu->read(x);
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
                    operand = mpu->read_byte(RB[rsrc]);
                else
                    operand = mpu->read(RB[rsrc]);
            }
            break;

        case AM_INDIRECT_INCR:
            if(rsrc == REG_CG2)
                operand = 0xffff;
            else if(rsrc == REG_CG1)
                operand = 0x8;
            else
            {
                operand = mpu->read(RB[rsrc]);
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
    MPU *mpu,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    uint16_t ad, uint16_t bw, uint16_t rdst)
{
    switch(ad)
    {
        case AM_REGISTER:
            return RB[rdst];

        case AM_INDEXED:
        {
            uint16_t x = mpu->read(RB[REG_PC]);
            if(rdst != REG_CG1)
                x += RB[rdst];

            if(bw)
                return mpu->read_byte(x);
            else
                return mpu->read(x);
        }

        default:
            // Oops
            break;
    }
    return -1;
}

static void doubleop_dest(
    MPU *mpu,
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
            uint16_t x = mpu->read(RB[REG_PC]);
            if(rdst != REG_CG1)
                x += RB[rdst];

            if(bw)
                mpu->write_byte(x, operand);
            else
                mpu->write(x, operand);
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

static size_t format2_0_cycles(uint8_t as)
{
    // Only for RRC, RRA, SWPB, SXT
    if(as == 1)
        return 1;
    if(as == 1)
        return 4;
    return 3;
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

static void extension_repeat(
    const extension_t &extension,
    extension_repeat_t &repeat,ac_regbank<16,
    msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB,
    uint8_t as,
    uint8_t ad,
    const std::string &instruction_name)
{
    repeat.zc = 0;
    repeat.al = 1;
    repeat.count = 1;

    if(extension.state == EXT_RUN)
    {
        //std::cout << "Extended " << instruction_name << std::endl;

        if(as == 0 && ad == 0)
        {
            extension_to_repeat(extension, RB, repeat.zc, repeat.al, repeat.count);
            //std::cout << " " << std::dec << repeat.count << " t imes" << std::endl;
        }
        else
            std::cerr << "Oops, extension not supported yet." << std::endl;
    }
}

// replay == true iff mpu
// replay == false otherwise
static void fire_interrupt(size_t source_id, bool replay)
{
    std::cout << "interrupt" << std::endl;

    // Push PC and SR
    context->DM.write(context->RB[REG_SP]-2, replay ? context->pc : context->RB[REG_PC]);
    context->DM.write(context->RB[REG_SP]-4, context->RB[REG_SR]);
    context->RB[REG_SP] -= 4;

    sr_flags_t sr(context->RB);
    sr.on_interrupt();

    // TODO: interrupt priority

    context->RB[REG_PC] = context->DM.read(0xff80 + (source_id << 1));
    context->ac_pc = context->RB[REG_PC];

    emanager->add_cycles(6, 0);

    context->ac_annul();
}

static void erase_memory_on_boot(ac_memory &DM)
{
    const uint8_t  sram_canary = 0xde;
    const uint8_t  periph_canary = 0xad;

    for(uint16_t i = SRAM_BEGIN; i < SRAM_END; ++i)
        DM.write_byte(i, sram_canary);

    for(uint16_t i = PERIPH_BEGIN; i < PERIPH_END; ++i)
        DM.write_byte(i, periph_canary);
}

//!Behavior executed before simulation begins.
void ac_behavior( begin )
{
    extern options_t *power_options; // Given by main.cpp

    mpu = new MPU(SRAM_BEGIN, SRAM_END, 16, DM, fire_interrupt, 39);
    platform = new platform_t(*mpu);
    supply = new PowerSupply(
        power_options->capacitance_nF,
        3.3,
        power_options->v_lo_V,
        power_options->v_hi_V,
        power_options->v_thres_V);
    emanager = new EnergyManager(elogger, *supply, *platform);
    commands = new Commands(*emanager);
    syscalls = new Syscalls(*platform, *emanager);
    syscalls->print();
    erase_memory_on_boot(DM);
    
    supply->set_infinite_energy(false);
}

//!Behavior executed after simulation ends.
void ac_behavior( end )
{
    delete syscalls;
    delete commands;
    delete emanager;
    delete supply;
    delete platform;
    delete mpu;
}

//!Generic instruction behavior method.
void ac_behavior( instruction )
{
    delete context;
    context = new arch_context_t(DM, RB, ac_pc, std::bind(&msp430x_isa::ac_annul, this));
    context->pc = ac_pc;

    extension.tick();

    emanager->log();

    //std::cout << std::endl;
    //std::cout << "pc=" << std::hex << ac_pc << std::endl;

    //std::cout << "sp=" << std::hex << RB[REG_SP] << std::endl;
    //std::cout << supply->voltage() << std::endl;
    
    switch(supply->get_state())
    {
        case ON:
            break;

        case INTERRUPT:
        {
            sr_flags_t sr(RB);
            if(sr.GIE)
            {
                fire_interrupt(60, false);
                ac_annul();
                return;
            }
            break;
        }

        default: // OFF
            supply->refill();
            emanager->stop_log();

            std::cout << "REBOOT" << std::endl;

            // Reboot
            RB[REG_PC] = DM.read(0xfffe); // Reset vector
            erase_memory_on_boot(DM);
            ac_pc = RB[REG_PC];
            ac_annul();
            return;
    }

    ac_pc += 2;
    RB[REG_PC] = ac_pc;
}
 
//! Instruction Format behavior methods.
void ac_behavior( Type_DoubleOp ) {}
void ac_behavior( Type_SimpleOp ) {}
void ac_behavior( Type_Jump ) {}
void ac_behavior( Type_PushPopM ){}
void ac_behavior( Type_Extension ){}
void ac_behavior( Type_Extended_II ){}
 
//!Instruction MOV behavior method.
void ac_behavior( MOV )
{
    uint16_t operand = doubleop_source(mpu, RB, as, bw, rsrc);

    if(rdst == REG_PC && syscalls->is_syscall(operand)) // Syscall: run symbolically
    {
        // BR instruction
        std::cout << "SYSCALL: " << syscalls->get_name(operand) << std::endl;
        doubleop_dest(mpu, RB, RB[REG_PC], ad, bw, rdst); // PC = next(PC)
        syscalls->run(operand, DM, RB);
    }
    else
        doubleop_dest(mpu, RB, operand, ad, bw, rdst);

    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, true)), 0);
}

//!Instruction ADD behavior method.
void ac_behavior( ADD )
{
    extension_repeat_t repeat;
    extension_repeat(extension, repeat, RB, as, ad, "ADD");

    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    for(size_t i = repeat.count; i--; )
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
    uint32_t promoted_result = promoted_dst;

    for(size_t i = repeat.count; i--; )
        promoted_result += promoted_src;

    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];
    extension.state = EXT_NONE;

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction ADDC behavior method.
void ac_behavior( ADDC )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_src += sr.C;
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

    uint32_t promoted_src = operand_src + sr.C;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction SUB behavior method.
void ac_behavior( SUB )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_src = ~operand_src + 1;
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

    uint32_t promoted_src = ~operand_src + 1;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction SUBC behavior method.
void ac_behavior( SUBC )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    operand_src = ~operand_src + sr.C;
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

    uint32_t promoted_src = ~operand_src + sr.C;
    uint32_t promoted_dst = operand_tmp;
    uint32_t promoted_result = promoted_src + promoted_dst;
    if(bw)
        sr.set_C(carry8(promoted_result));
    else
        sr.set_C(carry16(promoted_result));

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction CMP behavior method.
void ac_behavior( CMP )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand_dst;
    sr_flags_t sr(RB);

    sr.set_C(operand_dst >= operand_src);

    operand_src = ~operand_src+1;
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

    // Do not change the value
    doubleop_dest(mpu, RB, operand_tmp, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, true)), 0);
}

//!Instruction DADD behavior method.
void ac_behavior( DADD )
{
    std::cerr << "Oops (DADD)" << std::endl;
}

//!Instruction BIT behavior method.
void ac_behavior( BIT )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
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
    doubleop_dest(mpu, RB, tmp, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, true)), 0);
}

//!Instruction BIC behavior method.
void ac_behavior( BIC )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);

    operand_dst &= ~operand_src;

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction BIS behavior method.
void ac_behavior( BIS )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);

    operand_dst |= operand_src;

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction XOR behavior method.
void ac_behavior( XOR )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
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

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction AND behavior method.
void ac_behavior( AND )
{
    uint16_t operand_src = doubleop_source(mpu, RB, as, bw, rsrc);
    uint16_t operand_dst = doubleop_dest_operand(mpu, RB, ad, bw, rdst);
    sr_flags_t sr(RB);

    operand_dst &= operand_src;

    sr.set_Z(operand_dst == 0);
    sr.set_C(operand_dst != 0);
    sr.set_V(0);
    if(bw)
        sr.set_N(negative8(operand_dst));
    else
        sr.set_N(negative16(operand_dst));

    doubleop_dest(mpu, RB, operand_dst, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(doubleop_cycles(as, ad, rsrc, rdst, false)), 0);
}

//!Instruction RRC behavior method.
void ac_behavior( RRC )
{
    uint16_t operand = doubleop_source(mpu, RB, ad, bw, rdst);
    uint16_t operand_tmp = operand;
    sr_flags_t sr(RB);

    operand >>= 1;
    if(bw)
    {
        operand |= (sr.C ? 0xff80 : 0x0);
        sr.set_N(negative8(operand));
    }
    else
    {
        operand |= (sr.C << 15);
        sr.set_N(negative16(operand));
    }

    sr.set_V(0);
    sr.set_Z(operand == 0);
    sr.set_C(operand_tmp & 1);

    doubleop_dest(mpu, RB, operand, ad, bw, rdst);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(format2_0_cycles(ad)), 0);
}

//!Instruction RRA behavior method.
void ac_behavior( RRA )
{
    extension_repeat_t repeat;
    if(extension.state == EXT_RUN)
    {
        //std::cout << "Extended RRA" << std::endl;

        if(ad == 0)
        {
            extension_to_repeat(extension, RB, repeat.zc, repeat.al, repeat.count);
            //std::cout << " " << std::dec << repeat.count << " times" << std::endl;
        }
        else
            std::cerr << "Oops, extension not supported yet." << std::endl;
    }

    uint16_t operand = doubleop_source(mpu, RB, ad, bw, rdst);
    sr_flags_t sr(RB);

    for(size_t i = repeat.count; i--; )
    {
        sr.set_C(operand & 1);
        operand = ((operand & 0x8000) | (operand >> 1));
    }

    if(bw)
        sr.set_N(negative8(operand));
    else
        sr.set_N(negative16(operand));

    sr.set_V(0);
    sr.set_Z(operand == 0);

    doubleop_dest(mpu, RB, operand, ad, bw, rdst);
    ac_pc = RB[REG_PC];
    extension.state = EXT_NONE;

    emanager->add_cycles(ESTIMATE_PIPELINE(format2_0_cycles(ad)), 0);
}

//!Instruction PUSH behavior method.
void ac_behavior( PUSH )
{
    uint16_t operand = doubleop_source(mpu, RB, ad, bw, rdst);
    RB[REG_SP] -= 2;
    if(bw)
        mpu->write_byte(RB[REG_SP], operand);
    else
        mpu->write(RB[REG_SP], operand);
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(ad == AM_REGISTER ? 3 : 4), 0);
}

//!Instruction SWPB behavior method.
void ac_behavior( SWPB )
{
    std::cerr << "Oops (SWPB)" << std::endl;
}

//!Instruction CALL behavior method.
void ac_behavior( CALL )
{
    uint16_t address = doubleop_source(mpu, RB, ad, 0, rdst);
    if(commands->run(address)) // Program issued a simulation command
    {
        ac_pc = RB[REG_PC];
        return;
    }
    else if(syscalls->is_syscall(address)) // Syscall: run symbolically
    {
        std::cout << "SYSCALL: " << syscalls->get_name(address) << std::endl;
        syscalls->run(address, DM, RB);
    }
    else // Actually run the call
    {
        RB[REG_SP] -= 2;
        DM.write(RB[REG_SP], RB[REG_PC]);
        RB[REG_PC] = address;
    }

    ac_pc = RB[REG_PC];

    if(ad == AM_REGISTER)
        emanager->add_cycles(ESTIMATE_PIPELINE(4), 0);
    else if(rdst == REG_SR)
        emanager->add_cycles(ESTIMATE_PIPELINE(6), 0);
    else
        emanager->add_cycles(ESTIMATE_PIPELINE(5), 0);
}

//!Instruction RETI behavior method.
void ac_behavior( RETI )
{
    // Pop SR and PC
    RB[REG_SR] = DM.read(RB[REG_SP]);
    RB[REG_PC] = DM.read(RB[REG_SP] + 2);
    RB[REG_SP] += 4;

    ac_pc = RB[REG_PC];
    emanager->add_cycles(ESTIMATE_PIPELINE(5), 0);
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

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
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

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
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

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
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

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
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

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
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

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
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

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
}

//!Instruction JMP behavior method.
void ac_behavior( JMP )
{
    int16_t signed_offset = 2 * u10_to_i16(offset);
    uint16_t address = RB[REG_PC] + signed_offset;
    if(syscalls->is_syscall(address)) // Syscall: run symbolically
    {
        std::cout << "SYSCALL: " << syscalls->get_name(address) << std::endl;
        syscalls->run(address, DM, RB);
    }
    else // Actually run the call
        RB[REG_PC] = address;
    ac_pc = RB[REG_PC];

    emanager->add_cycles(ESTIMATE_PIPELINE(2), 0);
}

//!Instruction PUSHPOPM behavior method.
void ac_behavior( PUSHPOPM )
{
    uint16_t n = 1 + n1;
    uint16_t rdst = rdst1;

    if(!word)
        std::cerr << "PUSHPOPM: address mode not supported." << std::endl;
    /*
    std::cout << "PUSHPOP:" << std::endl
              << " n=" << std::dec << n << std::endl
              << " rdst=" << rdst << std::endl
              << " before: SP=" << std::hex << RB[REG_SP] << std::endl;

    */

    if(!subop) // PUSHM
    {
        //std::cout << " It's a pushm!" << std::endl;
        for(; n--; --rdst)
        {
            //std::cout << "  r" << std::dec << rdst << std::endl;
            RB[REG_SP] -= 2;
            mpu->write(RB[REG_SP], RB[rdst]);
        }
    }
    else // POPM
    {
        //std::cout << " It's a popm!" << std::endl;
        for(; n--; ++rdst)
        {
            //std::cout << "  r" << std::dec << rdst << std::endl;
            RB[rdst] = mpu->read(RB[REG_SP]);
            RB[REG_SP] += 2;
        }
    }
    ac_pc = RB[REG_PC];

    /*
    std::cout << " after: SP=" << std::hex << RB[REG_SP] << std::endl
              << std::endl;
    */

    emanager->add_cycles(ESTIMATE_PIPELINE(3 + n1), 0);
}

//!Instruction EXT behavior method.
void ac_behavior( EXT )
{
    extension.payload_h = payload_h;
    extension.payload_l = payload_l;
    extension.al        = al;

    //std::cout << "Extension!" << std::endl;
    if(extension.state != EXT_NONE)
        std::cerr << "Bad extension state (Oops)" << std::endl;
    if(!al)
        std::cerr << "20-bit words not supported (Oops)" << std::endl;

    extension.state = EXT_RDY;

}

//!Instruction RRCM behavior method.
void ac_behavior( RRCM )
{
    std::cerr << "Oops (RRCM)" << std::endl;
}

//!Instruction RRAM behavior method.
void ac_behavior( RRAM )
{
    std::cerr << "Oops (RRAM)" << std::endl;
}

//!Instruction RRUM behavior method.
void ac_behavior( RRUM )
{
    if(!w)
        std::cerr << "Oops (RRUM.A)" << std::endl;

    size_t n = n_1+1;
    uint16_t operand = RB[dst];
    sr_flags_t sr(RB);

    sr.set_C(operand & (1 << n_1));
    operand >>= n;

    sr.set_N(negative16(operand));
    sr.set_V(0);
    sr.set_Z(operand == 0);

    RB[dst] = operand;

    emanager->add_cycles(ESTIMATE_PIPELINE(n), 0);
}

//!Instruction RLAM behavior method.
void ac_behavior( RLAM )
{
    std::cerr << "Oops (RLAM)" << std::endl;
}

