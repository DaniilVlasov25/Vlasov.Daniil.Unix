#include <iostream>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/sha.h>

namespace fs = std::filesystem;

// Вычисление SHA1 хеша файла через системные вызовы (open/read)
std::string make_hash(const std::string& filepath)
{
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Не удалось открыть файл: " << filepath << "\n";
        return "";
    }

    SHA_CTX sha;
    if (!SHA1_Init(&sha)) {
        std::cerr << "Ошибка инициализации SHA1\n";
        close(fd);
        return "";
    }

    const size_t CHUNK = 8192;
    unsigned char buf[CHUNK];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buf, CHUNK)) > 0) {
        if (!SHA1_Update(&sha, buf, static_cast<size_t>(bytes_read))) {
            std::cerr << "Ошибка обновления хеша\n";
            close(fd);
            return "";
        }
    }

    if (bytes_read < 0) {
        std::cerr << "Ошибка чтения файла: " << filepath << "\n";
        close(fd);
        return "";
    }

    unsigned char raw[SHA_DIGEST_LENGTH];
    if (!SHA1_Final(raw, &sha)) {
        std::cerr << "Ошибка финализации хеша\n";
        close(fd);
        return "";
    }

    close(fd);

    // Преобразуем в hex-строку
    char hex[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", raw[i]);
    }
    hex[SHA_DIGEST_LENGTH * 2] = '\0';

    return std::string(hex);
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <каталог>\n";
        return 1;
    }

    std::string root = argv[1];
    if (!fs::exists(root) || !fs::is_directory(root)) {
        std::cerr << "Некорректный каталог: " << root << "\n";
        return 1;
    }

    // Карта: хеш -> путь к "оригиналу" (первый файл с таким хешем)
    std::unordered_map<std::string, std::string> seen_hashes;

    int processed_count = 0;
    int hardlinks_made = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string current_file = entry.path().string();
            std::string hash = make_hash(current_file);
            if (hash.empty()) {
                continue; // Ошибка хеширования — пропускаем
            }

            processed_count++;

            auto it = seen_hashes.find(hash);
            if (it == seen_hashes.end()) {
                // Первый файл с таким хешем — запоминаем как оригинал
                seen_hashes[hash] = current_file;
            } else {
                // Уже видели такой хеш — current_file — дубликат
                const std::string& original = it->second;

                struct stat orig_stat, curr_stat;
                if (stat(original.c_str(), &orig_stat) != 0 || stat(current_file.c_str(), &curr_stat) != 0) {
                    continue; // Не удалось получить метаданные
                }

                // Если это уже одна и та же inode — ничего не делаем
                if (orig_stat.st_dev == curr_stat.st_dev && orig_stat.st_ino == curr_stat.st_ino) {
                    continue;
                }

                // Жёсткие ссылки возможны ТОЛЬКО на одной файловой системе
                if (orig_stat.st_dev != curr_stat.st_dev) {
                    std::cerr << "Пропущен файл на другой ФС: " << current_file << "\n";
                    continue;
                }

                // Удаляем дубликат
                if (unlink(current_file.c_str()) != 0) {
                    std::cerr << "Не удалось удалить дубликат: " << current_file << "\n";
                    continue;
                }

                // Создаём жёсткую ссылку на оригинал
                if (link(original.c_str(), current_file.c_str()) != 0) {
                    std::cerr << "Не удалось создать жёсткую ссылку: " << current_file << "\n";
                    // Восстановить файл нельзя без бэкапа — пропускаем
                    continue;
                }

                hardlinks_made++;
                std::cout << "Заменён дубликат: " << current_file << " → " << original << "\n";
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Ошибка при обходе файловой системы: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\nОбработано файлов: " << processed_count << "\n";
    std::cout << "Создано жёстких ссылок: " << hardlinks_made << "\n";

    return 0;
}
