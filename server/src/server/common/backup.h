#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>

inline bool BackupBinary(const std::filesystem::path& backup_path, const std::string& bin) {
    std::ofstream ofs(backup_path, std::ios::binary);
    if (!ofs) { throw std::runtime_error("Failed to open backup file: " + backup_path.string()); }

    ofs.write(bin.data(), static_cast<std::streamsize>(bin.size()));

    if (!ofs) { return false; }
    ofs.close();

    return true;
}
