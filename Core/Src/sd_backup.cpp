#include "sd_backup.hpp"
#include "debug_uart.hpp"
#include <cstdio>

void SdBackup::make_drive(char* out, size_t out_sz) const
{
    if (out_sz < 3) return;
    out[0] = USERPath[0] ? USERPath[0] : '0';
    out[1] = ':';
    out[2] = '\0';
}

void SdBackup::make_full_path(char* out, size_t out_sz, const char* fname) const
{
    char drive[3];
    make_drive(drive, sizeof(drive));
    std::snprintf(out, out_sz, "%s/%s", drive, fname); // "0:/backup.jsonl"
}

bool SdBackup::init()
{
    char drive[3];
    make_drive(drive, sizeof(drive));

    FRESULT fr = f_mount(&m_fatfs, drive, 1);
    if (fr != FR_OK) {
        DBG.error("SD: mount fail (FR=%d)", (int)fr);
        m_mounted = false;
        return false;
    }

    m_mounted = true;
    return true;
}

void SdBackup::deinit()
{
    if (!m_mounted) return;

    char drive[3];
    make_drive(drive, sizeof(drive));

    f_mount(nullptr, drive, 0);
    m_mounted = false;
}

bool SdBackup::exists() const
{
    if (!m_mounted) return false;

    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);

    FILINFO fno;
    return f_stat(path, &fno) == FR_OK;
}

bool SdBackup::remove()
{
    if (!m_mounted) return false;

    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);

    return f_unlink(path) == FR_OK;
}

bool SdBackup::appendLine(const char* jsonLine)
{
    if (!m_mounted || !jsonLine) return false;

    const size_t n = std::strlen(jsonLine);

    // защита: не пишем слишком длинные строки
    if (n == 0 || n > Config::JSONL_LINE_MAX) {
        DBG.error("SD: JSONL line too long (%u)", (unsigned)n);
        return false;
    }
    // защита: не допускаем переносы строк внутри записи
    for (size_t i = 0; i < n; i++) {
        if (jsonLine[i] == '\r' || jsonLine[i] == '\n') {
            DBG.error("SD: JSONL line contains CR/LF");
            return false;
        }
    }

    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);

    FIL f{};
    FRESULT fr = f_open(&f, path, FA_OPEN_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return false;

    fr = f_lseek(&f, f_size(&f)); // в конец
    if (fr != FR_OK) { f_close(&f); return false; }

    UINT bw = 0;
    fr = f_write(&f, jsonLine, (UINT)n, &bw);
    if (fr != FR_OK || bw != (UINT)n) { f_close(&f); return false; }

    const char eol[] = "\r\n";
    UINT bw2 = 0;
    fr = f_write(&f, eol, sizeof(eol) - 1, &bw2);
    if (fr != FR_OK) { f_close(&f); return false; }

    fr = f_sync(&f);
    f_close(&f);
    return (fr == FR_OK);
}

bool SdBackup::readChunkAsJsonArray(char* out,
                                   uint32_t outSize,
                                   uint32_t maxPayloadBytes,
                                   uint32_t& linesRead,
                                   FSIZE_t& bytesUsedFromFile)
{
    linesRead = 0;
    bytesUsedFromFile = 0;
    if (!m_mounted || !out || outSize < 4) return false;

    if (maxPayloadBytes >= outSize) maxPayloadBytes = outSize - 1;
    if (maxPayloadBytes < 4) return false;

    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);

    FIL f{};
    FRESULT fr = f_open(&f, path, FA_READ);
    if (fr != FR_OK) return false;

    uint32_t off = 0;
    out[off++] = '[';

    // +4 — чтобы точно помещались \r\n\0 и т.п.
    char line[Config::JSONL_LINE_MAX + 4];
    FSIZE_t lastPosAfterLine = 0;

    while (true) {
        char* s = f_gets(line, sizeof(line), &f);
        if (!s) break; // EOF

        lastPosAfterLine = f_tell(&f);

        // trim справа
        size_t n = std::strlen(line);
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ' || line[n - 1] == '\t')) {
            line[--n] = '\0';
        }
        if (n == 0) {
            bytesUsedFromFile = lastPosAfterLine;
            continue;
        }

        // Проверка лимита maxPayloadBytes: оставляем место на ']' и '\0'
        // Текущая "полезная" длина будущей строки массива = off + (comma?) + n + 1 (])
        uint32_t need = (linesRead ? 1u : 0u) + (uint32_t)n + 1u + 1u; // +']' +'\0'
        if (off + need > maxPayloadBytes) {
            break;
        }

        // Проверка outSize (доп. страховка)
        if (off + need > outSize) {
            break;
        }

        if (linesRead) out[off++] = ',';
        std::memcpy(&out[off], line, n);
        off += (uint32_t)n;

        linesRead++;
        bytesUsedFromFile = lastPosAfterLine;
    }

    out[off++] = ']';
    out[off] = '\0';

    f_close(&f);

    return (linesRead > 0 && bytesUsedFromFile > 0);
}

bool SdBackup::consumePrefix(FSIZE_t bytesUsedFromFile)
{
    if (!m_mounted) return false;
    if (bytesUsedFromFile == 0) return true;

    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);

    char tmpPath[64];
    make_full_path(tmpPath, sizeof(tmpPath), "backup.tmp");

    FIL src{};
    FRESULT fr = f_open(&src, path, FA_READ);
    if (fr != FR_OK) return false;

    FSIZE_t sz = f_size(&src);
    if (bytesUsedFromFile >= sz) {
        f_close(&src);
        return (f_unlink(path) == FR_OK);
    }

    FIL dst{};
    fr = f_open(&dst, tmpPath, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) { f_close(&src); return false; }

    fr = f_lseek(&src, bytesUsedFromFile);
    if (fr != FR_OK) { f_close(&src); f_close(&dst); f_unlink(tmpPath); return false; }

    uint8_t buf[512];
    UINT br = 0, bw = 0;

    while (true) {
        fr = f_read(&src, buf, sizeof(buf), &br);
        if (fr != FR_OK) break;
        if (br == 0) { fr = FR_OK; break; }

        fr = f_write(&dst, buf, br, &bw);
        if (fr != FR_OK || bw != br) { fr = FR_DISK_ERR; break; }
    }

    f_close(&src);
    f_close(&dst);

    if (fr != FR_OK) { f_unlink(tmpPath); return false; }

    f_unlink(path);
    fr = f_rename(tmpPath, path);
    if (fr != FR_OK) { f_unlink(tmpPath); return false; }

    return true;
}
