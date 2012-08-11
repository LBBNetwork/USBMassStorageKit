//
//  UsbDeviceCbiLayer.c
//  USBMassStorageKit
//
//  Copyright (c) 2012 The Little Beige Box, http://www.beige-box.com
//  All rights reserved.
//  From BootMii because writing a USB stack is horrible.
//
/*-------------------------------------------------------------
 
 usbstorage.c -- Bulk-only USB mass storage support
 
 Copyright (C) 2008
 Sven Peter (svpe) <svpe@gmx.net>
 
 quick port to ehci/ios: Kwiirk
 
 This software is provided 'as-is', without any express or implied
 warranty.  In no event will the authors be held liable for any
 damages arising from the use of this software.
 
 Permission is granted to anyone to use this software for any
 purpose, including commercial applications, and to alter it and
 redistribute it freely, subject to the following restrictions:
 
 1.	The origin of this software must not be misrepresented; you
 must not claim that you wrote the original software. If you use
 this software in a product, an acknowledgment in the product
 documentation would be appreciated but is not required.
 
 2.	Altered source versions must be plainly marked as such, and
 must not be misrepresented as being the original software.
 
 3.	This notice may not be removed or altered from any source
 distribution.
 
 -------------------------------------------------------------*/

#include "BaseIncludes.h"

extern apple_usb_context_t context;
extern IOUSBDevRequest fUSBDeviceRequest;
extern UInt8 bulkInNumber, bulkOutNumber;


#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

#define	HEAP_SIZE			(32*1024)
#define	TAG_START			0x1//BADC0DE

#define	CBW_SIZE			31
#define	CBW_SIGNATURE			0x43425355
#define	CBW_IN				(1 << 7)
#define	CBW_OUT				0

#define	CSW_SIZE			13
#define	CSW_SIGNATURE			0x53425355

#define	SCSI_TEST_UNIT_READY		0x00
#define	SCSI_INQUIRY			0x12
#define	SCSI_REQUEST_SENSE		0x03
#define	SCSI_READ_CAPACITY		0x25
#define	SCSI_READ_10			0x28
#define	SCSI_WRITE_10			0x2A

#define	SCSI_SENSE_REPLY_SIZE		18
#define	SCSI_SENSE_NOT_READY		0x02
#define	SCSI_SENSE_MEDIUM_ERROR		0x03
#define	SCSI_SENSE_HARDWARE_ERROR	0x04

#define	USB_CLASS_MASS_STORAGE		0x08
#define	MASS_STORAGE_SCSI_COMMANDS	0x06
#define	MASS_STORAGE_BULK_ONLY		0x50

#define USBSTORAGE_GET_MAX_LUN		0xFE
#define USBSTORAGE_RESET		0xFF

#define	USB_ENDPOINT_BULK		0x02

#define USBSTORAGE_CYCLE_RETRIES	3

#define MAX_TRANSFER_SIZE			4096

#define DEVLIST_MAXSIZE    8

#define swab32(x) ((u32)(                                     \
(((u32)(x) & (u32)0x000000ffUL) << 24) | \
(((u32)(x) & (u32)0x0000ff00UL) <<  8) | \
(((u32)(x) & (u32)0x00ff0000UL) >>  8) | \
(((u32)(x) & (u32)0xff000000UL) >> 24)))
#define swab16(x) ((u16)(                                   \
(((u16)(x) & (u16)0x00ffU) << 8) |          \
(((u16)(x) & (u16)0xff00U) >> 8)))

#define cpu_to_le32(a) (a)
#define le32_to_cpu(a) (a)
#define cpu_to_le16(a) (a)
#define le16_to_cpu(a) (a)
#define cpu_to_be32(a) swab32(a)
#define be32_to_cpu(a) swab32(a)

void * USB_Alloc(int size) {
    void *p = malloc(size);
    if(!p)
        return NULL;
    
    memset(p,0,size);
    return p;
}

void USB_Free(void *ptr) {
    free(ptr);
}

static s32 __usbstorage_reset(usbstorage_handle *dev);
static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun);

// ehci driver has its own timeout.
static s32 __USB_BlkMsgTimeout(usbstorage_handle *dev, u8 bEndpoint, u16 wLength, void *rpData)
{
    IOReturn    err;
    UInt32 s;
    
    if(bEndpoint == dev->ep_in) {
        err = (*context.interface)->WritePipe(context.interface, bEndpoint, rpData, wLength);
        USB_LOG("Sending bulk transfer, %d bytes sent, (0x%08x)\n", wLength, err);
    } else if(bEndpoint == dev->ep_out) {
        err = (*context.interface)->ReadPipe(context.interface, bEndpoint, rpData, &s);
        USB_LOG("Receiving bulk transfer, %d bytes received, (0x%08x)\n", s, err);
    }
    
    return wLength;
}

