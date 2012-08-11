//
//  BaseIncludes.h
//  USBMassStorageKit
//
//  Copyright (c) 2012 The Little Beige Box, http://www.beige-box.com
//  All rights reserved.
//

#ifndef USBMassStorageKit_BaseIncludes_h
#define USBMassStorageKit_BaseIncludes_h

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugin.h>
#include <IOKit/SCSI/SCSITask.h>
#include <mach/mach.h>

typedef IOUSBDeviceInterface320 usb_device_t;
typedef IOCFPlugInInterface io_cf_plugin_ref_t;
typedef IOUSBInterfaceInterface197 usb_interface_t;

// Enumerations for Mass Storage Class Subclass types
enum
{
    kUSBStorageRBCSubclass 				= 1,
    kUSBStorageSFF8020iSubclass 		= 2,
    kUSBStorageQIC157Subclass			= 3,
    kUSBStorageUFISubclass				= 4,
    kUSBStorageSFF8070iSubclass			= 5,
    kUSBStorageSCSITransparentSubclass	= 6
};

enum
{
    kUSBMaxBulkTransfer		= 0x200000	// Max transfer is 2MB
};

// The supported USB Mass Storage Class transport protocols.
enum
{
    kProtocolControlBulkInterrupt	= 0x00,
    kProtocolControlBulk			= 0x01,
    kProtocolBulkOnly				= 0x50
};

typedef struct __apple_usb_context {
    io_cf_plugin_ref_t** plugin;
    usb_device_t **device;
    usb_interface_t **interface;
} apple_usb_context_t;

void* UsbDeviceOpen(SInt16 Vid, SInt16 Pid);
int UsbDeviceSetInterface(int interface, int alt_interface);
IOReturn UsbAbortScsiCommandForCbiProtocol(void);

typedef struct usb_storage_cbw {
    	UInt32 dCBWSignature;
    	UInt32 dCBWTag;
    	UInt32 dCBWDataTransferLength;
    	UInt8  bCWDFlags;
    	UInt8  bCBWLun;
    	UInt8  bCBWCBLength;
    	UInt8  CBWCB[16];
} usb_storage_cbw_t;

#pragma mark -
#pragma mark CBI Protocol Strutures
// Structure for the global PB's

struct CBIRequestBlock
{
	IOUSBDevRequest				cbiDevRequest;
	SCSICommandDescriptorBlock	cbiCDB;
	IOUSBCompletion				cbiCompletion;
	UInt32						currentState;
	UInt8						cbiGetStatusBuffer[2];	// 2 bytes as specified in the USB spec
};

typedef struct CBIRequestBlock	CBIRequestBlock;

#define USB_LOG     printf("[USB-IOKIT:%s():%d] ", __FUNCTION__, __LINE__), printf


typedef uint32_t u32;
typedef SInt32 s32;
typedef SInt16 s16;
typedef SInt8 s8;
typedef uint16_t u16;
typedef uint8_t u8;


#define USB_OK							0
#define USB_FAILED						1

/* Descriptor types */
#define USB_DT_DEVICE					0x01
#define USB_DT_CONFIG					0x02
#define USB_DT_STRING					0x03
#define USB_DT_INTERFACE				0x04
#define USB_DT_ENDPOINT					0x05

/* Standard requests */
#define USB_REQ_GETSTATUS				0x00
#define USB_REQ_CLEARFEATURE			0x01
#define USB_REQ_SETFEATURE				0x03
#define USB_REQ_SETADDRESS				0x05
#define USB_REQ_GETDESCRIPTOR			0x06
#define USB_REQ_SETDESCRIPTOR			0x07
#define USB_REQ_GETCONFIG				0x08
#define USB_REQ_SETCONFIG				0x09
#define USB_REQ_GETINTERFACE			0x0a
#define USB_REQ_SETINTERFACE			0x0b
#define USB_REQ_SYNCFRAME				0x0c

/* Descriptor sizes per descriptor type */
#define USB_DT_DEVICE_SIZE				18
#define USB_DT_CONFIG_SIZE				9
#define USB_DT_INTERFACE_SIZE			9
#define USB_DT_ENDPOINT_SIZE			7
#define USB_DT_ENDPOINT_AUDIO_SIZE		9	/* Audio extension */
#define USB_DT_HUB_NONVAR_SIZE			7

/* control message request type bitmask */
#define USB_CTRLTYPE_DIR_HOST2DEVICE	(0<<7)
#define USB_CTRLTYPE_DIR_DEVICE2HOST	(1<<7)
#define USB_CTRLTYPE_TYPE_STANDARD		(0<<5)
#define USB_CTRLTYPE_TYPE_CLASS			(1<<5)
#define USB_CTRLTYPE_TYPE_VENDOR		(2<<5)
#define USB_CTRLTYPE_TYPE_RESERVED		(3<<5)
#define USB_CTRLTYPE_REC_DEVICE			0
#define USB_CTRLTYPE_REC_INTERFACE		1
#define USB_CTRLTYPE_REC_ENDPOINT		2
#define USB_CTRLTYPE_REC_OTHER			3

#define USB_FEATURE_ENDPOINT_HALT		0

