/**
 * @file server_main.cpp
 * @brief Точка входа серверного приложения игры на память.
 */
#include "server.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>

/**
 * @brief Запускает серверное приложение игры на память.
 *
 * Использование командной строки:
 * @code{.sh}
 * server [host] [port]
 * @endcode
 */
int main(int argc, char *argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 12345;
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    MemoryGameServer server(host, port);
    if (!server.start()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
