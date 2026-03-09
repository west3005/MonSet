#include "sd_backup.hpp"
#include "debug_uart.hpp"
#include "fatfs.h"
#include <string>
#include <cstdio>

extern "C" {
#include "sdio.h"
#include "sd_raw_test.h"
#include "stm32f4xx_hal_sd.h"
}

static const char* frStr(FRESULT fr)
{
    switch (fr) {
        case FR_OK: return "FR_OK";
        case FR_DISK_ERR: return "FR_DISK_ERR";
        case FR_INT_ERR: return "FR_INT_ERR";
        case FR_NOT_READY: return "FR_NOT_READY";
        case FR_NO_FILE: return "FR_NO_FILE";
        case FR_NO_PATH: return "FR_NO_PATH";
        case FR_INVALID_NAME: return "FR_INVALID_NAME";
        case FR_DENIED: return "FR_DENIED";
        case FR_EXIST: return "FR_EXIST";
        case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
        case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
        case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
        case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
        case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM";
        case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
        case FR_TIMEOUT: return "FR_TIMEOUT";
        case FR_LOCKED: return "FR_LOCKED";
        case FR_NOT_ENOUGH_CORE: return "FR_NOT_ENOUGH_CORE";
        case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
        case FR_INVALID_PARAMETER: return "FR_INVALID_PARAMETER";
        default: return "FR_???";
    }
}

void SdBackup::make_drive(char* out, size_t out_sz) const
{
    if (out_sz < 3) return;
    out[0] = SDPath[0] ? SDPath[0] : '0';
    out[1] = ':';
    out[2] = '\0';
}

void SdBackup::make_full_path(char* out, size_t out_sz, const char* fname) const
{
    char drive[3];
    make_drive(drive, sizeof(drive));
    std::snprintf(out, out_sz, "%s/%s", drive, fname);
}

bool SdBackup::init()
{
    if (m_broken) {
        DBG.warn("SD: marked broken, skip init");
        m_mounted = false;
        return false;
    }

    char drive[3];
    make_drive(drive, sizeof(drive));

    /* RAW Test */
    {
       // int rawOk = sd_raw_rw_test(2000000UL);
      //  DBG.info("SDTEST: result=%d", rawOk);

        HAL_SD_DeInit(&hsd);
        MX_SDIO_SD_Init();

        /* Безопасная скорость */
        hsd.Init.ClockDiv = 40U;
        MODIFY_REG(SDIO->CLKCR, SDIO_CLKCR_CLKDIV, 40U);
        HAL_Delay(10);
    }

    const uint32_t t0 = HAL_GetTick();
    FRESULT fr = FR_INT_ERR;

    while ((HAL_GetTick() - t0) < 5000) {
        fr = f_mount(&m_fatfs, drive, 1);
        if (fr == FR_OK) break;
        HAL_Delay(50);
    }

    if (fr != FR_OK) {
        DBG.error("SD: mount fail FR=%d %s", (int)fr, frStr(fr));
        m_mounted = false;
        m_broken = true;
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
    return (f_stat(path, &fno) == FR_OK);
}

bool SdBackup::remove()
{
    if (!m_mounted) return false;
    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);
    return (f_unlink(path) == FR_OK);
}

DWORD SdBackup::getFreeSpaceBytes() const
{
    char drive[3];
    make_drive(drive, sizeof(drive));
    FATFS* fs = nullptr;
    DWORD fre_clust = 0;
    if (f_getfree(drive, &fre_clust, &fs) != FR_OK) return 0;
    return fre_clust * fs->csize * 512UL;
}