#define USB_ENDPOINT_IN					0x80
#define USB_ENDPOINT_OUT				0x00


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
# define ATTRIBUTE_PACKED				__attribute__((packed))
    
    typedef struct _usbctrlrequest {
        u8 bRequestType;
        u8 bRequest;
        u16 wValue;
        u16 wIndex;
        u16 wLength;
    } ATTRIBUTE_PACKED usbctrlrequest;
    
    typedef struct _usbendpointdesc
    {
        u8 bLength;
        u8 bDescriptorType;
        u8 bEndpointAddress;
        u8 bmAttributes;
        u16 wMaxPacketSize;
        u8 bInterval;
    } ATTRIBUTE_PACKED usb_endpointdesc;
    
    typedef struct _usbinterfacedesc
    {
        u8 bLength;
        u8 bDescriptorType;
        u8 bInterfaceNumber;
        u8 bAlternateSetting;
        u8 bNumEndpoints;
        u8 bInterfaceClass;
        u8 bInterfaceSubClass;
        u8 bInterfaceProtocol;
        u8 iInterface;
        struct _usbendpointdesc *endpoints;
    } ATTRIBUTE_PACKED usb_interfacedesc;
    
    typedef struct _usbconfdesc
    {
        u8 bLength;
        u8 bDescriptorType;
        u16 wTotalLength;
        u8 bNumInterfaces;
        u8 bConfigurationValue;
        u8 iConfiguration;
        u8 bmAttributes;
        u8 bMaxPower;
        struct _usbinterfacedesc *interfaces;
    } ATTRIBUTE_PACKED usb_configurationdesc;
    
    typedef struct _usbdevdesc
    {
        u8  bLength;
        u8  bDescriptorType;
        u16 bcdUSB;
        u8  bDeviceClass;
        u8  bDeviceSubClass;
        u8  bDeviceProtocol;
        u8  bMaxPacketSize0;
        u16 idVendor;
        u16 idProduct;
        u16 bcdDevice;
        u8  iManufacturer;
        u8  iProduct;
        u8  iSerialNumber;
        u8  bNumConfigurations;
        struct _usbconfdesc *configurations;
    } ATTRIBUTE_PACKED usb_devdesc;
    
    struct ehci_device;
    
    s32 USB_OpenDevice(const char *device,u16 vid,u16 pid,struct ehci_device **fd);
    s32 USB_CloseDevice(struct ehci_device **fd);
    
    s32 USB_GetDescriptors(struct ehci_device *fd, usb_devdesc *udd);
    void USB_FreeDescriptors(usb_devdesc *udd);
    
    s32 USB_GetDeviceDescription(struct ehci_device *fd,usb_devdesc *devdesc);
    
    void USB_SuspendDevice(struct ehci_device *fd);
    void USB_ResumeDevice(struct ehci_device *fd);
    
    s32 USB_ReadIntrMsg(struct ehci_device *fd,u8 bEndpoint,u16 wLength,void *rpData);
    
    s32 USB_ReadBlkMsg(struct ehci_device *fd,u8 bEndpoint,u16 wLength,void *rpData);
    
    s32 USB_ReadCtrlMsg(struct ehci_device *fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
    
    s32 USB_WriteIntrMsg(struct ehci_device *fd,u8 bEndpoint,u16 wLength,void *rpData);
    
    s32 USB_WriteBlkMsg(struct ehci_device *fd,u8 bEndpoint,u16 wLength,void *rpData);
    
    s32 USB_WriteCtrlMsg(struct ehci_device *fd,u8 bmRequestType,u8 bmRequest,u16 wValue,u16 wIndex,u16 wLength,void *rpData);
    
    s32 USB_GetConfiguration(struct ehci_device *fd, u8 *configuration);
    s32 USB_SetConfiguration(struct ehci_device *fd, u8 configuration);
    s32 USB_SetAlternativeInterface(struct ehci_device *fd, u8 interface, u8 alternateSetting);
    s32 USB_ClearHalt(struct ehci_device *fd, u8 endpointAddress);
    s32 USB_GetDeviceList(const char *devpath,void *descr_buffer,u8 num_descr,u8 b0,u8 *cnt_descr);
    
    /* alloc memory from the USB subsystem */
    void * USB_Alloc(int size);
    void USB_Free(void *ptr);
    
    
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#define	USBSTORAGE_OK			0
#define	USBSTORAGE_ENOINTERFACE		-10000
#define	USBSTORAGE_ESENSE		-10001
#define	USBSTORAGE_ESHORTWRITE		-10002
#define	USBSTORAGE_ESHORTREAD		-10003
#define	USBSTORAGE_ESIGNATURE		-10004
#define	USBSTORAGE_ETAG			-10005
#define	USBSTORAGE_ESTATUS		-10006
#define	USBSTORAGE_EDATARESIDUE		-10007
#define	USBSTORAGE_ETIMEDOUT		-ETIMEDOUT
#define	USBSTORAGE_EINIT		-10009

typedef struct
{
	u8 configuration;
	u32 interface;
	u32 altInterface;
    
	u8 ep_in;
	u8 ep_out;
    
	u8 max_lun;
	u32 sector_size[16];
	u32 n_sector[16];
    
	struct ehci_device * usb_fd;
    
	//mutex_t lock;
	//cond_t cond;
	s32 retval;
    
	u32 tag;
	u8 suspended;
    
	u8 *buffer;
} usbstorage_handle;

s32 USBStorage_Initialize(void);

s32 USBStorage_Open(usbstorage_handle *dev, struct ehci_device *fd);
s32 USBStorage_Close(usbstorage_handle *dev);
s32 USBStorage_Reset(usbstorage_handle *dev);

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev);
s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun);
s32 USBStorage_Suspend(usbstorage_handle *dev);

s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors);
s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer);
s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer);
s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun);

#define DEVICE_TYPE_WII_USB (('W'<<24)|('U'<<16)|('S'<<8)|'B')

s32 USBStorage_Try_Device(struct ehci_device *fd);



#endif
