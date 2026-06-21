#include "engine/Application.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> arguments;
        arguments.reserve(static_cast<std::size_t>(std::max(argc - 1, 0)));
        for (int index = 1; index < argc; ++index) {
            arguments.emplace_back(argv[index]);
        }
        engine::Application application{
            engine::parseApplicationOptions(arguments)
        };
        return application.run();
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
