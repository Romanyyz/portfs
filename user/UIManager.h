#pragma once

#include "StorageManager.h"

#include <string>

class UIManager
{
public:
    void start();

private:
    StorageManager storageManager{"storage.pfs"};

    void stringToLower(std::string& str);
    void askCreateFile();
    void askFileSize();
    void askExistingFile();
    void askMountPortfs(const std::string& storageFilePath);

};
