#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nao <n.chika156@gmail.com>");
MODULE_INFO(free_form_info, "You can write the string here freely");
MODULE_DESCRIPTION("This moduel is for testing");

#define DEV_NAME  "debimate"
#define DEV_CLASS "debimate_class"
#define MINOR_NR_BASE 0
#define MAX_MINOR_NR  1

static struct class *debimate_class;
static struct cdev   debimate_cdev;
static dev_t dev;

struct str_list {
  char  *s;
  struct list_head list;
};

static int debimate_open(struct inode *inode, struct file *file)
{
    struct str_list *s_list = NULL;

    pr_info("START: %s\n", __func__);

    /* リスト用のメモリを確保 */
    s_list = kmalloc(sizeof(struct str_list), GFP_KERNEL);
    if(!s_list) {
        pr_err("ERR  : Can't alloc memory(%s)\n", __func__);
        pr_err("ERR  : Can't open debimate(%s)\n", __func__);
        goto MEM_ALLOC_ERR;
    }

    /* Listを初期化し、file(device)毎の個別データにListを渡す。
     * 他のsystemcall(write, readなど)は、private_data経由で、
     * Listを操作する。*/
    INIT_LIST_HEAD(&s_list->list);
    file->private_data = (void *)s_list;

    pr_info("END  : %s\n", __func__);
    return 0;

MEM_ALLOC_ERR:
    return -ENOMEM;
}

static int debimate_close(struct inode *inode, struct file *file)
{
    int    loop_cnt       = 0;
    struct str_list *itr  = NULL;
    struct str_list *tmp  = NULL;
    struct str_list *head = (struct str_list*)file->private_data;

    pr_info("START: %s\n", __func__);

    list_for_each_entry_safe(itr ,tmp, &head->list, list){
        pr_info("loop number: %d", loop_cnt);
        kfree(itr->s);
        list_del(&itr->list);
        kfree(itr);
        loop_cnt++;
    }

    pr_info("END  : %s\n", __func__);
    return 0;
}

static ssize_t debimate_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
    int loop_cnt           = 0;
    ssize_t size           = 0;
    ssize_t total_size     = 0;
    char   *str            = NULL;
    char   *str_head       = NULL;
    struct  str_list *itr  = NULL;
    struct  str_list *head = (struct str_list*)filp->private_data;
    unsigned long result   = 0;

    pr_info("START: %s\n", __func__);

    /* 文字列コピー用のメモリを確保 */
    str = kmalloc(count+1, GFP_KERNEL);
    if(!str) {
        pr_err("ERR  : Can't alloc memory for string(%s)\n", __func__);
        goto STR_MEM_ALLOC_ERR;
    }
    memset(str, '\0', count+1);

    /* Listを先頭から順に探索し、リストの要素(文字列)を連結する。
     * 文字列の連結は、ユーザが読み込みたいByte数と一致するまで続く。 */
    str_head = str;
    list_for_each_entry(itr, &head->list, list) {
        size = strlen(itr->s);
        pr_info("loop count=%d: %s is %ld byte\n", loop_cnt, itr->s, size);
        if((total_size += size) > count) {
            strncpy(str, itr->s, (total_size - count));
            total_size = count;
            break;
        }
        strncpy(str, itr->s, size);
        str = str + size;
        loop_cnt++;
    }

    /* ユーザ空間に文字列をコピー */
    result = copy_to_user(buf, str_head, total_size);
    if (result) {
        pr_err("ERR  : Can't copy data to user space(result=%ld)\n",
                result);
        goto COPY_DATA_ERR;
    }
    pr_info("Copy %s from kernel space to user space(result=%ld))\n",
            str_head, result);

    kfree(str_head);
    pr_info("END  : %s\n", __func__);

    return total_size;

COPY_DATA_ERR:
    kfree(str);
STR_MEM_ALLOC_ERR:
    return result;
}

