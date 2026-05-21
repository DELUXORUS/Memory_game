/**
 * @file client.hpp
 * @brief Интерфейс клиента игры на память и обертка сетевого протокола.
 */
#ifndef MEMORY_GAME_CLIENT_HPP
#define MEMORY_GAME_CLIENT_HPP

#include <cstdint>
#include <string>

/**
 * @brief TCP-клиент для игры на память.
 *
 * Клиент подключается к серверу, отправляет команды, принимает символы игры и
 * выводит ответы сервера для интерактивной игры.
 */
class MemoryGameClient {
public:
    /**
     * @brief Создает клиента игры на память.
     * @param host Адрес сервера.
     * @param port TCP-порт сервера.
     * @param name Имя игрока.
     */
    MemoryGameClient(const std::string &host, uint16_t port, const std::string &name);

    /**
     * @brief Запускает цикл обработки клиента.
     * @return true при успешном запуске, false при ошибке.
     */
    bool run();

private:
    bool connect_to_server();
    void main_loop();
    void print_help() const;
    void handle_user_input();
    bool handle_server_read();
    void process_server_messages();
    void handle_server_message(const std::string &message);
    void send_command(const std::string &command);
    bool flush_send_buffer();

    std::string host_;        /**< Адрес сервера (имя хоста или IP). */
    uint16_t port_;           /**< Порт, по которому слушает сервер. */
    std::string name_;        /**< Имя игрока, отправляемое серверу. */
    int socket_fd_;           /**< Дескриптор TCP-сокета. */
    std::string send_buffer_; /**< Буфер для исходящих данных. */
    std::string recv_buffer_; /**< Буфер для входящих данных. */
    std::string current_sequence_; /**< Последняя полученная от сервера последовательность символов. */
};

#endif // MEMORY_GAME_CLIENT_HPP
