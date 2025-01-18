#include "UIManager.h"

#include <iostream>

void UIManager::stringToLower(std::string& str)
{
    for (char& c : str)
        c = std::tolower(static_cast<unsigned char>(c));
}


void UIManager::start()
{
    askCreateFile();
}


void UIManager::askCreateFile()
{
    std::cout << "\nDo you want to create a storage file? (y/n): ";
    std::string answer;
    std::cin >> answer;
    stringToLower(answer);

    if (answer == "yes" || answer == "y")
        askFileSize();
    else if (answer == "no" || answer == "n")
        askExistingFile();
    else
    {
        std::cout << "\nWrong input. Please enter \'Yes\' or \'No\'.";
        askCreateFile();
    }
}


void UIManager::askFileSize()
{
    std::cout << "\nEnter file size (in GB): ";
    size_t fileSize{0};
    std::cin >> fileSize;

    size_t fileSizeInBytes = fileSize * 1024 * 1024 * 1024;

    std::cout << "\nFile with size " << fileSize << "GB will be created in directory /var/tmp/.";
    std::string filePath = storageManager.createFile(fileSizeInBytes);
    if (!filePath.empty())
    {
        std::cout << "\nFile successfuly created.";
        std::cout << "\nFormatting file...";
        storageManager.formatStorage(filePath);
        askMountPortfs(filePath);
    }
    else
        std::cerr << "\nError happened during creation of file. Exiting...";
}


void UIManager::askExistingFile()
{
    std::cout << "\nDo you have an existing file? (y/n): ";
    std::string answer;
    std::cin >> answer;
    stringToLower(answer);

    if (answer == "yes" || answer == "y")
    {
        std::cout << "\nEnter path to existing file: ";
        std::string filePath;
        std::cin >> filePath;
        std::cout << "\nFile " << filePath << " will be used.";
        askMountPortfs(filePath);
    }
    else if (answer == "no" || answer == "n")
    {
        std::cout << "\nStorage file is required to continue.";
        askCreateFile();
    }
    else
    {
        std::cout << "\nWrong input. Please enter \'Yes\' or \'No\'.";
        askExistingFile();
    }
}


void UIManager::askMountPortfs(const std::string& storageFilePath)
{
    std::cout << "\nDo you want to mount portfs on file " << storageFilePath << "? (y/n): ";
    std::string answer;
    std::cin >> answer;
    stringToLower(answer);

    if (answer == "yes" || answer == "y")
    {
        std::cout << "\nEnter path to mount directory: ";
        std::string mountDirPath;
        std::cin >> mountDirPath;
        std::cout << "\nDirectory " << mountDirPath << " will be used.";
        storageManager.mountPortfs(mountDirPath, storageFilePath);
    }
    else if (answer == "no" || answer == "n")
    {
        return;
    }
    else
    {
        std::cout << "\nWrong input. Please enter \'Yes\' or \'No\'.";
        askMountPortfs(storageFilePath);
    }
}
