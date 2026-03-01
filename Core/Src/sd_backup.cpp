#include "sd_backup.hpp"
#include "debug_uart.hpp"

#include <cstdio>
#include <cstring>

static const char* frStr(FRESULT fr)
{
  switch (fr) {
    case FR_OK:             return "FR_OK";
    case FR_DISK_ERR:       return "FR_DISK_ERR";
    case FR_INT_ERR:        return "FR_INT_ERR";
    case FR_NOT_READY:      return "FR_NOT_READY";
    case FR_NO_FILE:        return "FR_NO_FILE";
    case FR_NO_PATH:        return "FR_NO_PATH";
    case FR_INVALID_NAME:   return "FR_INVALID_NAME";
    case FR_DENIED:         return "FR_DENIED";
    case FR_EXIST:          return "FR_EXIST";
    case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
    case FR_WRITE_PROTECTED:return "FR_WRITE_PROTECTED";
    case FR_INVALID_DRIVE:  return "FR_INVALID_DRIVE";
    case FR_NOT_ENABLED:    return "FR_NOT_ENABLED";
    case FR_NO_FILESYSTEM:  return "FR_NO_FILESYSTEM";
    case FR_MKFS_ABORTED:   return "FR_MKFS_ABORTED";
    case FR_TIMEOUT:        return "FR_TIMEOUT";
    case FR_LOCKED:         return "FR_LOCKED";
    case FR_NOT_ENOUGH_CORE:return "FR_NOT_ENOUGH_CORE";
    case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
    case FR_INVALID_PARAMETER:   return "FR_INVALID_PARAMETER";
    default:                return "FR_???";
  }
}

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
  std::snprintf(out, out_sz, "%s/%s", drive, fname); // "0:/backup.json"
}

bool SdBackup::init()
{
  char drive[3];
  make_drive(drive, sizeof(drive));

  FRESULT fr = f_mount(&m_fatfs, drive, 1);
  if (fr != FR_OK) {
    DBG.error("SD: mount fail drive=%s (FR=%d %s)", drive, (int)fr, frStr(fr));
    m_mounted = false;
    return false;
  }

  m_mounted = true;
  DBG.info("SD: mounted drive=%s", drive);
  return true;
}

void SdBackup::deinit()
{
  if (!m_mounted) return;

  char drive[3];
  make_drive(drive, sizeof(drive));
  f_mount(nullptr, drive, 0);

  m_mounted = false;
  DBG.info("SD: unmounted drive=%s", drive);
}

bool SdBackup::exists() const
{
  if (!m_mounted) return false;

  char path[64];
  make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);

  FILINFO fno;
  FRESULT fr = f_stat(path, &fno);
  return (fr == FR_OK);
}

bool SdBackup::remove()
{
  if (!m_mounted) return false;

  char path[64];
  make_full_path(path, sizeof(path), Config::BACKUP_FILENAME);

  FRESULT fr = f_unlink(path);
  if (fr != FR_OK) {
    DBG.error("SD: unlink fail path=%s (FR=%d %s)", path, (int)fr, frStr(fr));
    return false;
  }
  return true;
}

