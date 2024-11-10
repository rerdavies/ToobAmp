// debug_plot.hpp
#include <vector>
#include <fstream>
#include <string>
#include <cstdlib>
#include <iostream>

class DebugPlotter
{
public:
    static void plot(const std::vector<float> &data,
                     const std::string &title = "Debug Plot",
                     const std::string &xlabel = "Index",
                     const std::string &ylabel = "Value")
    {
        // Write data to temporary file
        std::ofstream dataFile("debug_data.tmp");
        for (size_t i = 0; i < data.size(); ++i)
        {
            dataFile << i << " " << data[i] << "\n";
        }
        dataFile.close();

        // Create gnuplot script
        std::ofstream scriptFile("plot_script.gnu");
        scriptFile << "set title '" << title << "'\n"
                   << "set xlabel '" << xlabel << "'\n"
                   << "set ylabel '" << ylabel << "'\n"
                   << "set grid\n"
                   << "plot 'debug_data.tmp' with linespoints title 'Data'\n"
                   << "pause mouse close\n"; // Keep window open until clicked
        scriptFile.close();

// Launch gnuplot
#ifdef _WIN32
        int retval = system("start gnuplot plot_script.gnu");
#else
        int retval = system("gnuplot plot_script.gnu &");
#endif
        if (retval != EXIT_SUCCESS)
        {
            std::cerr << "Error: Failed to execut gnuplot." << std::endl;
        }
    }

    // Overload for multiple datasets
    static void plot(const std::vector<std::vector<float>> &datasets,
                     const std::vector<std::string> &labels,
                     const std::string &title = "Debug Plot",
                     const std::string &xlabel = "Index",
                     const std::string &ylabel = "Value")
    {
        // Write data to separate files
        for (size_t d = 0; d < datasets.size(); ++d)
        {
            std::ofstream dataFile("debug_data_" + std::to_string(d) + ".tmp");
            for (size_t i = 0; i < datasets[d].size(); ++i)
            {
                dataFile << i << " " << datasets[d][i] << "\n";
            }
            dataFile.close();
        }

        // Create gnuplot script
        std::ofstream scriptFile("plot_script.gnu");
        scriptFile << "set title '" << title << "'\n"
                   << "set xlabel '" << xlabel << "'\n"
                   << "set ylabel '" << ylabel << "'\n"
                   << "set grid\n"
                   << "plot ";

        for (size_t d = 0; d < datasets.size(); ++d)
        {
            if (d > 0)
                scriptFile << ", ";
            scriptFile << "'debug_data_" << d << ".tmp' with linespoints title '"
                       << (d < labels.size() ? labels[d] : "Dataset " + std::to_string(d)) << "'";
        }

        scriptFile << "\npause mouse close\n";
        scriptFile.close();

// Launch gnuplot
        int retval;
#ifdef _WIN32
        retval = system("start gnuplot plot_script.gnu");
#else
        retval = system("gnuplot plot_script.gnu &");
#endif
        if (retval != EXIT_SUCCESS)
        {
            throw std::runtime_error("Error: Failed to execute gnuplot.");
        }
    }
};

extern "C" __attribute__((visibility("default"))) void plotArray(float *values, int length)
{
    std::vector<float> v;
    for (int i = 0; i < length; ++i)
    {
        v.push_back(values[i]);
    }
    
    DebugPlotter::plot(v);
}
