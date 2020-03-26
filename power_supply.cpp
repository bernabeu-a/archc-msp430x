#include <iostream>
#include <cmath>

#include "power_supply.h"

PowerSupply::PowerSupply(double capacitance_nF, double supply_vcc_V, double v_lo_threshold_V, double v_hi_threshold_V, double v_interrupt_threshold_V):
    capacitance_nF(capacitance_nF),
    supply_vcc_V(supply_vcc_V),
    e_interrupt_threshold_nJ(capacitance_nF * v_interrupt_threshold_V * v_interrupt_threshold_V / 2.),
    state(ON) // Start on
{
    set_lo_threshold(v_lo_threshold_V);
    set_hi_threshold(v_hi_threshold_V);

    std::cout << "# Power supply" << std::endl
              << std::dec << "  total      : " << (e_hi_threshold_nJ - e_lo_threshold_nJ) << " nJ" << std::endl
              <<             "  until chkpt: " << (e_hi_threshold_nJ - e_interrupt_threshold_nJ) << " nJ" << std::endl << std::endl;

    refill(); // Start full
    // TODO check threshold comparison
}

void PowerSupply::add_energy(double e_nJ)
{
    if(!infinite_energy)
    {
        energy_nJ += e_nJ;
        update_state();

        if(energy_nJ < e_lo_threshold_nJ)
            energy_nJ = e_lo_threshold_nJ - 1;

        // TODO clamp energy between 0 and e_max
    }
}

void PowerSupply::refill()
{
    energy_nJ = e_hi_threshold_nJ;
    state = ON;
}

double PowerSupply::voltage() const
{
    return sqrt(2. * energy_nJ / capacitance_nF);
}

double PowerSupply::vcc() const
{
    return supply_vcc_V;
}

power_supply_state_e PowerSupply::get_state() const
{
    return state;
}

void PowerSupply::set_lo_threshold(double v_threshold_V)
{
    e_lo_threshold_nJ = capacitance_nF * v_threshold_V * v_threshold_V / 2.;
    // TODO check threshold comparison
}

void PowerSupply::set_hi_threshold(double v_threshold_V)
{
    e_hi_threshold_nJ = capacitance_nF * v_threshold_V * v_threshold_V / 2.;
    // TODO check threshold comparison
}

void PowerSupply::set_infinite_energy(bool i_energy)
{
    infinite_energy = i_energy;
}

void PowerSupply::update_state()
{
    switch(state)
    {
        case ON:
            if(energy_nJ <= e_lo_threshold_nJ)
                state = OFF;
            else if(energy_nJ <= e_interrupt_threshold_nJ)
                state = INTERRUPT;
            break;

        case INTERRUPT:
            if(energy_nJ <= e_lo_threshold_nJ)
                state = OFF;
            break;

        default: // OFF
            if(energy_nJ >= e_hi_threshold_nJ)
            {
                state = ON;
                // TODO boot
            }
    }
}

void PowerSupply::force_reboot()
{
    state = OFF;
}

