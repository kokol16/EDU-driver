#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h> 
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>


MODULE_LICENSE("GPL");
static int major;

// DMA
#define DMA_BASE 	0x40000
#define DMA_REG_SRC	0x80
#define DMA_REG_DST	0x88
#define DMA_REG_LEN	0x90
#define DMA_REG_CMD	0x98

#define DMA_CMD_SEND	0x01
#define DMA_CMD_RECV	0x03
#define DMA_CMD_IRQ		0x04

// MMIO
static void __iomem *mmio_base;
static struct pci_dev *pci_device;





//PCI ID: 1234:11e8/* Registers. */
#define IO_IRQ_STATUS 0x24
#define IO_IRQ_ACK 0x64
#define IO_DMA_SRC 0x80
#define IO_DMA_DST 0x88
#define IO_DMA_CNT 0x90
#define IO_DMA_CMD 0x98


static struct pci_device_id pci_id_table[] = {
    { PCI_DEVICE(0x1234, 0x11e8) },
    {0,}
};

MODULE_DEVICE_TABLE(pci, pci_id_table);


char __user *usr_buf_g_ref;
volatile void *vaddr_g_ref;
volatile int len_g_ref;



static irqreturn_t
dma_irq_handler(int irq, void *device)
{
	int status_reg;

	if(*(int *)device != major)
		return IRQ_NONE;

	status_reg = ioread32(mmio_base + 0x24);
	
	/*if ( raw_copy_to_user(usr_buf_g_ref, (const void * )vaddr_g_ref, len_g_ref)){
		printk("error in copy to user interrupts\n");
	} // copy back to user buffer
	*/
	printk("contents: %s  \n",usr_buf_g_ref);

	pr_info("irq_handler irq = %d dev = %d irq_status = %llx\n",
				irq, device, (unsigned long long)status_reg);
	iowrite32(status_reg, mmio_base + 0x64);

	return IRQ_HANDLED;
}

static int  edu_reg_read(struct file *filp, const char __user *buf, size_t len, loff_t *off){
	u32 memory=0;
	memory = ioread32 (mmio_base + *off) ;
	if (raw_copy_to_user(buf,(const void *) &memory,len)){
		return 0;
	}else{
		(*off) += len;
		return 1;
	}
	
}

static void edu_dma_read(char __user *buf, size_t len, loff_t *off)
{
	dma_addr_t dma_handle;
	void *vaddr;


	vaddr = dma_alloc_coherent(&(pci_device->dev), len, &dma_handle, GFP_ATOMIC);
	

	iowrite32(*off			, mmio_base + DMA_REG_SRC); // offset to read from
	iowrite32(dma_handle	, mmio_base + DMA_REG_DST); // physical dma mapped addr
	iowrite32(len			, mmio_base + DMA_REG_LEN); // length
	iowrite32(DMA_CMD_RECV | DMA_CMD_IRQ, mmio_base + DMA_REG_CMD); // send cmd

	// Spin till done
	while(ioread32(mmio_base + 0x98) & 0x1) { /* Spin */}
	
	printk(KERN_INFO "[%s] DMA-Read: %s \n", __FUNCTION__, vaddr);


	vaddr_g_ref = vaddr;
	usr_buf_g_ref = buf;
	len_g_ref = len;
	

	raw_copy_to_user(buf, (const void * )vaddr, len); // copy back to user buffer


	// free shared memory
	dma_free_coherent(&(pci_device->dev), len, vaddr, dma_handle);

	return;
}


static ssize_t edu_read(struct file *filp, char __user *buf, size_t len, loff_t *off){
	unsigned int memory;
	printk(KERN_INFO "[%s] Read called \n", __FUNCTION__);

	if(*off < DMA_BASE){
		printk(KERN_INFO "[%s] length: [%x] offset: [%x] \n", __FUNCTION__,len, *off);
		if (edu_reg_read(filp, buf, len, off)!=0){
			return len;

		}

	}
	else{
		edu_dma_read(buf,len,off);
	}
	
	printk(KERN_INFO "[%s] 	 \n", __FUNCTION__);
	return 0;
}

static int 
edu_reg_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	u32 memory;
	
	if (*off % 4 != 0){
		return 0;
	}

	if (len == 0)return 0;
	
	if (*off < 0x80 && len != 4){
		return 0;
	} 
	if (len!=4  &&  len != 8){
		return 0;
	}

	if (raw_copy_from_user(&memory,buf,len) < 0){
		return 0;
	}
	else{
		printk(KERN_INFO "[%s] Write at %x + %x = %x\n", __FUNCTION__, mmio_base, *off, memory);
		iowrite32(memory, mmio_base+ *off);
		//(*off) += len;
		return len;
	}
}

