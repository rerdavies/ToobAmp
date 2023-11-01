
#include "FlacReader.hpp"
#include <iostream>


// Flac library linkage is exceptionally fragile. 
// make sure that we can actually decode a flac file.

using namespace toob;
using namespace std;

int main(int argc, char**argv)
{
    std::filesystem::path filename = "../../Assets/test-chirp.flac";

    if (argc >= 2)
    {
        filename = argv[1];
    }

    try {
        FlacReader::Load(filename);
    } catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    cout << "Success." << endl;
    return EXIT_SUCCESS;

}