#include <iostream>

#include "syscalls.h"

#define REG_FIRST_PARAM  12
#define REG_SECOND_PARAM 13
#define REG_THIRD_PARAM  14
#define REG_FOURTH_PARAM 15

const float MCLK_FREQ = 24; // MHz

static const elf_wl_functions_t whitelist{
    // Leds
    "leds_init",
    "led_on",
    "leds_on",
    "leds_off",

    // Port
    "prt_drv_init_hw",

    // Spi
    "spi_init_hw",

    // DMA
    "dma_memset",
    "dma_memcpy",

    // Clock
    "clk_delay_micro",

    // cc2500
    "cc2500_init_hw",
    "cc2500_drv_restore_hw",
    "cc2500_configure_hw",
    "cc2500_idle_hw",
    "cc2500_sleep_hw",
    "cc2500_wakeup_hw",
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
    else if(name == "prt_drv_init_hw")
        port_init();
    else if(name == "spi_init_hw")
        spi_init();
    else if(name == "dma_memset")
        dma_memset(DM, RB[REG_FIRST_PARAM], RB[REG_SECOND_PARAM], RB[REG_THIRD_PARAM]);
    else if(name == "dma_memcpy")
        dma_memcpy(DM, RB[REG_FIRST_PARAM], RB[REG_SECOND_PARAM], RB[REG_THIRD_PARAM]);
    else if(name == "clk_delay_micro")
    {
        // TODO
    }
    else if(name == "cc2500_init_hw")
        cc2500_init();
    else if(name == "cc2500_drv_restore_hw")
    {
        // TODO
    }
    else if(name == "cc2500_configure_hw")
        cc2500_configure();
    else if(name == "cc2500_idle_hw")
        cc2500_idle();
    else if(name == "cc2500_sleep_hw")
        cc2500_sleep();
    else if(name == "cc2500_wakeup_hw")
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
    emanager.transaction(
        451,
        10, // 10.318
        platform.cc2500.current());
    platform.cc2500.init();
}

void Syscalls::cc2500_configure()
{
    emanager.transaction(
        575,
        5, // 4.459
        platform.cc2500.current());
    // TODO
}

void Syscalls::cc2500_idle()
{
    if(platform.cc2500.is_sleep())
    {
        emanager.transaction(
            399,
            3, // 3.088
            platform.cc2500.current());
    }
    else // RX
    {
        emanager.transaction(
            112,
            6, // 6.015
            platform.cc2500.current());
    }
    platform.cc2500.idle();
}

void Syscalls::cc2500_sleep()
{
    if(platform.cc2500.is_idle())
    {
        emanager.transaction(
            25,
            0, // 0.168
            platform.cc2500.current());
    }
    else // RX
    {
        emanager.transaction(
            25,
            1, // 1.088
            platform.cc2500.current());
    }
    platform.cc2500.sleep();
}

void Syscalls::cc2500_wakeup()
{
    emanager.transaction(
        399,
        3, // 3.088
        platform.cc2500.current());
    platform.cc2500.wakeup();
}

void Syscalls::cc2500_send_packet(const uint8_t *buf, size_t size)
{
    platform.cc2500.send_packet(buf, size);

    size_t duration = (size < 64 ?
        1263.170 + 38.080 * size:
        1646.238 + 32.014 * size);
    emanager.transaction(
        duration,
        48 + 2.393 * size,
        platform.cc2500.current());
}

void Syscalls::dma_memset(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    uint16_t dst, uint8_t val, uint16_t len
)
{
    if(!len)
        return;

    for(uint16_t i = len; i--;)
        DM.write_byte(dst++, val);
    emanager.transaction((1 + 2*len) / MCLK_FREQ, 0, 0);
}

void Syscalls::dma_memcpy(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    uint16_t dst, uint16_t src, uint16_t len
)
{
    if(!len)
        return;

    for(uint16_t i = len; i--;)
        DM.write_byte(dst++, DM.read_byte(src++));
    emanager.transaction((1 + 2*len) / MCLK_FREQ, 0, 0);
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

