#ifndef OPTIONS_H
#define OPTIONS_H

#include <string>
#include <sstream>
#include <cmath>

struct options_t
{
    float capacitance_nF;
    float v_hi_V;
    float v_lo_V;
    float v_thres_V;
    bool failed;
    size_t arg_offset;

    void from_voltage(float hi_V, float lo_V, float thres_V, float C_uF)
    {
        v_hi_V = hi_V;
        v_lo_V = lo_V;
        v_thres_V = thres_V;
        capacitance_nF = C_uF * 1e3;
    }

    void from_energy(float e_progress_uJ, float e_chkpt_uJ, float lo_V, float C_uF)
    {
        float half_cap_uF = C_uF / 2.;
        float e_lo_uJ = half_cap_uF * lo_V * lo_V;
        float e_total_uJ = e_progress_uJ + e_chkpt_uJ;

        v_hi_V = sqrt(e_total_uJ / half_cap_uF + lo_V * lo_V);
        v_lo_V = lo_V;
        v_thres_V = sqrt(e_chkpt_uJ / half_cap_uF + lo_V * lo_V);
        capacitance_nF = C_uF * 1e3;
    }

    void from_default()
    {
        capacitance_nF = 100e3;
        v_hi_V = 4.;
        v_lo_V = 3.5;
        v_thres_V = 3.6;
    }

    void recap() const
    {
        std::cout << "# Options" << std::endl
                  << "  cap     (uF): " << (capacitance_nF * 1e-3) << std::endl
                  << "  v_hi    (V) : " << v_hi_V << std::endl
                  << "  v_lo    (V) : " << v_lo_V << std::endl
                  << "  v_thres (V) : " << v_thres_V << std::endl
                  << std::endl;
    }

    options_t(int argc, char **argv)
    {
        failed = false;

        if(argc < 2)
        {
            usage(argv[0]);
            failed = true;
        }
        else
        {
            const std::string ARG_ARCHC("--");
            const std::string ARG_ENERGY("e");
            const std::string ARG_VOLTAGE("v");

            std::string command(argv[1]);
            if(command == ARG_ARCHC)
            {
                // (1) ./msp430x.x -- kernel

                if(argc < 3)
                {
                    usage(argv[0]);
                    failed = true;
                }
                else
                {
                    from_default();
                    arg_offset = 0;
                }
            }
            else if(command == ARG_VOLTAGE)
            {
                  // (2) ./msp430x.x v hi_V lo_V thres_V C_uF -- kernel

                if(argc < 8 || std::string(argv[6]) != ARG_ARCHC)
                {
                    usage(argv[0]);
                    failed = true;
                }
                else
                {
                    float hi_V = str2float(argv[2]);
                    float lo_V = str2float(argv[3]);
                    float thres_V = str2float(argv[4]);
                    float C_uF = str2float(argv[5]);
                    from_voltage(hi_V, lo_V, thres_V, C_uF);
                    arg_offset = 5;
                }
            }
            else if(command == ARG_ENERGY)
            {
                // (3) ./msp430x.x e progress_uJ chkpt_uJ lo_V C_uF -- kernel
                float progress_uJ = str2float(argv[2]);
                float chkpt_uJ = str2float(argv[3]);
                float lo_V = str2float(argv[4]);
                float C_uF = str2float(argv[5]);
                from_energy(progress_uJ, chkpt_uJ, lo_V, C_uF);
                arg_offset = 5;
            }
            else
            {
                usage(argv[0]);
                failed = true;
            }
        }

        if(!failed)
            recap();
    }

    static void usage(const std::string &invocation)
    {
        std::cout << "(1) " << invocation << " -- kernel" << std::endl
                  << "(2) " << invocation << " v hi_V lo_V thres_V C_uF -- kernel" << std::endl
                  << "      - hi_V   : charge up to voltage (V)" << std::endl
                  << "      - lo_V   : discharge down to voltage (V)" << std::endl
                  << "      - thres_V: low-voltage interrupt voltage (V)" << std::endl
                  << "      - C_uF   : platform capacitance (uF)" << std::endl
                  << "      [Constraints] lo_V < thres_V < hi_V" << std::endl
                  << "(3) " << invocation << " e progress_uJ chkpt_uJ lo_V C_uF -- kernel" << std::endl
                  << "      - progress_uJ: available energy before low-voltage interrupt (uJ)" << std::endl
                  << "      - chkpt_uJ   : low-voltage interrupt energy level (uJ)" << std::endl
                  << "      - lo_V       : discharge down to voltage (V)" << std::endl
                  << "      - C_uF       : platform capacitance (uF)" << std::endl
                  << std::endl;
    }

    static float str2float(const std::string &s)
    {
        float tmp;
        std::istringstream stream(s);
        stream >> tmp;
        return tmp;
    }
};

#endif

