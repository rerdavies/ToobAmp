/* Check that we can load all models in /var/pipedal/audio_uploads/ToobMlModels */

#include "ToobML.h"
#include <filesystem>
#include <iostream>

using namespace toob;

int main(int argc, char **argv)
{
    namespace fs = std::filesystem;
    int nModels = 0;
    int nFailures = 0;
    for (auto direntry : fs::recursive_directory_iterator("/var/pipedal/audio_uploads/ToobMlModels"))
    {
        if (!direntry.is_directory())
        {
            auto path = direntry.path();
            if (path.extension() == ".json")
            {
                try
                {
                    ++nModels;
                    auto result = ToobMlModel::Load(direntry.path());
                    if (!result)
                    {
                        throw std::runtime_error("Null model returned.");
                    }
                }
                catch (const std::exception &e)
                {
                    ++nFailures;
                    std::cout << "Error: " << direntry.path() << ": " << e.what() << std::endl;
                }
            }
        }
    }
    std::cout << "Models: " << nModels << " Failures: " << nFailures << std::endl;
    return nFailures = 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}