#include <iostream>

#include "syscalls.h"

#define REG_FIRST_PARAM  12
#define REG_SECOND_PARAM 13
#define REG_THIRD_PARAM  14
#define REG_FOURTH_PARAM 15
#define REG_RETURN       12

const float MCLK_FREQ = 24; // MHz

static const elf_wl_functions_t whitelist{
    // Leds
    "leds_init",

    // Port
    "prt_drv_init_hw",
    "prt_out_bic",
    "prt_out_bis",

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

    // Temperature
    "tmp_drv_init_hw",
    "tmp_drv_sample",

    // Accelerometer
    "accelerometer_init",
    "accelerometer_on",
    "accelerometer_off",
    "accelerometer_measure",

    // ADC
    "adc_measure_vcap",

    // Energy
    "energy_init",
    "energy_reduce_consumption",
    "energy_start_measurements",
    "energy_stop_measurements",

    // MPU
    "mpu_hw_block",
    "mpu_hw_unblock",

    "syt_shutdown_platform"
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
    else if(name == "prt_drv_init_hw")
        port_init();
    else if(name == "prt_out_bic")
        port_out_bic(RB[REG_FIRST_PARAM], RB[REG_SECOND_PARAM]);
    else if(name == "prt_out_bis")
        port_out_bis(RB[REG_FIRST_PARAM], RB[REG_SECOND_PARAM]);
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
    else if(name == "tmp_drv_init_hw")
        temperature_init();
    else if(name == "tmp_drv_sample")
        RB[REG_RETURN] = temperature_sample();
    else if(name == "accelerometer_init")
        accelerometer_init();
    else if(name == "accelerometer_on")
        accelerometer_on();
    else if(name == "accelerometer_off")
        accelerometer_off();
    else if(name == "accelerometer_measure")
    {
        uint16_t ptr = RB[REG_FIRST_PARAM];
        Accelerometer::acquisition_t data;
        accelerometer_measure(data);
        DM.write(ptr + 0, data.x);
        DM.write(ptr + 2, data.y);
        DM.write(ptr + 4, data.z);
    }
    else if(name == "adc_measure_vcap")
        RB[REG_RETURN] = adc_measure_vcap();
    else if(name == "energy_init")
        energy_init();
    else if(name == "energy_reduce_consumption")
        energy_reduce_consumption();
    else if(name == "energy_start_measurements")
        energy_start_measurements();
    else if(name == "energy_stop_measurements")
        energy_stop_measurements();
    else if(name == "mpu_hw_block")
    {
        uint16_t blockid = RB[REG_FIRST_PARAM];
        mpu_block(blockid);
    }
    else if(name == "mpu_hw_unblock")
    {
        uint16_t blockid = RB[REG_FIRST_PARAM];
        mpu_unblock(blockid);
    }
    else if(name == "syt_shutdown_platform")
    {
        emanager.force_reboot();
    }
    else
    {
        std::cerr << "Oops: Unsupported syscall \"" << name << "\"" << std::endl;
    }
}

void Syscalls::leds_init()
{
    platform.leds.init();
}

void Syscalls::port_init()
{
    platform.port.init();
}

void Syscalls::port_out_bic(uint8_t port, uint8_t mask)
{
    if(port == 5)
    {
        // LEDS O~3
        for(size_t i = 0; i < 4; ++i)
            if(mask & (1 << i))
                platform.leds.off(i);
    }
    else if(port == 3)
    {
        // LEDS 4~7
        for(size_t i = 4; i < 8; ++i)
            if(mask & (1 << i))
                platform.leds.off(i);
    }
}

void Syscalls::port_out_bis(uint8_t port, uint8_t mask)
{
    if(port == 5)
    {
        // LEDS O~3
        for(size_t i = 0; i < 4; ++i)
            if(mask & (1 << i))
                platform.leds.on(i);
    }
    else if(port == 3)
    {
        // LEDS 4~7
        for(size_t i = 4; i < 8; ++i)
            if(mask & (1 << i))
                platform.leds.on(i);
    }
}

void Syscalls::spi_init()
{
    platform.spi.init();
}

void Syscalls::cc2500_init()
{
    emanager.transaction(
        451,
        10318,
        platform.cc2500.current_ua());
    platform.cc2500.init();
}

void Syscalls::cc2500_configure()
{
    emanager.transaction(
        575,
        4459,
        platform.cc2500.current_ua());
    // TODO
}

