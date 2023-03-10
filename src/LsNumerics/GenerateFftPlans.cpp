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
#include "BalancedFft.hpp"
#include <stdexcept>
#include "../ss.hpp"

using namespace std;
using namespace LsNumerics;

int main(int argc, char**argv)
{
    if (argc != 2 && argc != 3)
    {
        cout << "Syntax:  GenerateFftPlans <output_directory> [<max-size_in_kb>]" << endl;
        return EXIT_FAILURE;
    }
    std::filesystem::path outputDirectory = argv[1];

    size_t maxSize = 65536;
    if (argc == 3)
    {
        try {
            int kb = std::stoi(argv[2]);
            if (kb < 64) {
                throw std::invalid_argument("");
            }
            maxSize = 1024U*(size_t)kb;
        } catch (const std::exception&e)
        {
            cout << "Invalid 2nd argument. Expecting an integer greater than 32.";
        }
        cout << "Max size: " << maxSize << endl;

    }

    std::vector<float> impulseResponse;
    impulseResponse.push_back(0);

    std::filesystem::create_directories(outputDirectory);
    for (size_t n = 32; n <= maxSize; n *= 2)
    {
        cout << "Generating ConvolutionSection plan n=" << n << endl;
        BalancedConvolutionSection section {n,0,impulseResponse};
        std::filesystem::path fileName = outputDirectory / SS(n << ".convolutionPlan");

        section.Save(fileName);
    }


    return EXIT_SUCCESS;

}