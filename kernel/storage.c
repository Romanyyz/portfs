#include "portfs.h"

struct file* storage_init(char *path)
{
    pr_info("storage_init: Recieved file path %s\n", path);
    umode_t mode = S_IRUSR | S_IWUSR;
    struct file *storage_filp = NULL;
    storage_filp = filp_open(path, O_RDWR, mode);
    if (IS_ERR(storage_filp))
    {
        pr_err("storage_init: Failed to open file: %s, error: %ld\n", path, PTR_ERR(storage_filp));
        return storage_filp;
    }
    if (storage_filp->f_inode)
    {
        loff_t file_size = storage_filp->f_inode->i_size;
        total_blocks = file_size / BLOCK_SIZE;
    }
    else
    {
        pr_err("Invalid inode for file %s\n", path);
        return ERR_PTR(-EINVAL);
    }
    return storage_filp;
}
