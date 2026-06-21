#include "engine/Application.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    try {
        // 이동식 패키지는 실행 파일 옆의 리소스를 사용한다. 개발 빌드는 기존 cwd를 유지한다.
        if (argc > 0) {
            const std::filesystem::path executableDirectory =
                std::filesystem::absolute(argv[0]).parent_path();
            if (std::filesystem::is_directory(executableDirectory / "shaders") &&
                std::filesystem::is_directory(executableDirectory / "Content")) {
                std::filesystem::current_path(executableDirectory);
            }
        }
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
