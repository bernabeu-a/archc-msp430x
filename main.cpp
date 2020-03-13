const char *project_name="msp430x";
const char *project_file="msp430x.ac";
const char *archc_version="2.4.1";
const char *archc_options="-abi ";

#include  <iostream>
#include  <systemc.h>
#include  "ac_stats_base.H"
#include  "msp430x.H"

#include "options.h"

// Completes the namespace defined in msp430_parms.H
namespace msp430x_parms
{
    options_t *power_options;
}

int sc_main(int ac, char *av[])
{
    msp430x_parms::power_options = new options_t(ac, av);
    if(msp430x_parms::power_options->failed)
    {
        delete msp430x_parms::power_options;
        return EXIT_FAILURE;
    }

    //!  ISA simulator
    msp430x msp430x_proc1("msp430x");

    msp430x_proc1.init(
        ac - msp430x_parms::power_options->arg_offset,
        av + msp430x_parms::power_options->arg_offset
    );
    msp430x_proc1.set_prog_args();
    cerr << endl;

    sc_start();

    msp430x_proc1.PrintStat();
    cerr << endl;

#ifdef AC_STATS
    //ac_stats_base::print_all_stats(std::cerr);
#endif 

    delete msp430x_parms::power_options;

    return msp430x_proc1.ac_exit_status;
}