static ssize_t debimate_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
    int    result           = 0;
    char  *str              = NULL;
    struct str_list *s_list = NULL;
    struct str_list *head   = (struct str_list*)filp->private_data;

    pr_info("START: %s\n", __func__);

    /* リスト用のメモリを確保 */
    s_list = kmalloc(sizeof(struct str_list), GFP_KERNEL);
    if(!s_list) {
        pr_err("ERR  : Can't alloc memory for list(%s)\n", __func__);
        goto LIST_MEM_ALLOC_ERR;
    }

    /* 文字列コピー用のメモリを確保 */
    str = kmalloc(count+1, GFP_KERNEL);
    if(!str) {
        pr_err("ERR  : Can't alloc memory for string(%s)\n", __func__);
        goto STR_MEM_ALLOC_ERR;
    }
    memset(str, '\0', count+1);

    /* ユーザ空間メモリ領域からカーネル空間メモリ領域にデータをコピー */
    result = strncpy_from_user(str, buf, count);
    if(result != count) {
        pr_err("ERR  : Can't copy data from user space to kernel space(%s)\n",
               __func__);
        goto WRITE_DATA_ERR;
    }
    s_list->s = str;
    list_add_tail(&s_list->list ,&head->list); /* リストへ挿入 */

    pr_info("END  : %s\n", __func__);
    return result;

WRITE_DATA_ERR:
    kfree(str);
STR_MEM_ALLOC_ERR:
    kfree(s_list);
LIST_MEM_ALLOC_ERR:
    return -ENOMEM;
}

static struct file_operations debimate_drv_fops ={
	.owner    = THIS_MODULE,
	.open     = debimate_open,
	.release  = debimate_close,
	.read     = debimate_read,
	.write    = debimate_write,
};

static int __init debimate_init(void)
{
    int result                  = 0;
    struct device *debimate_dev = NULL;

    pr_info(KERN_INFO "START: %s\n", __func__);

    /* メジャー番号の動的確保 */
    result = alloc_chrdev_region(&dev, MINOR_NR_BASE,
                                 MAX_MINOR_NR, DEV_NAME);
    if (result) {
        pr_err("%s: alloc_chrdev_region = %d\n", __func__, result);
        goto REGION_ERR;
    }

    /* デバイスをクラス登録 */
    debimate_class = class_create(THIS_MODULE, DEV_CLASS);
    if(IS_ERR(debimate_class)) {
        result = PTR_ERR(debimate_class);
        pr_err("%s: class_create = %d\n", __func__, result);
        goto CREATE_CLASS_ERR;
    }

    /* キャラクターデバイス構造体(cdev構造体)初期化および
     * システムコールの関数ポインタ登録 */
    cdev_init(&debimate_cdev, &debimate_drv_fops);

    /* キャラクターデバイスをKernelに登録 */
    result = cdev_add(&debimate_cdev, dev, MAX_MINOR_NR);
    if (result) {
        pr_err("%s: cdev_add = %d\n", __func__, result);
        goto CDEV_ADD_ERR;
    }

    /* デバイスノードを作成。作成したノードは/dev以下からアクセス可能 */
    debimate_dev = device_create(debimate_class, NULL, dev, NULL,
                                 DEV_NAME, MINOR_NR_BASE);
    if (IS_ERR(debimate_dev)) {
        result = PTR_ERR(debimate_dev);
        pr_err("%s: device_create = %d\n", __func__, result);
        goto DEV_CREATE_ERR;
    }
    pr_info(KERN_INFO "END  : %s\n", __func__);
    return result;

DEV_CREATE_ERR:
    cdev_del(&debimate_cdev);
CDEV_ADD_ERR:
    class_destroy(debimate_class);
CREATE_CLASS_ERR:
    unregister_chrdev_region(dev, MAX_MINOR_NR);
REGION_ERR:
    return result;
}

static void __exit debimate_exit(void)
{
    pr_info(KERN_INFO "START: %s\n", __func__);

    /* デバイスノードの削除 */
    device_destroy(debimate_class, dev);

    /* キャラクターデバイスをKernelから削除 */
    cdev_del(&debimate_cdev);

    /* デバイスのクラス登録を削除 */
    class_destroy(debimate_class);

    /* デバイスが使用していたメジャー番号の登録を解除 */
    unregister_chrdev_region(dev, MAX_MINOR_NR);

    pr_info(KERN_INFO "END  : %s\n", __func__);
}

module_init(debimate_init);
module_exit(debimate_exit);
