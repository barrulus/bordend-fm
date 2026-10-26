#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--version") {
            std::puts("nordend " NORDEND_VERSION);
            return 0;
        }
    }
    return 0;
}
