#include <iostream>

#include "syscalls.h"

#define REG_FIRST_PARAM  12
#define REG_SECOND_PARAM 13
#define REG_THIRD_PARAM  14
#define REG_FOURTH_PARAM 15

const float VCC = 3.3; // V
const float Icpu_active = 1864e-6; // A
const float Pcpu_active = VCC * Icpu_active;
const float MCLK_FREQ = 24; // MHz

static const elf_wl_functions_t whitelist{
    // Leds
    "leds_init",
    "led_on",
    "leds_on",
    "leds_off",

    // Port
    "prt_drv_init",

    // Spi
    "spi_init",

    // DMA
    "dma_memset",
    "dma_memcpy",

    // Clock
    "clk_delay_micro",

    // cc2500
    "cc2500_init",
    "cc2500_configure",
    "cc2500_idle",
    "cc2500_sleep",
    "cc2500_wakeup",
    "cc2500_send_packet",

    // Energy
    "energy_init",
    "energy_reduce_consumption",
    "energy_start_measurements",
    "energy_stop_measurements"
};

Syscalls::Syscalls(platform_t &platform, EnergyManager &emanager):
    functions(read_functions_from_args(0, nullptr, whitelist)),
    platform(platform),
    emanager(emanager)
{
}

void Syscalls::print() const
{
    for(const auto &it : functions)
        std::cout << std::hex << it.first << " -> " << it.second << std::endl;
}

bool Syscalls::is_syscall(uint16_t address) const
{
    return functions.find(address) != functions.cend();
}

std::string Syscalls::get_name(uint16_t address) const
{
    return functions.find(address)->second;
}

void Syscalls::run(
    uint16_t address,
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    ac_regbank<16, msp430x_parms::ac_word, msp430x_parms::ac_Dword>& RB
)
{
    std::string name = get_name(address);
    if(name == "leds_init")
        leds_init();
    else if(name == "leds_off")
        leds_off();
    else if(name == "leds_on")
        leds_on();
    else if(name == "led_on")
        led_on(RB[REG_FIRST_PARAM]);
    else if(name == "prt_drv_init")
        port_init();
    else if(name == "spi_init")
        spi_init();
    else if(name == "dma_memset")
        dma_memset(DM, RB[REG_FIRST_PARAM], RB[REG_SECOND_PARAM], RB[REG_THIRD_PARAM]);
    else if(name == "dma_memcpy")
        dma_memcpy(DM, RB[REG_FIRST_PARAM], RB[REG_SECOND_PARAM], RB[REG_THIRD_PARAM]);
    else if(name == "clk_delay_micro")
    {
        // TODO
    }
    else if(name == "cc2500_init")
        cc2500_init();
    else if(name == "cc2500_configure")
        cc2500_configure();
    else if(name == "cc2500_idle")
        cc2500_idle();
    else if(name == "cc2500_sleep")
        cc2500_sleep();
    else if(name == "cc2500_wakeup")
        cc2500_wakeup();
    else if(name == "cc2500_send_packet")
    {
        uint16_t ptr = RB[REG_FIRST_PARAM];
        uint16_t size = RB[REG_SECOND_PARAM];

        std::vector<uint8_t> buf(size);
        for(size_t i = 0; i < size; ++i)
            buf[i] = DM.read_byte(ptr++);

        cc2500_send_packet(buf.data(), size);
    }
    else if(name == "energy_init")
        energy_init();
    else if(name == "energy_reduce_consumption")
        energy_reduce_consumption();
    else if(name == "energy_start_measurements")
        energy_start_measurements();
    else if(name == "energy_stop_measurements")
        energy_stop_measurements();
    else
    {
        std::cerr << "Oops: Unsupported syscall \"" << name << "\"" << std::endl;
    }
}

void Syscalls::leds_init()
{
    platform.leds.init();
}

void Syscalls::leds_off()
{
    platform.leds.off();
}

void Syscalls::leds_on()
{
    platform.leds.on();
}

void Syscalls::led_on(uint8_t n)
{
    platform.leds.on(n);
}

void Syscalls::port_init()
{
    platform.port.init();
}

void Syscalls::spi_init()
{
    platform.spi.init();
}

void Syscalls::cc2500_init()
{
    const size_t duration = 225;
    emanager.transaction(duration, 13 - duration * Pcpu_active);
    platform.cc2500.init();
}

void Syscalls::cc2500_configure()
{
    const size_t duration = 573;
    emanager.transaction(duration, 7 - duration * Pcpu_active);
    // TODO
}

void Syscalls::cc2500_idle()
{
    const size_t duration = 110;
    emanager.transaction(110, 7 - duration * Pcpu_active);
    platform.cc2500.idle();
}

void Syscalls::cc2500_sleep()
{
    emanager.transaction(20, 0);
    platform.cc2500.sleep();
}

void Syscalls::cc2500_wakeup()
{
    const size_t duration = 395;
    emanager.transaction(395, 5 - duration * Pcpu_active);
    platform.cc2500.wakeup();
}

void Syscalls::cc2500_send_packet(const uint8_t *buf, size_t size)
{
    platform.cc2500.send_packet(buf, size);

    size_t duration;
    if(size < 64)
        duration = 1264.1 + 37.557 * size;
    else
        duration = 1690.9 + 30.794 * size;
    emanager.transaction(duration, 50 + 2.477 * size - duration * Pcpu_active);
}

void Syscalls::dma_memset(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    uint16_t dst, uint8_t val, uint16_t len
)
{
    for(uint16_t i = len; i--;)
        DM.write_byte(dst++, val);
    emanager.transaction((1 + 2*len) / MCLK_FREQ, 0);
}

void Syscalls::dma_memcpy(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    uint16_t dst, uint16_t src, uint16_t len
)
{
    for(uint16_t i = len; i--;)
        DM.write_byte(dst++, DM.read_byte(src++));
    emanager.transaction((1 + 2*len) / MCLK_FREQ, 0);
}

void Syscalls::energy_init()
{
    platform.energy.init();
}

void Syscalls::energy_reduce_consumption()
{
}

void Syscalls::energy_start_measurements()
{
    platform.energy.start();
    emanager.start_log();
}

void Syscalls::energy_stop_measurements()
{
    platform.energy.stop();
    emanager.stop_log();
}

