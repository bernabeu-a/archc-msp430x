#ifndef POWER_SUPPLY
#define POWER_SUPPLY

enum power_supply_state_e
{
    ON,
    OFF,
    INTERRUPT
};

class PowerSupply
{
    public:
        PowerSupply(double capacitance_nF, double supply_vcc_V, double v_lo_threshold_V, double v_hi_threshold_V, double v_interrupt_threshold_V);

        void add_energy(double e_nJ);
        void refill();

        double voltage() const;
        double vcc() const;
        power_supply_state_e get_state() const;

        void set_lo_threshold(double v_threshold);
        void set_hi_threshold(double v_threshold);
        void set_infinite_energy(bool infinite_energy);

    private:
        void update_state();

        double capacitance_nF;
        double supply_vcc_V;
        double e_lo_threshold_nJ;
        double e_hi_threshold_nJ;
        double e_interrupt_threshold_nJ;
        double energy_nJ;
        power_supply_state_e state;
        bool infinite_energy;
};

#endif

