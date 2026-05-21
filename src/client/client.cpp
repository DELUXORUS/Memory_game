#include "client.hpp"
#include "../include/protocol.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

namespace {

// Устанавливает сокет в неблокирующий режим
bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

// Удаляет пробелы в начале и конце строки
std::string trim_string(const std::string &text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        start++;
    }
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        end--;
    }
    return text.substr(start, end - start);
}

} // namespace

MemoryGameClient::MemoryGameClient(const std::string &host, uint16_t port, const std::string &name)
    : host_(host), port_(port), name_(name), socket_fd_(-1) {}

// Запускает клиент: подключается к серверу, отправляет имя и входит в цикл обработки
bool MemoryGameClient::run() {
    if (!connect_to_server()) {
        return false;
    }

    send_command(name_);
    print_help();
    main_loop();
    return true;
}

// Подключается к серверу по TCP
bool MemoryGameClient::connect_to_server() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Не удалось создать сокет: " << std::strerror(errno) << "\n";
        return false;
    }

    if (!set_nonblocking(socket_fd_)) {
        std::cerr << "Не удалось установить сокет в неблокирующий режим: " << std::strerror(errno) << "\n";
        close(socket_fd_);
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *result = nullptr;
    int error = getaddrinfo(host_.c_str(), nullptr, &hints, &result);
    if (error != 0 || result == nullptr) {
        std::cerr << "Не удалось разрешить хост: " << gai_strerror(error) << "\n";
        close(socket_fd_);
        return false;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_);
    server_address.sin_addr = reinterpret_cast<sockaddr_in *>(result->ai_addr)->sin_addr;
    freeaddrinfo(result);

    int connect_result = connect(socket_fd_, reinterpret_cast<sockaddr *>(&server_address), sizeof(server_address));
    if (connect_result < 0 && errno != EINPROGRESS) {
        std::cerr << "Подключение не удалось: " << std::strerror(errno) << "\n";
        close(socket_fd_);
        return false;
    }

    std::cout << "Подключено к серверу " << host_ << ":" << port_ << "\n";
    return true;
}

// Основной цикл обработки событий: сеть и ввод пользователя
void MemoryGameClient::main_loop() {
    while (true) {
        pollfd fds[2];
        fds[0].fd = socket_fd_;
        fds[0].events = POLLIN | (send_buffer_.empty() ? 0 : POLLOUT);
        fds[0].revents = 0;
        fds[1].fd = STDIN_FILENO;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int ready = poll(fds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Poll не удался: " << std::strerror(errno) << "\n";
            break;
        }

        if (fds[0].revents & (POLLERR | POLLHUP)) {
            std::cerr << "Сервер отключился\n";
            break;
        }

        if (fds[0].revents & POLLIN) {
            if (!handle_server_read()) {
                break;
            }
        }

        if (fds[0].revents & POLLOUT) {
            if (!flush_send_buffer()) {
                break;
            }
        }

        if (fds[1].revents & POLLIN) {
            handle_user_input();
        }
    }
}

// Выводит справку по командам
void MemoryGameClient::print_help() const {
    std::cout << "Введите команды: начать, рекорд, сдаюсь, тест\n";
    std::cout << "Также можно вводить запомненную последовательность после появления символов.\n";
    std::cout << "Для удобства поддерживаются английские синонимы: start, record, give_up, test.\n";
}

// Обрабатывает ввод пользователя
void MemoryGameClient::handle_user_input() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return;
    }
    line = trim_string(line);
    if (line.empty()) {
        return;
    }
    send_command(line);
}

// Читает данные от сервера
bool MemoryGameClient::handle_server_read() {
    char buffer[1024];
    while (true) {
        ssize_t received = recv(socket_fd_, buffer, sizeof(buffer), 0);
        if (received > 0) {
            recv_buffer_.append(buffer, static_cast<size_t>(received));
            process_server_messages();
            continue;
        }
        if (received == 0) {
            return false;
        }
        if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        return false;
    }
    return true;
}

// Обрабатывает полученные сообщения от сервера
void MemoryGameClient::process_server_messages() {
    while (true) {
        size_t pos = recv_buffer_.find(memory_game::k_line_delimiter);
        if (pos == std::string::npos) {
            return;
        }
        std::string line = recv_buffer_.substr(0, pos);
        recv_buffer_.erase(0, pos + 1);
        handle_server_message(trim_string(line));
    }
}

// Обрабатывает одно сообщение от сервера
void MemoryGameClient::handle_server_message(const std::string &message) {
    if (message == memory_game::k_welcome) {
        std::cout << "Сервер: добро пожаловать\n";
        // print_help();
        return;
    }

    if (message.rfind(memory_game::k_record_prefix, 0) == 0) {
        std::cout << "Сервер: " << message << "\n";
        return;
    }

    if (message.rfind(memory_game::k_symbol_prefix, 0) == 0) {
        if (message.size() > memory_game::k_symbol_prefix.size()) {
            char symbol = message[memory_game::k_symbol_prefix.size()];
            current_sequence_.push_back(symbol);
            std::cout << "Раунд " << current_sequence_.size() << ": Символ " << symbol << "\n";
            std::cout << "Текущая последовательность: " << current_sequence_ << "\n";
            std::cout << "Введите полную последовательность или сдаюсь:\n";
        }
        return;
    }

    if (message == memory_game::k_correct) {
        std::cout << "Сервер: правильно. Длина последовательности " << current_sequence_.size()
                  << ". Ожидаем следующий символ...\n";
        return;
    }

    if (message.rfind(memory_game::k_wrong_prefix, 0) == 0) {
        std::string payload = message.substr(memory_game::k_wrong_prefix.size());
        size_t separator = payload.find(':');
        if (separator != std::string::npos) {
            std::string length_text = payload.substr(0, separator);
            std::string record_text = payload.substr(separator + 1);
            std::cout << "Сервер: игра окончена. Достигнута длина " << length_text
                      << ", рекорд " << record_text << ".\n";
        } else {
            std::cout << "Сервер: " << message << "\n";
        }
        current_sequence_.clear();
        return;
    }

    if (message == memory_game::k_test_done) {
        std::cout << "Сервер: тест производительности завершен\n";
        return;
    }

    if (message.rfind(memory_game::k_error_prefix, 0) == 0) {
        std::cout << "Сервер: " << message << "\n";
        return;
    }

    std::cout << "Сервер: " << message << "\n";
}

// Отправляет команду серверу
void MemoryGameClient::send_command(const std::string &command) {
    send_buffer_ += memory_game::make_line(command);
    flush_send_buffer();
}

// Отправляет данные из буфера
bool MemoryGameClient::flush_send_buffer() {
    while (!send_buffer_.empty()) {
        ssize_t sent = send(socket_fd_, send_buffer_.data(), send_buffer_.size(), 0);
        if (sent > 0) {
            send_buffer_.erase(0, static_cast<size_t>(sent));
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return true;
        }
        return false;
    }
    return true;
}
