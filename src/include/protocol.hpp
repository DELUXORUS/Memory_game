/**
 * @file protocol.hpp
 * @brief Общие определения протокола для игры на память.
 *
 * Этот файл содержит определения текстового TCP-протокола, используемого
 * сервером и клиентом игры на память. Протокол использует сообщения,
 * разделенные символом новой строки.
 */
#ifndef MEMORY_GAME_PROTOCOL_H
#define MEMORY_GAME_PROTOCOL_H

#include <string>

namespace memory_game {

/**
 * @brief Разделитель строки в протоколе.
 */
inline constexpr char k_line_delimiter = '\n';

/** @brief Команда клиента для начала новой партии. */
inline const std::string k_start_command = "начать";
/** @brief Команда клиента для запроса рекорда. */
inline const std::string k_record_command = "рекорд";
/** @brief Команда клиента для прекращения текущей партии. */
inline const std::string k_give_up_command = "сдаюсь";
/** @brief Команда клиента для запуска режима тестирования. */
inline const std::string k_test_command = "тест";

/** @brief Приветственное сообщение сервера. */
inline const std::string k_welcome = "welcome";
/** @brief Префикс для сообщений об ошибке. */
inline const std::string k_error_prefix = "error:";
/** @brief Префикс для ответа с рекордом. */
inline const std::string k_record_prefix = "record:";
/** @brief Префикс для ответа с символом. */
inline const std::string k_symbol_prefix = "symbol:";
/** @brief Сообщение об успешном ответе. */
inline const std::string k_correct = "correct";
/** @brief Префикс для сообщения о неправильном ответе. */
inline const std::string k_wrong_prefix = "wrong:";
/** @brief Сообщение о завершении теста. */
inline const std::string k_test_done = "test_done";

/**
 * @brief Сформировать сообщение об ошибке.
 * @param payload Описание ошибки.
 * @return Строка протокола с ошибкой.
 */
inline std::string make_error(const std::string &payload) {
    return k_error_prefix + payload;
}

/**
 * @brief Сформировать сообщение с рекордом.
 * @param record Лучшее количество символов.
 * @return Строка протокола с рекордом.
 */
inline std::string make_record(int record) {
    return k_record_prefix + std::to_string(record);
}

/**
 * @brief Сформировать сообщение с символом.
 * @param symbol Символ для клиента.
 * @return Строка протокола с символом.
 */
inline std::string make_symbol(char symbol) {
    return k_symbol_prefix + symbol;
}

/**
 * @brief Сформировать сообщение о неправильном ответе.
 * @param length Длина последовательности, достигнутой игроком.
 * @param record Личный рекорд игрока.
 * @return Строка протокола о неправильном ответе и рекорде.
 */
inline std::string make_wrong(int length, int record) {
    return k_wrong_prefix + std::to_string(length) + ":" + std::to_string(record);
}

/**
 * @brief Добавить символ новой строки к сообщению протокола.
 * @param payload Тело сообщения.
 * @return Полное сообщение протокола.
 */
inline std::string make_line(const std::string &payload) {
    return payload + k_line_delimiter;
}

} // namespace memory_game

#endif // MEMORY_GAME_PROTOCOL_H