static s32 __USB_CtrlMsgTimeout(usbstorage_handle *dev, u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, u16 wLength, void *rpData)
{
    IOUSBDevRequest ctrl;
    IOReturn    err;
    
    ctrl.bmRequestType = bmRequestType;
    ctrl.bRequest = bmRequest;
    ctrl.wValue = wValue;
    ctrl.wIndex = wIndex;
    ctrl.wLength = wLength;
    ctrl.pData = rpData;
    
    err = (*context.interface)->ControlRequest(context.interface, 0, &ctrl);
    USB_LOG("Sending control transfer, %d bytes sent (%d attempted), (0x%08x)\n", ctrl.wLenDone, wLength, err);

    return ctrl.wLenDone;
}

static s32 __send_cbw(usbstorage_handle *dev, u8 lun, u32 len, u8 flags, const u8 *cb, u8 cbLen)
{
	s32 retval = USBSTORAGE_OK;
    
	if(cbLen == 0 || cbLen > 16)
		return -EINVAL;
	memset(dev->buffer, 0, CBW_SIZE);
    
	((u32*)dev->buffer)[0]=cpu_to_le32(CBW_SIGNATURE);
	((u32*)dev->buffer)[1]=cpu_to_le32(dev->tag);
	((u32*)dev->buffer)[2]=cpu_to_le32(len);
	dev->buffer[12] = flags;
	dev->buffer[13] = lun;
	dev->buffer[14] = (cbLen > 6 ? 0x10 : 6);
    
	memcpy(dev->buffer + 15, cb, cbLen);
    
	retval = __USB_BlkMsgTimeout(dev, dev->ep_out, CBW_SIZE, (void *)dev->buffer);
    
	if(retval == CBW_SIZE) return USBSTORAGE_OK;
	else if(retval >= 0) return USBSTORAGE_ESHORTWRITE;
    
	return retval;
}

static s32 __read_csw(usbstorage_handle *dev, u8 *status, u32 *dataResidue)
{
	s32 retval = USBSTORAGE_OK;
	u32 signature, tag, _dataResidue, _status;
    
	memset(dev->buffer, 0xff, CSW_SIZE);
    
	retval = __USB_BlkMsgTimeout(dev, dev->ep_in, CSW_SIZE, dev->buffer);
    USB_LOG("Bulk timeout 0x%08x\n", retval);
    
    //print_hex_dump_bytes("csv resp:",DUMP_PREFIX_OFFSET,dev->buffer,CSW_SIZE);
    
	if(retval >= 0 && retval != CSW_SIZE) return USBSTORAGE_ESHORTREAD;
	else if(retval < 0) return retval;
    
	signature = le32_to_cpu(((u32*)dev->buffer)[0]);
	tag = le32_to_cpu(((u32*)dev->buffer)[1]);
	_dataResidue = le32_to_cpu(((u32*)dev->buffer)[2]);
	_status = dev->buffer[12];
    
	if(signature != CSW_SIGNATURE) {
        return USBSTORAGE_ESIGNATURE;
    }
    
	if(dataResidue != NULL)
		*dataResidue = _dataResidue;
	if(status != NULL)
		*status = _status;
    
	if(tag != dev->tag) return USBSTORAGE_ETAG;
	dev->tag++;
    
	return USBSTORAGE_OK;
}
static s32 __cycle(usbstorage_handle *dev, u8 lun, u8 *buffer, u32 len, u8 *cb, u8 cbLen, u8 write, u8 *_status, u32 *_dataResidue)
{
	s32 retval = USBSTORAGE_OK;
    
	u8 status = 0;
	u32 dataResidue = 0;
	u32 thisLen;
    
	s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;
    
	do
	{
		retries--;
		if(retval == USBSTORAGE_ETIMEDOUT)
			break;
        
		if(write)
		{
			retval = __send_cbw(dev, lun, len, CBW_OUT, cb, cbLen);
			if(retval == USBSTORAGE_ETIMEDOUT)
				break;
			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
			while(len > 0)
			{
                thisLen=len;
				retval = __USB_BlkMsgTimeout(dev, dev->ep_out, thisLen, buffer);
                
				if(retval == USBSTORAGE_ETIMEDOUT)
					break;
                
				if(retval < 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					break;
				}
                
                
				if(retval != thisLen && len > 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					break;
				}
				len -= retval;
				buffer += retval;
			}
            
			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
		}
		else
		{
			retval = __send_cbw(dev, lun, len, CBW_IN, cb, cbLen);
            
			if(retval == USBSTORAGE_ETIMEDOUT)
				break;
            
			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
			while(len > 0)
			{
                thisLen=len;
				retval = __USB_BlkMsgTimeout(dev, dev->ep_in, thisLen, buffer);
                //print_hex_dump_bytes("usbs in:",DUMP_PREFIX_OFFSET,dev->buffer,36);
				if(retval < 0)
					break;
                
				len -= retval;
				buffer += retval;
                
				if(retval != thisLen)
					break;
			}
            
			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
		}
        
		retval = __read_csw(dev, &status, &dataResidue);
        
		if(retval == USBSTORAGE_ETIMEDOUT)
			break;
        
		if(retval < 0)
		{
			if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
				retval = USBSTORAGE_ETIMEDOUT;
			continue;
		}
        
		retval = USBSTORAGE_OK;
	} while(retval < 0 && retries > 0);
    
	if(retval < 0 && retval != USBSTORAGE_ETIMEDOUT)
	{
		if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
			retval = USBSTORAGE_ETIMEDOUT;
	}
    
    
	if(_status != NULL)
		*_status = status;
	if(_dataResidue != NULL)
		*_dataResidue = dataResidue;
    
	return retval;
}

