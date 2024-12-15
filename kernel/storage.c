#include "portfs.h"

int storage_init(void)
{
    umode_t mode = S_IRUSR | S_IWUSR;
    storage_filp = filp_open(storage_path, O_RDWR, mode);
    if (IS_ERR(storage_filp))
    {
        pr_err("Failed to open file: %s\n", storage_path);
        return (int)PTR_ERR(storage_filp);
    }
    if (storage_filp->f_inode)
    {
        loff_t file_size = storage_filp->f_inode->i_size;
        total_blocks = file_size / BLOCK_SIZE;
    }
    else
    {
        pr_err("Invalid inode for file %s\n", storage_path);
    }
    return 0;
}