bool SdBackup::appendLine(const char* jsonLine)
{
  if (!m_mounted || !jsonLine) return false;

  const size_t n = std::strlen(jsonLine);

  if (n == 0 || n > Config::JSONL_LINE_MAX) {
    DBG.error("SD: JSONL line too long (%u)", (unsigned)n);
    return false;
  }

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
  if (fr != FR_OK) {
    DBG.error("SD: open for append fail path=%s (FR=%d %s)", path, (int)fr, frStr(fr));
    return false;
  }

  fr = f_lseek(&f, f_size(&f));
  if (fr != FR_OK) {
    DBG.error("SD: lseek(end) fail path=%s (FR=%d %s)", path, (int)fr, frStr(fr));
    f_close(&f);
    return false;
  }

  UINT bw = 0;
  fr = f_write(&f, jsonLine, (UINT)n, &bw);
  if (fr != FR_OK || bw != (UINT)n) {
    DBG.error("SD: write line fail path=%s (FR=%d %s bw=%u/%u)",
              path, (int)fr, frStr(fr), (unsigned)bw, (unsigned)n);
    f_close(&f);
    return false;
  }

  const char eol[] = "\r\n";
  UINT bw2 = 0;
  fr = f_write(&f, eol, sizeof(eol) - 1, &bw2);
  if (fr != FR_OK || bw2 != (sizeof(eol) - 1)) {
    DBG.error("SD: write EOL fail path=%s (FR=%d %s bw=%u/%u)",
              path, (int)fr, frStr(fr), (unsigned)bw2, (unsigned)(sizeof(eol) - 1));
    f_close(&f);
    return false;
  }

  fr = f_sync(&f);
  if (fr != FR_OK) {
    DBG.error("SD: sync fail path=%s (FR=%d %s)", path, (int)fr, frStr(fr));
    f_close(&f);
    return false;
  }

  f_close(&f);
  return true;
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
  if (fr != FR_OK) {
    DBG.error("SD: open for read fail path=%s (FR=%d %s)", path, (int)fr, frStr(fr));
    return false;
  }

  uint32_t off = 0;
  out[off++] = '[';

  char line[Config::JSONL_LINE_MAX + 4];
  FSIZE_t lastPosAfterLine = 0;

  while (true) {
    char* s = f_gets(line, sizeof(line), &f);
    if (!s) break; // EOF

    lastPosAfterLine = f_tell(&f);

    size_t n = std::strlen(line);
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ' || line[n - 1] == '\t')) {
      line[--n] = '\0';
    }

    if (n == 0) {
      bytesUsedFromFile = lastPosAfterLine;
      continue;
    }

    uint32_t need = (linesRead ? 1u : 0u) + (uint32_t)n + 1u + 1u; // comma + line + ']' + '\0'
    if (off + need > maxPayloadBytes) break;
    if (off + need > outSize) break;

    if (linesRead) out[off++] = ',';
    std::memcpy(&out[off], line, n);
    off += (uint32_t)n;

    linesRead++;
    bytesUsedFromFile = lastPosAfterLine;
  }

  out[off++] = ']';
  out[off] = '\0';

  f_close(&f);

  if (!(linesRead > 0 && bytesUsedFromFile > 0)) {
    DBG.error("SD: readChunk got empty (lines=%u used=%lu) path=%s",
              (unsigned)linesRead, (unsigned long)bytesUsedFromFile, path);
    return false;
  }

  return true;
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
  if (fr != FR_OK) {
    DBG.error("SD: consume open src fail path=%s (FR=%d %s)", path, (int)fr, frStr(fr));
    return false;
  }

  FSIZE_t sz = f_size(&src);
  if (bytesUsedFromFile >= sz) {
    f_close(&src);
    fr = f_unlink(path);
    if (fr != FR_OK) {
      DBG.error("SD: consume unlink fail path=%s (FR=%d %s)", path, (int)fr, frStr(fr));
      return false;
    }
    return true;
  }

  FIL dst{};
  fr = f_open(&dst, tmpPath, FA_CREATE_ALWAYS | FA_WRITE);
  if (fr != FR_OK) {
    DBG.error("SD: consume open dst fail path=%s (FR=%d %s)", tmpPath, (int)fr, frStr(fr));
    f_close(&src);
    return false;
  }

  fr = f_lseek(&src, bytesUsedFromFile);
  if (fr != FR_OK) {
    DBG.error("SD: consume lseek src fail used=%lu (FR=%d %s)", (unsigned long)bytesUsedFromFile, (int)fr, frStr(fr));
    f_close(&src);
    f_close(&dst);
    f_unlink(tmpPath);
    return false;
  }

  uint8_t buf[512];
  UINT br = 0, bw = 0;

  while (true) {
    fr = f_read(&src, buf, sizeof(buf), &br);
    if (fr != FR_OK) {
      DBG.error("SD: consume read fail (FR=%d %s)", (int)fr, frStr(fr));
      break;
    }
    if (br == 0) { fr = FR_OK; break; }

    fr = f_write(&dst, buf, br, &bw);
    if (fr != FR_OK || bw != br) {
      DBG.error("SD: consume write fail (FR=%d %s bw=%u/%u)", (int)fr, frStr(fr), (unsigned)bw, (unsigned)br);
      fr = FR_DISK_ERR;
      break;
    }
  }

  f_close(&src);
  f_close(&dst);

  if (fr != FR_OK) {
    f_unlink(tmpPath);
    return false;
  }

  f_unlink(path);
  fr = f_rename(tmpPath, path);
  if (fr != FR_OK) {
    DBG.error("SD: consume rename fail (FR=%d %s)", (int)fr, frStr(fr));
    f_unlink(tmpPath);
    return false;
  }

  return true;
}
