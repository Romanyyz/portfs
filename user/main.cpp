#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <string>
#include <cctype>

void askCreateFile();
void askFileSize();
void askExistingFile();
void askMountPortfs(const std::string& storageFilePath);

std::string createFile(size_t fileSize);
void mountPortfs(const std::string& mountDirPath, const std::string& filePath);

void stringToLower(std::string& str);

const std::string storageFileName{"storage.pfs"};

int main()
{
    askCreateFile();
    return 0;
}


void stringToLower(std::string& str)
{
    for (char& c : str)
        c = std::tolower(static_cast<unsigned char>(c));
}


void askCreateFile()
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


void askFileSize()
{
    std::cout << "\nEnter file size (in GB): ";
    size_t fileSize{0};
    std::cin >> fileSize;

    std::cout << "\nFile with size " << fileSize << "GB and name \'" << storageFileName << "\' will be created in directory /var/tmp/.";
    std::string filePath = createFile(fileSize);
    if (!filePath.empty())
    {
        std::cout << "\nFile successfuly created.";
        askMountPortfs(filePath);
    }
    else
        std::cerr << "\nError happened during creation of file. Exiting...";
}


void askExistingFile()
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
        std::cout << "\nYou need a storage file to continue.";
        askCreateFile();
    }
    else
    {
        std::cout << "\nWrong input. Please enter \'Yes\' or \'No\'.";
        askExistingFile();
    }
}


void askMountPortfs(const std::string& storageFilePath)
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
        mountPortfs(mountDirPath, storageFilePath);
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


std::string createFile(size_t fileSize)
{
    const std::string filePath{"/var/tmp/" + storageFileName};
    
    int fd = open(filePath.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd == -1)
    {
        std::cerr << "\nCould not create file " << filePath;
        return {};
    }
    fileSize = fileSize * 1024 * 1024 * 1024; // Convert GigaBytes to Bytes
    if (ftruncate(fd, fileSize) == -1)
    {
        std::cerr << "\nCould not set file size: " << fileSize << "MB";
        return {};
    }

    close(fd);
    return filePath;
}


void mountPortfs(const std::string& mountDirPath, const std::string& filePath)
{
    std::cout<<mountDirPath<<filePath;
}