static void edu_dma_write(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	dma_addr_t dma_handle;
	void *vaddr;

	vaddr = dma_alloc_coherent(&(pci_device->dev), len, &dma_handle, GFP_ATOMIC);

	if(raw_copy_from_user(vaddr, buf, len) < 0){
		printk(KERN_INFO "[%s] DMA Write Failed from %x \n", __FUNCTION__, *off);
		return;
	}

	iowrite32(dma_handle	, mmio_base + DMA_REG_SRC);
	iowrite32(*off			, mmio_base + DMA_REG_DST);
	iowrite32(len			, mmio_base + DMA_REG_LEN);
	iowrite32(DMA_CMD_SEND | DMA_CMD_IRQ, mmio_base + DMA_REG_CMD);

	// Spin till done
	while(ioread32(mmio_base + 0x98) & 0x1) { /* Spin */ }
	
	// free shared memory
	//dma_free_coherent(&(pci_device->dev), len, vaddr, dma_handle);

	return;
}

static ssize_t edu_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){

	/* range check */
	if(*off < DMA_BASE)
		edu_reg_write(filp, buf, len, off);
	else
		edu_dma_write(filp, buf, len, off);

	printk(KERN_INFO "[%s] write finished \n", __FUNCTION__);
	return 0;
}


static loff_t edu_llseek(struct file *filp, loff_t off, int whence){

	filp->f_pos = off;
	return off;
}

static struct file_operations file_ops = {
	.owner 	= THIS_MODULE,
	.llseek	= edu_llseek,
	.read	= edu_read,
	.write	= edu_write
};

/*
 * 

        Enable the device

        Request MMIO/IOP resources

        Set the DMA mask size (for both coherent and streaming DMA)

        Allocate and initialize shared control data (pci_allocate_coherent())

        Access device configuration space (if needed)

        Register IRQ handler (request_irq())

        Initialize non-PCI (i.e. LAN/SCSI/etc parts of the chip)

        Enable DMA/processing engines.

	https://www.kernel.org/doc/html/latest/PCI/pci.html	
 */

static int edu_probe(struct pci_dev *device, const struct pci_device_id *device_id){
	printk(KERN_INFO "[%s] Probe \n", __FUNCTION__);
	/*
	* Enable the device
	*/
	major = register_chrdev(0, "edu", &file_ops);
	if(pci_enable_device(device) < 0){
		printk(KERN_INFO "[%s] Device init failed \n", __FUNCTION__);
		return 1;
	}

	/*
	* 
	*/
	if (pci_request_region(device, 0, "region")){
		printk(KERN_INFO "[%s] address space occupied by other device \n", __FUNCTION__);
	}

	
	if(dma_set_mask_and_coherent(&(device->dev),  DMA_BIT_MASK(28))){
		dev_warn(&(device->dev), "mydev: No suitable DMA available\n");
	}

	mmio_base = pci_iomap(device, 0, pci_resource_len(device, 0));
	pci_set_master(device);
	pci_device = device;


	/* Register IRQ */
	if(request_irq(device->irq, dma_irq_handler, IRQF_SHARED, "pci_irq_handler0", &major) < 0){
		printk(KERN_INFO "[%s] Registering DMA handler failed\n", __FUNCTION__);
		return 0;
	}


	//iowrite32(0xffff,  mmio_base+ 0x60);
	//iowrite32(0xffff,  mmio_base+ 0x64);



	printk(KERN_INFO "[%s] Initialization done\n", __FUNCTION__);

	return 0;
}

static void edu_remove(struct pci_dev *device){


	free_irq(pci_device->irq, &major);
	pci_release_region(pci_device, 0);
	unregister_chrdev(major, "edu");

	return;
}



static struct pci_driver pci_driver = {
	.name = "edu",
	.id_table = pci_id_table,
	.probe = edu_probe,
	.remove = edu_remove
};


static int __init edu_init(void) {
	if(pci_register_driver(&pci_driver) < 0){
		printk(KERN_INFO "[%s] Init failed \n", __FUNCTION__);
		return 1;
	}

	printk(KERN_INFO "[%s] Init sucessful \n", __FUNCTION__);
	return 0;
}

static void __exit edu_exit(void) {
	pci_unregister_driver(&pci_driver);

}

module_init(edu_init);
module_exit(edu_exit);
