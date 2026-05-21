/**
 * @file server.hpp
 * @brief Интерфейс сервера игры на память и менеджер клиентских сессий.
 */
#ifndef MEMORY_GAME_SERVER_HPP
#define MEMORY_GAME_SERVER_HPP

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>
#include <poll.h>

/**
 * @brief TCP-сервер для игры на память.
 *
 * Сервер принимает несколько TCP-клиентов, хранит состояние каждого игрока и
 * реализует протокол игры на память на основе строковых команд с разделителем строки.
 */
class MemoryGameServer {
public:
    /**
     * @brief Создает сервер игры на память.
     * @param host Адрес для привязки.
     * @param port Порт для прослушивания.
     */
    MemoryGameServer(const std::string &host, uint16_t port);

    /**
     * @brief Запускает сервер и цикл обработки клиентов.
     * @return true при успешном запуске, false при ошибке.
     */
    bool start();

private:
    /**
     * @brief Состояние протокола клиента.
     */
    enum class ClientState { Connected, Waiting, Playing, Testing };

    /**
     * @brief Данные сессии клиента, хранящиеся на сервере.
     */
    struct Client {
        int socket_fd = -1;           /**< Дескриптор сокета клиента. */
        std::string name;             /**< Зарегистрированное имя игрока. */
        std::vector<char> sequence;   /**< Текущая последовательность символов во время игры. */
        int record = 0;               /**< Лучшее количество запомненных символов. */
        ClientState state = ClientState::Connected; /**< Текущее состояние протокола. */
        std::string input_buffer;     /**< Буфер входящих данных от клиента. */
        std::string output_buffer;    /**< Буфер исходящих данных для клиента. */
    };

    void main_loop();
    void accept_clients();
    bool handle_client_read(int fd);
    void process_client_messages(int fd);
    void handle_message(int fd, const std::string &message);
    void send_next_symbol(int fd);
    void start_test(int fd);
    void queue_message(int fd, const std::string &message);
    bool flush_client_output(int fd);
    void update_poll_events(int fd);
    void remove_client(int fd);

    std::string host_;             /**< Адрес прослушивания. */
    uint16_t port_;                /**< Порт прослушивания. */
    int server_fd_;                /**< Дескриптор серверного сокета. */
    std::vector<pollfd> poll_fds_;  /**< Список дескрипторов для poll. */
    std::map<int, Client> clients_; /**< Активные подключенные клиенты. */
    std::mt19937 rng_;             /**< Генератор случайных символов. */
};

#endif // MEMORY_GAME_SERVER_HPP
