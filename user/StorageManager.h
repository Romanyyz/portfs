#pragma once

#include <cstdint>
#include <string>

#include "../common/shared_structs.h"

class StorageManager
{
public:
    StorageManager(const std::string& fileName);

    std::string createFile(uint64_t fileSize);
    void formatStorage(const std::string& filePath);
    void mountPortfs(const std::string& mountDirPath, const std::string& filePath);

private:
    std::string storageFileName_;
    uint64_t fileSizeInBytes_{0};
    const uint64_t averageFileSize_{1 * 1024 * 1024};
    const uint32_t blockSize_{4096};
    const uint32_t portfsMagic_{0x506F5254};
    portfs_superblock fillSuperblock();
};
