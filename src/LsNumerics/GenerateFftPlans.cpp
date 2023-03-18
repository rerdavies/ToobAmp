/*
 * MIT License
 *
 * Copyright (c) 2023 Robin E. R. Davies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <string>
#include <filesystem>
#include "BalancedConvolution.hpp"
#include <stdexcept>
#include "../ss.hpp"
#include "../CommandLineParser.hpp"

using namespace std;
using namespace LsNumerics;
using namespace TwoPlay;

static void PrintHelp()
{
    cout
        << "GenerateFftPlans - Generated pre-computed balanced convolution section execution plans." << std::endl
        << "Syntax:  GenerateFftPlans <output_directory> [<max-size_in_kb>]" << endl
        << endl
        << endl
        << "If the directory name ends in '.gz', output files will be gzip-ed." << endl
        << endl
        << "Generating plan files is currently at least O(N^2 Log(N)) in execution time and O(N^2) in memory use. " << endl
        << "A full set of plan files requires at least 6GB of memory, to generate and it takes up to an hour and " << endl
        << "a half to do so. Close all other programs when generating plan files if you have 8GB of memory." << endl
        << endl
        << "If you are unable to generate plan files on your computer, the project includes a pregenerated set in " << endl
        << "the 'fftplans.gz' directory. You only need to regenerate the plan files if you have made changes to the " << endl
        << "file format of plan files in LsNumerics/BalancedConvolution.cpp." << endl
        << endl;
}
int main(int argc, const char **argv)
{

    bool help = false;
    size_t maxSize = 128 * 1024;
    CommandLineParser parser;
    std::filesystem::path outputDirectory;

    try
    {
        parser.AddOption("h", "help", &help);

        parser.Parse(argc, argv);

        switch (parser.Arguments().size())
        {
        case 0:
            help = true;
            break;
        case 1:
            outputDirectory = parser.Arguments()[0];
            break;
        case 2:
            outputDirectory = parser.Arguments()[1];
            try
            {
                size_t kb = std::stoul(parser.Arguments()[1]);
                maxSize = (size_t)(kb * 1024);
            }
            catch (const std::exception &e)
            {
                throw std::logic_error(SS("Expecting a positive numeric value: " << parser.Arguments()[1]));
            }
            break;
        default:
            throw std::logic_error("Incorrect number of arguments.");
        }

        if (help)
        {
            PrintHelp();
            return EXIT_SUCCESS;
        }
        std::vector<float> impulseResponse;
        impulseResponse.push_back(0);
        bool gzipOutput = false;

        if (outputDirectory.extension() == ".gz")
        {
            gzipOutput = true;
        }
        if (gzipOutput)
        {
            cout << "Generating gzip-ed output." << endl;
        }
        std::filesystem::create_directories(outputDirectory);
        for (size_t n = 1; n <= maxSize; n *= 2)
        {
            cout << "Generating ConvolutionSection plan n=" << n << endl;
            BalancedConvolutionSection section{n, 0, impulseResponse};
            std::filesystem::path fileName = outputDirectory / SS(n << ".convolutionPlan" << (gzipOutput ? ".gz" : ""));

            section.Save(fileName);
        }
    }
    catch (const std::exception &e)
    {
        cout << "ERROR: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}