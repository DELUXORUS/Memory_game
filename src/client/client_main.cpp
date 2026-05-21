/**
 * @file client_main.cpp
 * @brief Точка входа клиентского приложения игры на память.
 */
#include "client.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

/**
 * @brief Запускает клиентское приложение игры на память.
 *
 * Использование командной строки:
 * @code{.sh}
 * client <имя> [host] [port]
 * @endcode
 */
int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Использование: " << argv[0] << " <имя> [host] [port]" << "\n";
        return EXIT_FAILURE;
    }

    std::string name = argv[1];
    std::string host = "172.16.45.116";
    uint16_t port = 12345;
    if (argc >= 3) {
        host = argv[2];
    }
    if (argc == 4) {
        port = static_cast<uint16_t>(std::stoi(argv[3]));
    }

    MemoryGameClient client(host, port, name);
    if (!client.run()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
