#include <cmath>

#include "power_supply.h"

PowerSupply::PowerSupply(double capacitance, double supply_vcc, double v_lo_threshold, double v_hi_threshold, double v_interrupt_threshold):
    capacitance(capacitance),
    supply_vcc(supply_vcc),
    e_lo_threshold(capacitance * v_lo_threshold * v_lo_threshold / 2.),
    e_hi_threshold(capacitance * v_hi_threshold * v_hi_threshold / 2.),
    e_interrupt_threshold(capacitance * v_interrupt_threshold * v_interrupt_threshold / 2.),
    state(ON) // Start on
{
    refill(); // Start full
    // TODO check threshold comparison
}

void PowerSupply::add_energy(double e)
{
    energy += e;
    update_state();

    // TODO clamp energy between 0 and e_max
}

void PowerSupply::refill()
{
    energy = e_hi_threshold;
    state = ON;
}

double PowerSupply::voltage() const
{
    return sqrt(2. * energy / capacitance);
}

double PowerSupply::vcc() const
{
    return supply_vcc;
}

power_supply_state_e PowerSupply::get_state() const
{
    return state;
}

void PowerSupply::set_lo_threshold(double v_threshold)
{
    e_lo_threshold = capacitance * v_threshold * v_threshold / 2.;
    // TODO check threshold comparison
}

void PowerSupply::set_hi_threshold(double v_threshold)
{
    e_hi_threshold = capacitance * v_threshold * v_threshold / 2.;
    // TODO check threshold comparison
}

void PowerSupply::update_state()
{
    switch(state)
    {
        case ON:
            if(energy <= e_lo_threshold)
                state = OFF;
            else if(energy <= e_interrupt_threshold)
                state = INTERRUPT;
            break;

        case INTERRUPT:
            if(energy <= e_lo_threshold)
                state = OFF;
            break;

        default: // OFF
            if(energy >= e_hi_threshold)
            {
                state = ON;
                // TODO boot
            }
    }
}

