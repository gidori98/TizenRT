/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <tinyara/config.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <tinyara/gidori.h>
#include <tinyara/fs/fs.h>
#include <tinyara/fs/ioctl.h>

#include <stdio.h>
#include <string.h>

// copy_to_user

/******************************************************************************
 * Private Variables
 *****************************************************************************/
static int gidori_open(FAR struct file *filep);
static int gidori_close(FAR struct file *filep);
static int gidori_read(FAR struct file *filep);
static int gidori_write(FAR struct file *filep);
static int gidori_ioctrl(FAR struct file *filep, int cmd, unsigned long arg);

static const struct file_operations g_gidoriops = {
    gidori_open,				/* open */
    gidori_close,				/* close */
    gidori_read,	        	/* read */
    gidori_write,   		    /* write */
    NULL,						/* seek */
    gidori_ioctrl,				/* ioctl */
#ifndef CONFIG_DISABLE_POLL
    NULL						/* poll */
#endif
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gidoridrvr_open
 ****************************************************************************/

static int gidori_open(FAR struct file *filep)
{
    return OK;
}

/****************************************************************************
 * Name: gidoridrvr_close
 ****************************************************************************/

static int gidori_close(FAR struct file *filep)
{
    return OK;
}

/****************************************************************************
 * Name: gidoridrvr_read
 ****************************************************************************/

static int gidori_read(FAR struct file *filep)
{
    return OK;
}

/****************************************************************************
 * Name: gidoridrvr_write
 ****************************************************************************/

static int gidori_write(FAR struct file *filep)
{
    return OK;
}


/****************************************************************************
 * Name: gidoridrvr_ioctrl
 ****************************************************************************/

static int gidori_ioctrl(FAR struct file *filep, int cmd, unsigned long arg) 
{
    FAR struct inode *inode = filep->f_inode;
	FAR struct gidori_dev_buf *dev = inode->i_private;

    int ret = -ENOSYS;

    switch(cmd){
    case GIDORI_WRITE:
        if (strlen((char*)arg) > BUFFER_SIZE){
            ret = ENOBUFS;
            break;
        }
        memcpy(dev->buffer, (char*)arg, strlen((char*)arg)+1);
        ret = OK;
        break;
    case GIDORI_READ:
        memcpy((char*)arg, dev->buffer, strlen(dev->buffer)+1);
        ret = OK;
        break;
    default:
        ret = EINVAL;
        break;
    }
}

/****************************************************************************
 * Name: gidori_register
 * Description:
 *   register device.
 *
 ****************************************************************************/

void gidori_register(FAR struct gidori_dev_buf *dev)
{
    const char *path = "/dev/gidori";
    
    (void)register_driver(path, &g_gidoriops, 0666, dev);
}
