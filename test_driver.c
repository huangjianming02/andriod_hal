#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>		//kmolloc
#include <linux/of.h>
#include <linux/platform_device.h>

//#include <linux/kernel.h>
//#include <linux/proc_fs.h>

#define	MAJORS	200		//主设备号
#define	MY_MINOR	0		//次设备号基址
#define	MY_MINORS_ALLOMAX		1		//从设备的个数
#define	DEVICE_NAME		"chrdev"	//设备的名字


static struct class *chrdev_class;
struct chrdev_devices{
	char val;
	struct cdev my_cdev;
};
struct chrdev_devices *roger_dev;


// DEVICE_ATTR,会生成一个结构体dev_attr_roger.
// 在proe函数里需要调用 sysfs_create_file 函数才会生成对应的文件。
//static DEVICE_ATTR(roger, 0660, roger_show, roger_store);

static int chrdev_open(struct inode *node, struct file *filp)
{	
	//获取到当前自定义结构体变量，保存至file中，以便其他函数方便访问
   	struct chrdev_devices *n_cdev = container_of(node->i_cdev,struct chrdev_devices,my_cdev);
    	filp->private_data = n_cdev;
	printk("This is a test/n");
	return 0;
}

static ssize_t chrdev_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
	struct chrdev_devices *n_cdev = (struct chrdev_devices *)filp->private_data;
	int err;
   	printk("test2 read %c  count : %d",n_cdev->val, sizeof(n_cdev->val));
    	if(copy_to_user(buf,&n_cdev->val, sizeof(n_cdev->val))){
        	err = -EFAULT;
        	return err;
    	}
    	return sizeof(n_cdev->val);
}

static ssize_t chrdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{	
	struct chrdev_devices *n_cdev = (struct chrdev_devices *)filp->private_data;
	int err;
    	//printk("test2 write %s  count : %d",(*buf),count); //调用这个函数会死机,应该是*buf是空指针的问题
   	 if(copy_from_user(&n_cdev->val,buf,count)){
        	err = -EFAULT;
       	 	return err;
    	}
    	return sizeof(n_cdev->val);
}

int chrdev_release(struct inode *node, struct file *filp)
{
	//printk("This is a test/n");
	filp->private_data = NULL;
	return 0;
}


//为/dev/chrdev_roger这个文件提供操作接口
static const struct file_operations chrdev_fops = {
	.owner		= THIS_MODULE,
	.open		= chrdev_open,
	.read		= chrdev_read,
	.write		= chrdev_write,
	.release	= chrdev_release,
};

static int chrdev_probe(struct platform_device *pdev)
{
	int res = -1;
	struct device *dev = &pdev->dev;
	
	//申请设备号----申请成功后，cat /proc/devices可以查看到设备的名字DEVICE_NAME
	res = register_chrdev_region(MKDEV(MAJORS, MY_MINOR), MY_MINORS_ALLOMAX, DEVICE_NAME);
	if(res < 0){
		printk(KERN_ERR "register_chrdev_region error.\n");
		goto out;
	}

	//向内核申请分配内存
	roger_dev = kmalloc(sizeof(struct chrdev_devices), GFP_KERNEL); 
	if(roger_dev == NULL){
		printk(KERN_INFO "kmalloc error.\n");
		goto out_unreg_chrdev;
	}
	//初始化cdev结构
	cdev_init(&roger_dev->my_cdev, &chrdev_fops);
	roger_dev->my_cdev.owner = THIS_MODULE;
	res = cdev_add(&roger_dev->my_cdev, MKDEV(MAJORS, MY_MINOR), MY_MINORS_ALLOMAX);
	if (res){
		printk(KERN_ERR "cdev_add error.\n");
		goto out_unreg_kfree;
	}
	
	//创建一个设备类----在/sys/class目录下可以找到一个my_chrdev_class文件夹
	chrdev_class = class_create(THIS_MODULE, "my_chrdev_class");
	if(IS_ERR(chrdev_class)){
		res = IS_ERR(chrdev_class);
		printk(KERN_ERR "class_create error.\n");
		goto out_unreg_cdev_add;
	}

	//创建设备----在/dev 目录下可以找到一个 chrdev_roger的文件
	//同时在 /dev/my_chrdev_class目录下可以找到chrdev_roger文件夹
	dev = device_create(chrdev_class, 0, MKDEV(MAJORS, MY_MINOR), NULL, "chrdev_roger");
	if(IS_ERR(dev)){
		res = PTR_ERR(dev);
		printk(KERN_ERR "device_create error.\n");
		goto out_unreg_class;
	}
	
	/*res = sysfs_create_file(&dev->kobj, &dev_attr_roger.attr);
	if (res < 0){
		printk(KERN_INFO "sysfs_create_file error.\n");
		goto out_unreg_device;
	}*/
	
	printk(KERN_INFO "chrdev init success.\n");
	return 0;
	
out_unreg_class:
	class_destroy(chrdev_class);
out_unreg_cdev_add:
	cdev_del(&(roger_dev->my_cdev));
out_unreg_kfree:
	kfree(roger_dev);
out_unreg_chrdev:
	unregister_chrdev_region(MKDEV(MAJORS, MY_MINOR), MY_MINORS_ALLOMAX);
out:	
	return res;
}


static int chrdev_remove(struct platform_device *pdev)
{
	device_destroy(chrdev_class, MKDEV(MAJORS, MY_MINOR));
	class_destroy(chrdev_class);
	cdev_del(&(roger_dev->my_cdev));
	kfree(roger_dev);
	unregister_chrdev_region(MKDEV(MAJORS, MY_MINOR), MY_MINORS_ALLOMAX);//卸载模块
	printk(KERN_INFO "chrdev_exit.\n");
	
	return 0;
}

const struct of_device_id roger_match_table[] = {
	{.compatible = "my_dev01_test",},
	{},
};

static struct platform_driver roger_driver = {
	.probe = chrdev_probe,
	.remove = chrdev_remove,

	.driver = {
		.name = "my_dev01_test",
		.owner = THIS_MODULE,
		.of_match_table = roger_match_table,
	},
};

static int __init roger_module_init(void)
{
	printk("Hello!moudule,you are here \n");
	return platform_driver_register(&roger_driver);
}
module_init(roger_module_init);

static void __exit roger_module_exit(void)
{	
	printk("Goodbye, you will go\n");
	platform_driver_unregister(&roger_driver);
}
module_exit(roger_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roger");
