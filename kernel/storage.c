#include "portfs.h"
#include "../common/shared_structs.h"

struct file* portfs_storage_init(char *path)
{
    pr_info("portfs_storage_init: Recieved file path %s\n", path);
    umode_t mode = S_IRUSR | S_IWUSR;
    struct file *storage_filp = NULL;
    storage_filp = filp_open(path, O_RDWR | O_LARGEFILE, mode);
    if (IS_ERR(storage_filp))
    {
        pr_err("portfs_storage_init: Failed to open file: %s, error: %ld\n", path, PTR_ERR(storage_filp));
    }

    return storage_filp;
}
