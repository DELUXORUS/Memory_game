#include "server.hpp"
#include "../include/protocol.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
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

// Генерирует случайный символ для игры
char random_symbol(std::mt19937 &engine) {
    static const char symbols[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<size_t> distribution(0, sizeof(symbols) - 2);
    return symbols[distribution(engine)];
}

} // namespace

MemoryGameServer::MemoryGameServer(const std::string &host, uint16_t port)
    : host_(host), port_(port), server_fd_(-1), rng_(std::random_device{}()) {}

// Запускает сервер: создает сокет, привязывает, слушает и входит в цикл
bool MemoryGameServer::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Не удалось создать сокет: " << std::strerror(errno) << "\n";
        return false;
    }

    int reuse = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host_.c_str());
    address.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        std::cerr << "Bind не удался: " << std::strerror(errno) << "\n";
        close(server_fd_);
        return false;
    }

    if (listen(server_fd_, SOMAXCONN) < 0) {
        std::cerr << "Listen не удался: " << std::strerror(errno) << "\n";
        close(server_fd_);
        return false;
    }

    if (!set_nonblocking(server_fd_)) {
        std::cerr << "Не удалось установить серверный сокет в неблокирующий режим: " << std::strerror(errno) << "\n";
        close(server_fd_);
        return false;
    }

    poll_fds_.push_back({server_fd_, POLLIN, 0});
    std::cout << "Сервер запущен на " << host_ << ":" << port_ << "\n";

    main_loop();
    return true;
}

// Основной цикл обработки событий от клиентов
void MemoryGameServer::main_loop() {
    while (true) {
        int ready = poll(poll_fds_.data(), poll_fds_.size(), -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Poll не удался: " << std::strerror(errno) << "\n";
            break;
        }

        size_t index = 0;
        while (index < poll_fds_.size()) {
            pollfd &current = poll_fds_[index];
            if (current.revents == 0) {
                ++index;
                continue;
            }

            if (current.fd == server_fd_) {
                if (current.revents & POLLIN) {
                    accept_clients();
                }
                ++index;
                continue;
            }

            int fd = current.fd;
            if (current.revents & (POLLHUP | POLLERR)) {
                remove_client(fd);
                continue;
            }

            if (current.revents & POLLIN) {
                if (!handle_client_read(fd)) {
                    remove_client(fd);
                    continue;
                }
            }

            if (current.revents & POLLOUT) {
                if (!flush_client_output(fd)) {
                    remove_client(fd);
                    continue;
                }
            }

            update_poll_events(fd);
            ++index;
        }
    }
}

// Принимает новые подключения клиентов
void MemoryGameServer::accept_clients() {
    while (true) {
        sockaddr_in client_address;
        socklen_t address_length = sizeof(client_address);
        int client_socket = accept(server_fd_, reinterpret_cast<sockaddr *>(&client_address), &address_length);
        if (client_socket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            std::cerr << "Accept не удался: " << std::strerror(errno) << "\n";
            break;
        }

        if (!set_nonblocking(client_socket)) {
            std::cerr << "Не удалось установить клиентский сокет в неблокирующий режим: " << std::strerror(errno) << "\n";
            close(client_socket);
            continue;
        }

        Client client;
        client.socket_fd = client_socket;
        client.state = ClientState::Connected;
        clients_.emplace(client_socket, std::move(client));
        poll_fds_.push_back({client_socket, POLLIN, 0});
        std::cout << "Клиент подключился: " << client_socket << "\n";
    }
}

// Читает данные от клиента
bool MemoryGameServer::handle_client_read(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        return false;
    }
    char buffer[1024];
    while (true) {
        ssize_t recv_size = recv(fd, buffer, sizeof(buffer), 0);
        if (recv_size > 0) {
            it->second.input_buffer.append(buffer, static_cast<size_t>(recv_size));
            process_client_messages(fd);
            continue;
        }

        if (recv_size == 0) {
            return false;
        }

        if (recv_size < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return true;
            }
            return false;
        }
    }
}

// Обрабатывает сообщения от клиента
void MemoryGameServer::process_client_messages(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        return;
    }

    std::string &buffer = it->second.input_buffer;
    while (true) {
        size_t pos = buffer.find(memory_game::k_line_delimiter);
        if (pos == std::string::npos) {
            break;
        }
        std::string message = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        handle_message(fd, trim_string(message));
    }
}

