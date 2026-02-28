#ifndef SD_BACKUP_HPP
#define SD_BACKUP_HPP

#include "main.h"
#include "config.hpp"
#include <cstdint>
#include <cstring>

extern "C" {
#include "ff.h"
#include "fatfs.h"
}

class SdBackup {
public:
    SdBackup() = default;

    bool init();
    void deinit();

    // JSONL: дописать одну строку (JSON-объект) + \r\n
    bool appendLine(const char* jsonLine);

    // Прочитать чанк из начала файла и собрать JSON-массив в out: [obj,obj,...]
    bool readChunkAsJsonArray(char* out,
                              uint32_t outSize,
                              uint32_t maxPayloadBytes,
                              uint32_t& linesRead,
                              FSIZE_t& bytesUsedFromFile);

    // Удалить из начала файла bytesUsedFromFile байт (отправленный префикс)
    bool consumePrefix(FSIZE_t bytesUsedFromFile);

    bool exists() const;
    bool remove();

private:
    bool  m_mounted = false;
    FATFS m_fatfs{};
    FIL   m_file{};

    void make_drive(char* out, size_t out_sz) const;
    void make_full_path(char* out, size_t out_sz, const char* fname) const;
};

#endif
