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
        PowerSupply(double capacitance, double supply_vcc, double v_lo_threshold, double v_hi_threshold, double v_interrupt_threshold);

        void add_energy(double e);
        void refill();

        double voltage() const;
        double vcc() const;
        power_supply_state_e get_state() const;

        void set_lo_threshold(double v_threshold);
        void set_hi_threshold(double v_threshold);

    private:
        void update_state();

        double capacitance;
        double supply_vcc;
        double e_lo_threshold;
        double e_hi_threshold;
        double e_interrupt_threshold;
        double energy;
        power_supply_state_e state;
};

#endif

