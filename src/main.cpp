#include "first_app.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    cpe::FirstApp app{};
    try {
        app.runRayTracer();
    } catch(const std::exception &err) {
        std::cerr << "Ray tracer failed: " << err.what() << '\n';
        try {
            app.runRasterizer();
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        };
    }

    return EXIT_SUCCESS;
}