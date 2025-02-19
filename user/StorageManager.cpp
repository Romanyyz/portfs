#include "StorageManager.h"

#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/mount.h>

#include <iostream>
#include <fstream>
#include <vector>

StorageManager::StorageManager(const std::string& storageFileName) : storageFilePath_("/var/tmp/" + storageFileName)
{
}


std::string StorageManager::createFile(uint64_t fileSizeInBytes)
{
    int fd = open(storageFilePath_.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd == -1)
    {
        std::cerr << "\nCould not create file " << storageFilePath_;
        close(fd);
        return {};
    }
    if (ftruncate(fd, fileSizeInBytes) == -1)
    {
        std::cerr << "\nCould not set file size.";
        close(fd);
        return {};
    }

    close(fd);
    storageFileSizeInBytes_ = fileSizeInBytes;
    return storageFilePath_;
}


int  StorageManager::formatStorage()
{
    portfs_superblock msb = createSuperblock();

    if (writeSuperblock(msb) != 0)
    {
        return -1;
    }
    if (writeFileTable(msb) != 0)
    {
        return -1;
    }
    if (writeBlockBitmap(msb) != 0)
    {
        return -1;
    }

    return 0;
}


portfs_superblock StorageManager::createSuperblock()
{
    portfs_superblock msb{};

    msb.magic_number    = portfsMagic_;
    msb.block_size      = blockSize_;
    msb.total_blocks    = storageFileSizeInBytes_ / msb.block_size;
    msb.filetable_start = 1;

    uint32_t maxFilesCount       = storageFileSizeInBytes_ / averageFileSize_;
    uint32_t filetableSizeBytes  = maxFilesCount * sizeof(filetable_entry);
    uint32_t filetableSizeBlocks = filetableSizeBytes / msb.block_size;

    msb.filetable_size     = filetableSizeBlocks;
    msb.block_bitmap_start = msb.filetable_start + msb.filetable_size;

    uint32_t blockBitmapSizeBytes  = msb.total_blocks / 8;
    uint32_t blockBitmapSizeBlocks = blockBitmapSizeBytes / msb.block_size;

    msb.block_bitmap_size = blockBitmapSizeBlocks;
    msb.data_start        = msb.block_bitmap_start + msb.block_bitmap_size;
    msb.max_file_count    = maxFilesCount;
    msb.checksum          = 0;

    msb.last_mount_time = 0;
    msb.last_write_time = 0;
    msb.flags           = 0;

    return msb;
}


int StorageManager::writeSuperblock(const portfs_superblock& msb)
{
    portfs_superblock dsb{};
    dsb.magic_number       = htobe32(msb.magic_number);
    dsb.block_size         = htobe32(msb.block_size);
    dsb.total_blocks       = htobe32(msb.total_blocks);
    dsb.filetable_start    = htobe32(msb.filetable_start);
    dsb.filetable_size     = htobe32(msb.filetable_size);
    dsb.block_bitmap_start = htobe32(msb.block_bitmap_start);
    dsb.block_bitmap_size  = htobe32(msb.block_bitmap_size);
    dsb.data_start         = htobe32(msb.data_start);
    dsb.max_file_count     = htobe32(msb.max_file_count);
    dsb.checksum           = htobe32(msb.checksum);
    dsb.last_mount_time    = htobe32(msb.last_mount_time);
    dsb.last_write_time    = htobe32(msb.last_write_time);
    dsb.flags              = htobe32(msb.flags);

    std::ofstream file(storageFilePath_, std::ios::binary | std::ios::out | std::ios::in);
    if (!file.is_open())
    {
        std::cerr << "Error opening file for writing: " << storageFilePath_ << '\n';
        return -1;
    }

    file.write(reinterpret_cast<const char*>(&dsb), sizeof(dsb));
    file.close();

    std::cout << "\nSuperblock written successfully.";

    return 0;
}


int StorageManager::writeFileTable(const portfs_superblock& msb)
{
    std::ofstream file(storageFilePath_, std::ios::binary | std::ios::out | std::ios::in);
    if (!file.is_open())
    {
        std::cerr << "Error opening file for writing: " << storageFilePath_ << '\n';
        return -1;
    }

    file.seekp(msb.filetable_start * msb.block_size);
    if (!file)
    {
        std::cerr << "Failed to seek to offset.\n";
        file.close();
        return -1;
    }

    constexpr size_t BUFFER_SIZE{1 * 1024 * 1024}; // 1MB
    filetable_entry entry{};
    entry.name[0] = '\0';
    entry.sizeInBytes = 0;
    entry.extentCount = 0;
    entry.ino = 0;
    for (size_t i = 0; i < MAX_EXTENTS; ++i)
    {
        entry.extents[i] = {0,0};
    }

    std::vector<filetable_entry> buffer;
    buffer.reserve(BUFFER_SIZE / sizeof(filetable_entry));

    for (size_t i = 0; i < msb.max_file_count; ++i)
    {
        buffer.push_back(entry);

        if (buffer.size() * sizeof(filetable_entry) >= BUFFER_SIZE)
        {
            file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(filetable_entry));
            buffer.clear();
        }
    }

    if (!buffer.empty())
    {
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(filetable_entry));
    }

    std::cout << "\nFiletable written successfully.";

    file.close();
    return 0;
}


int StorageManager::writeBlockBitmap(const portfs_superblock& msb)
{
    std::ofstream file(storageFilePath_, std::ios::binary | std::ios::out | std::ios::in);
    if (!file.is_open())
    {
        std::cerr << "Error opening file for writing: " << storageFilePath_ << '\n';
        return -1;
    }

    file.seekp(msb.block_bitmap_start * msb.block_size);
    if (!file)
    {
        std::cerr << "Failed to seek to offset.\n";
        file.close();
        return -1;
    }

    constexpr size_t BUFFER_SIZE{1 * 1024 * 1024};
    std::vector<std::byte> buffer(BUFFER_SIZE, std::byte{0});
    size_t remainingBytes = msb.block_bitmap_size * msb.block_size;
    while (remainingBytes > 0)
    {
        size_t bytesToWrite = std::min(BUFFER_SIZE, remainingBytes);
        file.write(reinterpret_cast<const char*>(buffer.data()), bytesToWrite);
        remainingBytes -= bytesToWrite;
    }

    std::cout << "\nBlock bitmap written successfully.";

    file.close();
    return 0;
}


int StorageManager::mountPortfs(const std::filesystem::path& mountDirPath,
                                const std::filesystem::path& storageFilePath)
{
    const std::string source{"none"};
    const std::string filesystemtype{"portfs"};
    const std::string options{std::string{"path="} + storageFilePath.c_str()};

    if (mount(source.c_str(), mountDirPath.c_str(),
              filesystemtype.c_str(), 0, options.c_str()) != 0)
    {
        perror("Mount failed");
        return -1;
    }

    std::cout << "\nPortFS mounted successfully!" << std::endl;
    return 0;
}

