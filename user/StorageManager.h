#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

#include "../common/shared_structs.h"

class StorageManager
{
public:
    StorageManager(const std::string& fileName);

    std::string createFile(uint64_t fileSize);
    int formatStorage();
    int mountPortfs(const std::filesystem::path& mountDirPath,
                    const std::filesystem::path& storageFilePath);

private:
    portfs_superblock createSuperblock();
    int writeSuperblock(const portfs_superblock& msb);
    int writeFileTable(const portfs_superblock& msb);
    int writeBlockBitmap(const portfs_superblock& msb);

    std::filesystem::path storageFilePath_;
    uint64_t storageFileSizeInBytes_{0};
    const uint64_t averageFileSize_{1 * 1024 * 1024};
    const uint32_t blockSize_{4096};
    const uint32_t portfsMagic_{0x506F5254};
};
