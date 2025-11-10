#include "App.h"
#include <iostream>

int main(int argc, char* argv[]) {
    Cartograph::App app;
    
    if (!app.Init("Cartograph", 1280, 720)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    int exitCode = app.Run();
    
    app.Shutdown();
    
    return exitCode;
}

