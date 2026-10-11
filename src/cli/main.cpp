// Headless command line entry point (no SDL window, no UI).

#include "Cli.h"

#include <iostream>

int main(int argc, char** argv) {
    return Cartograph::RunCli(argc, argv, std::cout, std::cerr);
}
