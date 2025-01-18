#include "StorageManager.h"

#include <fcntl.h>
#include <unistd.h>

#include <iostream>

StorageManager::StorageManager(const std::string& storageFileName) : storageFileName_(storageFileName)
{
}


std::string StorageManager::createFile(uint64_t fileSizeInBytes)
{
    const std::string filePath{"/var/tmp/" + storageFileName_};

    int fd = open(filePath.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd == -1)
    {
        std::cerr << "\nCould not create file " << filePath;
        close(fd);
        return {};
    }
    if (ftruncate(fd, fileSizeInBytes) == -1)
    {
        std::cerr << "\nCould not set file size.";
        close(fd);
        return {};
    }

    fileSizeInBytes_ = fileSizeInBytes;
    close(fd);
    return filePath;
}


void StorageManager::formatStorage(const std::string& filePath)
{
    portfs_superblock msb = fillSuperblock();
}


void StorageManager::mountPortfs(const std::string& mountDirPath, const std::string& filePath)
{
    std::cout<<mountDirPath<<filePath;
}


portfs_superblock StorageManager::fillSuperblock()
{
    portfs_superblock msb{};
    msb.magic_number = portfsMagic_;
    msb.block_size = blockSize_;
    msb.total_blocks = fileSizeInBytes_ / msb.block_size;

    return msb;
}