// Обрабатывает одно сообщение от клиента
void MemoryGameServer::handle_message(int fd, const std::string &message) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        return;
    }
    Client &client = it->second;

    if (client.state == ClientState::Connected) {
        if (!message.empty()) {
            client.name = message;
            client.state = ClientState::Waiting;
            queue_message(fd, memory_game::k_welcome);
            std::cout << "Игрок присоединился: " << client.name << "\n";
            return;
        }
        queue_message(fd, memory_game::make_error("invalid_name"));
        return;
    }

    if (message == "record" || message == memory_game::k_record_command) {
        queue_message(fd, memory_game::make_record(client.record));
        return;
    }

    if (message == "test" || message == memory_game::k_test_command) {
        client.state = ClientState::Testing;
        start_test(fd);
        return;
    }

    if (message == "start" || message == memory_game::k_start_command) {
        if (client.state == ClientState::Waiting) {
            client.sequence.clear();
            client.state = ClientState::Playing;
            send_next_symbol(fd);
            return;
        }
        queue_message(fd, memory_game::make_error("invalid_state"));
        return;
    }

    if (message == "give_up" || message == memory_game::k_give_up_command) {
        if (client.state == ClientState::Playing) {
            int length = static_cast<int>(client.sequence.size()) - 1;
            if (length < 0) {
                length = 0;
            }
            int current_record = client.record;
            queue_message(fd, memory_game::make_wrong(length, current_record));
            client.state = ClientState::Waiting;
            client.sequence.clear();
            return;
        }
        queue_message(fd, memory_game::make_error("not_playing"));
        return;
    }

    if (client.state == ClientState::Playing) {
        std::string expected(client.sequence.begin(), client.sequence.end());
        if (message == expected) {
            queue_message(fd, memory_game::k_correct);
            send_next_symbol(fd);
            return;
        }
        int length = static_cast<int>(client.sequence.size()) - 1;
        if (length < 0) {
            length = 0;
        }
        if (length > client.record) {
            client.record = length;
        }
        queue_message(fd, memory_game::make_wrong(length, client.record));
        client.state = ClientState::Waiting;
        client.sequence.clear();
        return;
    }

    if (client.state == ClientState::Waiting) {
        queue_message(fd, memory_game::make_error("unknown_command"));
        return;
    }

    if (client.state == ClientState::Testing) {
        return;
    }
}

// Отправляет следующий символ клиенту
void MemoryGameServer::send_next_symbol(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        return;
    }
    char symbol = random_symbol(rng_);
    it->second.sequence.push_back(symbol);
    queue_message(fd, memory_game::make_symbol(symbol));
}

// Запускает тест производительности
void MemoryGameServer::start_test(int fd) {
    for (int i = 0; i < 1000; ++i) {
        queue_message(fd, memory_game::make_symbol(random_symbol(rng_)));
    }
    queue_message(fd, memory_game::k_test_done);
    auto it = clients_.find(fd);
    if (it != clients_.end()) {
        it->second.state = ClientState::Waiting;
    }
}

// Добавляет сообщение в очередь для отправки клиенту
void MemoryGameServer::queue_message(int fd, const std::string &message) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        return;
    }
    Client &client = it->second;
    client.output_buffer += memory_game::make_line(message);
    update_poll_events(fd);
}

// Отправляет данные из буфера клиенту
bool MemoryGameServer::flush_client_output(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        return false;
    }

    Client &client = it->second;
    while (!client.output_buffer.empty()) {
        ssize_t sent = send(fd, client.output_buffer.data(), client.output_buffer.size(), 0);
        if (sent > 0) {
            client.output_buffer.erase(0, static_cast<size_t>(sent));
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return true;
        }
        return false;
    }
    return true;
}

// Обновляет события poll для клиента
void MemoryGameServer::update_poll_events(int fd) {
    for (auto &entry : poll_fds_) {
        if (entry.fd != fd) {
            continue;
        }
        entry.events = POLLIN;
        auto it = clients_.find(fd);
        if (it != clients_.end() && !it->second.output_buffer.empty()) {
            entry.events |= POLLOUT;
        }
        return;
    }
}

// Удаляет клиента и закрывает соединение
void MemoryGameServer::remove_client(int fd) {
    clients_.erase(fd);
    for (auto it = poll_fds_.begin(); it != poll_fds_.end(); ++it) {
        if (it->fd == fd) {
            poll_fds_.erase(it);
            break;
        }
    }
    close(fd);
    std::cout << "Клиент отключился: " << fd << "\n";
}