static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[16];
	u8 *sense= USB_Alloc(SCSI_SENSE_REPLY_SIZE);
	u8 status = 0;
	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_TEST_UNIT_READY;
    
	retval = __cycle(dev, lun, NULL, 0, cmd, 1, 1, &status, NULL);
	if(retval < 0) return retval;
    
	if(status != 0)
	{
		cmd[0] = SCSI_REQUEST_SENSE;
		cmd[1] = lun << 5;
		cmd[4] = SCSI_SENSE_REPLY_SIZE;
		cmd[5] = 0;
		memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
		retval = __cycle(dev, lun, sense, SCSI_SENSE_REPLY_SIZE, cmd, 6, 0, NULL, NULL);
		if(retval < 0) goto error;
        
		status = sense[2] & 0x0F;
		if(status == SCSI_SENSE_NOT_READY || status == SCSI_SENSE_MEDIUM_ERROR || status == SCSI_SENSE_HARDWARE_ERROR)
            retval = USBSTORAGE_ESENSE;
	}
error:
    USB_Free(sense);
	return retval;
}

static s32 __usbstorage_reset(usbstorage_handle *dev)
{
	s32 retval;
    
    /*
     retval = ehci_reset_device(dev->usb_fd);
     if(retval < 0 && retval != -7004)
     goto end;
     */
    //debug_printf("usbstorage reset..\n");
    USB_LOG("Resetting device.\n");
	retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_RESET, 0, dev->interface, 0, NULL);
    
	/* FIXME?: some devices return -7004 here which definitely violates the usb ms protocol but they still seem to be working... */
	if(retval < 0 && retval != -7004)
		goto end;
    
	/* gives device enough time to process the reset */
	usleep(10);
    
    USB_LOG("Clearing halts on both endpoints\n");
    //debug_printf("cleat halt on bulk ep..\n");
    
    retval = (*context.interface)->ClearPipeStallBothEnds(context.interface, dev->ep_in);
    if(retval < 0)
		goto end;
	retval = (*context.interface)->ClearPipeStallBothEnds(context.interface, dev->ep_out);
    
end:
	return retval;
}

extern UInt8 bulkInNumber, bulkOutNumber;
extern int fMaxLogicalUnitNumber;
extern int gInterface, gAlt_Interface;

s32 USBStorage_Open(usbstorage_handle *dev, struct ehci_device *fd)
{
    dev->ep_in = bulkInNumber;
    dev->ep_out = bulkOutNumber;
    dev->max_lun = fMaxLogicalUnitNumber;
    dev->altInterface = gAlt_Interface;
    dev->interface = gInterface;
    dev->buffer = USB_Alloc(512);
	return 0;
}

s32 USBStorage_Close(usbstorage_handle *dev)
{
    if(dev->buffer != NULL)
        USB_Free(dev->buffer);
	memset(dev, 0, sizeof(*dev));
	return 0;
}

s32 USBStorage_Reset(usbstorage_handle *dev)
{
	s32 retval;
    
	retval = __usbstorage_reset(dev);
    
	return retval;
}

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev)
{
    
	return dev->max_lun;
}

s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
    
	if(lun >= dev->max_lun)
		return -EINVAL;
    
	retval = __usbstorage_clearerrors(dev, lun);
	if(retval < 0)
		return retval;
    
	retval = USBStorage_Inquiry(dev, lun);
    
	retval = USBStorage_ReadCapacity(dev, lun, &dev->sector_size[lun], &dev->n_sector[lun]);
	return retval;
}