bool SdBackup::checkAndRotateFile(FIL& f, const char* path)
{
    if (f_size(&f) < MAX_BACKUP_FILE_SIZE) return true;
    f_close(&f);
    return (f_open(&f, path, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
}

static bool writeLineWithRetry(FIL& f, const char* data, UINT len, uint8_t maxRetries)
{
    UINT bw = 0;
    for (uint8_t a = 0; a < maxRetries; a++) {
        if (a > 0) HAL_Delay(50);
        if (f_write(&f, data, len, &bw) == FR_OK && bw == len) return true;
        f_sync(&f);
    }
    return false;
}

static bool syncWithRetry(FIL& f, uint8_t maxRetries)
{
    for (uint8_t a = 0; a < maxRetries; a++) {
        if (a > 0) HAL_Delay(20);
        if (f_sync(&f) == FR_OK) return true;
    }
    return false;
}

bool SdBackup::appendLine(const char* jsonLine)
{
    if (!m_mounted || !jsonLine) return false;
    const size_t n = std::strlen(jsonLine);
    if (n == 0 || n > Config::JSONL_LINE_MAX) return false;

    for (size_t i = 0; i < n; i++) {
        if (jsonLine[i] == '\r' || jsonLine[i] == '\n') return false;
    }

    const DWORD freeBytes = getFreeSpaceBytes();
    if (freeBytes != 0 && freeBytes < (DWORD)n + 10) return false;

    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);
    FIL f{};
    FRESULT fr = f_open(&f, path, FA_OPEN_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return false;

    if (!checkAndRotateFile(f, path)) {
        f_close(&f);
        return false;
    }

    f_lseek(&f, f_size(&f));
    bool ok = writeLineWithRetry(f, jsonLine, (UINT)n, 3);
    if (ok) {
        const char eol[] = "\r\n";
        writeLineWithRetry(f, eol, 2, 3);
        syncWithRetry(f, 3);
    }
    f_close(&f);
    return ok;
}

bool SdBackup::readChunkAsJsonArray(char* out, uint32_t outSize, uint32_t maxPayloadBytes,
                                   uint32_t& linesRead, FSIZE_t& bytesUsedFromFile)
{
    linesRead = 0; bytesUsedFromFile = 0;
    if (!m_mounted || !out || outSize < 4) return false;

    char path[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);
    FIL f{};
    if (f_open(&f, path, FA_READ) != FR_OK) return false;

    uint32_t off = 0;
    out[off++] = '[';
    char line[Config::JSONL_LINE_MAX + 4];

    while (f_gets(line, sizeof(line), &f)) {
        size_t n = std::strlen(line);
        while (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n' || line[n-1] == ' ')) {
            line[--n] = '\0';
        }
        if (n == 0) {
            bytesUsedFromFile = f_tell(&f);
            continue;
        }
        uint32_t need = (linesRead ? 1u : 0u) + (uint32_t)n + 2u;
        if (off + need > maxPayloadBytes || off + need > outSize) break;

        if (linesRead) out[off++] = ',';
        std::memcpy(&out[off], line, n);
        off += (uint32_t)n;
        linesRead++;
        bytesUsedFromFile = f_tell(&f);
    }
    out[off++] = ']';
    out[off] = '\0';
    f_close(&f);
    return (linesRead > 0);
}

bool SdBackup::consumePrefix(FSIZE_t bytesUsedFromFile)
{
    if (!m_mounted || bytesUsedFromFile == 0) return true;
    char path[64], tmpPath[64];
    make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);
    make_full_path(tmpPath, sizeof(tmpPath), "backup.tmp");

    FIL src{}, dst{};
    if (f_open(&src, path, FA_READ) != FR_OK) return false;

    if (bytesUsedFromFile >= f_size(&src)) {
        f_close(&src);
        return (f_unlink(path) == FR_OK);
    }

    if (f_open(&dst, tmpPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        f_close(&src); return false;
    }

    f_lseek(&src, bytesUsedFromFile);
    uint8_t buf[512];
    UINT br, bw;
    FRESULT fr = FR_OK;
    while (f_read(&src, buf, sizeof(buf), &br) == FR_OK && br > 0) {
        fr = f_write(&dst, buf, br, &bw);
        if (fr != FR_OK || bw != br) break;
    }
    f_close(&src); f_close(&dst);

    if (fr == FR_OK) {
        f_unlink(path);
        f_rename(tmpPath, path);
        return true;
    }
    f_unlink(tmpPath);
    return false;
}