void Syscalls::cc2500_idle()
{
    if(platform.cc2500.is_sleep())
    {
        emanager.transaction(
            399,
            3088,
            platform.cc2500.current_ua());
    }
    else // RX
    {
        emanager.transaction(
            112,
            6015,
            platform.cc2500.current_ua());
    }
    platform.cc2500.idle();
}

void Syscalls::cc2500_sleep()
{
    if(platform.cc2500.is_idle())
    {
        emanager.transaction(
            25,
            168,
            platform.cc2500.current_ua());
    }
    else // RX
    {
        emanager.transaction(
            25,
            1088,
            platform.cc2500.current_ua());
    }
    platform.cc2500.sleep();
}

void Syscalls::cc2500_wakeup()
{
    emanager.transaction(
        399,
        3088,
        platform.cc2500.current_ua());
    platform.cc2500.wakeup();
}

void Syscalls::cc2500_send_packet(const uint8_t *buf, size_t size)
{
    size_t duration = (size < 64 ?
        1263.170 + 38.080 * size:
        1646.238 + 32.014 * size);
    emanager.transaction(
        duration,
        48000 + 2393 * size,
        platform.cc2500.current_ua());

    platform.cc2500.send_packet(buf, size);
}

void Syscalls::temperature_init()
{
    emanager.transaction(
        159,
        0,
        platform.temperature.current_ua());
    platform.temperature.init();
}

uint16_t Syscalls::temperature_sample()
{
    emanager.transaction(
        76,
        200,
        platform.temperature.current_ua());
    return platform.temperature.sample();
}

void Syscalls::accelerometer_init()
{
    emanager.transaction(
        55,
        0,
        platform.accelerometer.current_ua());
    platform.accelerometer.init();
}

void Syscalls::accelerometer_on()
{
    emanager.transaction(
        1025,
        46000,
        platform.accelerometer.current_ua());
    platform.accelerometer.on();
}

void Syscalls::accelerometer_off()
{
    emanager.transaction(
        10,
        0,
        platform.accelerometer.current_ua());
    platform.accelerometer.off();
}

void Syscalls::accelerometer_measure(Accelerometer::acquisition_t &data)
{
    emanager.transaction(
        171,
        0,
        platform.accelerometer.current_ua());
    platform.accelerometer.measure(data);
}

void Syscalls::dma_memset(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    uint16_t dst, uint8_t val, uint16_t len
)
{
    if(!len)
        return;

    std::cout << std::hex << "memset( " << dst << ", " << val << ", " << len << ")" << std::endl;

    for(uint16_t i = len; i--;)
        platform.mpu.write_byte(dst++, val);
    emanager.transaction((1 + 2*len) / MCLK_FREQ, 0, 0);
}

void Syscalls::dma_memcpy(
    ac_memport<msp430x_parms::ac_word, msp430x_parms::ac_Hword>& DM,
    uint16_t dst, uint16_t src, uint16_t len
)
{
    if(!len)
        return;

    std::cout << std::hex << "memcpy( " << dst << ", " << src << ", " << len << ")" << std::endl;

    for(uint16_t i = len; i--;)
        platform.mpu.write_byte(dst++, DM.read_byte(src++));
    emanager.transaction((1 + 2*len) / MCLK_FREQ, 0, 0);
}

uint16_t Syscalls::adc_measure_vcap()
{
    const double VREFM_V = 0.;
    const double VREFP_V = 2.;
    const double VMAX_V = 3.67;
    const double K = VREFP_V / VMAX_V;
    const size_t ADC_RESOLUTION = 10;

    double vcap_v = platform.supply.voltage();
    double vin_v = vcap_v * K;

    uint16_t n_adc = ((1 << ADC_RESOLUTION) - 1) * (vin_v - VREFM_V) / (VREFP_V - VREFM_V);
    std::cout << "ADC(VCAP) = 0x" << std::hex << n_adc << std::endl;

    return n_adc;
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
    emanager.stop_log();
    platform.energy.stop();
}

void Syscalls::mpu_block(uint16_t blockid)
{
    std::cout << "BLOCK(" << std::dec << blockid << ")" << std::endl;
    platform.mpu.block(blockid);
}

void Syscalls::mpu_unblock(uint16_t blockid)
{
    std::cout << "UNBLOCK(" << std::dec << blockid << ")" << std::endl;
    platform.mpu.unblock(blockid);
}