s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[] = {SCSI_INQUIRY, lun << 5,0,0,36,0};
	u8 *response = USB_Alloc(36);
    USB_LOG("Inquiring about SCSI drive\n");
	retval = __cycle(dev, lun, response, 36, cmd, 6, 0, NULL, NULL);
    //print_hex_dump_bytes("inquiry result:",DUMP_PREFIX_OFFSET,response,36);
    USB_Free(response);
	return retval;
}
s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors)
{
	s32 retval;
	u8 cmd[] = {SCSI_READ_CAPACITY, lun << 5};
	u8 *response = USB_Alloc(8);
    u32 val;
	retval = __cycle(dev, lun, response, 8, cmd, 2, 0, NULL, NULL);
	if(retval >= 0)
	{
        
        memcpy(&val, response, 4);
		if(n_sectors != NULL)
            *n_sectors = be32_to_cpu(val);
        memcpy(&val, response + 4, 4);
		if(sector_size != NULL)
			*sector_size = be32_to_cpu(val);
		retval = USBSTORAGE_OK;
	}
    USB_Free(response);
	return retval;
}

s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_READ_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >>  8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
    };
	if(lun >= dev->max_lun || dev->sector_size[lun] == 0)
		return -EINVAL;
	retval = __cycle(dev, lun, buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 0, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_WRITE_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >> 8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
    };
	if(lun >= dev->max_lun || dev->sector_size[lun] == 0)
		return -EINVAL;
	retval = __cycle(dev, lun, (u8 *)buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 1, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

s32 USBStorage_Suspend(usbstorage_handle *dev)
{
	return USBSTORAGE_OK;
}
/*
 The following is for implementing the ioctl interface inpired by the disc_io.h
 as used by libfat
 
 This opens the first lun of the first usbstorage device found.
 */

static usbstorage_handle __usbfd;
static u8 __lun = 0;
static u8 __mounted = 0;
static u16 __vid = 0;
static u16 __pid = 0;

/* perform 512 time the same read */
s32 USBStorage_Read_Stress(u32 sector, u32 numSectors, void *buffer)
{
    s32 retval;
    int i;
    if(__mounted != 1)
        return false;
    
    for(i=0;i<512;i++){
        retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);
        sector+=numSectors;
        if(retval == USBSTORAGE_ETIMEDOUT)
        {
            __mounted = 0;
            USBStorage_Close(&__usbfd);
        }
        if(retval < 0)
            return false;
    }
    return true;
    
}
// temp function before libfat is available */
s32 USBStorage_Try_Device(struct ehci_device *fd)
{
    int maxLun,j,retval;
    if(USBStorage_Open(&__usbfd, fd) < 0)
        return -EINVAL;
    
    maxLun = USBStorage_GetMaxLUN(&__usbfd);
    if(maxLun == USBSTORAGE_ETIMEDOUT)
        return -EINVAL;
    for(j = 0; j < maxLun; j++)
    {
        retval = USBStorage_MountLUN(&__usbfd, j);
        if(retval == USBSTORAGE_ETIMEDOUT)
        {
            USBStorage_Reset(&__usbfd);
            USBStorage_Close(&__usbfd);
            break;
        }
        
        if(retval < 0)
            continue;
        
        (*context.device)->GetDeviceVendor(context.device, &__vid);
        (*context.device)->GetDeviceProduct(context.device, &__pid);
        __mounted = 1;
        __lun = j;
        return 0;
    }
    return -EINVAL;
}

s32 USBStorage_Get_Capacity(u32*sector_size)
{
    if(__mounted == 1)
    {
        if(sector_size){
            *sector_size = __usbfd.sector_size[__lun];
        }
        return __usbfd.n_sector[__lun];
    }
    return 0;
}

s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer)
{
    s32 retval;
    
    if(__mounted != 1)
        return false;
    
    retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);
    if(retval == USBSTORAGE_ETIMEDOUT)
    {
        __mounted = 0;
        USBStorage_Close(&__usbfd);
    }
    if(retval < 0)
        return false;
    return true;
}

s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer)
{
    s32 retval;
    
    if(__mounted != 1)
        return false;
    
    retval = USBStorage_Write(&__usbfd, __lun, sector, numSectors, buffer);
    if(retval == USBSTORAGE_ETIMEDOUT)
    {        __mounted = 0;
        USBStorage_Close(&__usbfd);
    }
    if(retval < 0)
        return false;
    return true;
}

