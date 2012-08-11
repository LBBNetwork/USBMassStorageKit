//
//  UsbDeviceIo.c
//  USBMassStorageKit
//
//  Copyright (c) 2012 The Little Beige Box, http://www.beige-box.com
//  All rights reserved.
//

#include "BaseIncludes.h"

const int DeviceVersion = 320;
int gInterface, gAlt_Interface;
apple_usb_context_t context;
IOUSBDevRequest fUSBDeviceRequest;
int fMaxLogicalUnitNumber;
UInt8 bulkInNumber = 0, bulkOutNumber = 0;
UInt8 NumEndpoints, interruptNumber = 0;

// Unknown
void* UsbDeviceOpen(SInt16 Vid, SInt16 Pid) {
    io_iterator_t deviceIterator;
    io_service_t usbDevice;
    UInt8 numConfigurations;
    IOReturn rc;
    
    rc = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(kIOUSBDeviceClassName), &deviceIterator);
    if(rc) {
        return NULL;
    }
    
    if(!IOIteratorIsValid(deviceIterator))
        return NULL;
     
    USB_LOG("The iterator is valid.\n");
    
    while ((usbDevice = IOIteratorNext(deviceIterator))) {
        SInt32 score;
        io_cf_plugin_ref_t **plugInInterface;
        
        rc = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        if(rc)
            return NULL;
        
        USB_LOG("The interface plugin has been created successfully.\n");
            
        //IOObjectRelease(usbDevice);
        
        if(rc == kIOReturnSuccess && plugInInterface) {
            UInt16 idVendor, idProduct;
            usb_device_t **dev;
            rc = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID320), (LPVOID)&dev);
            (*plugInInterface)->Release(plugInInterface);
            
            if(!dev) {
                USB_LOG("Error in creating the device interface. (0x%08x)\n", rc);
                return NULL;
            }
                
            (*dev)->GetDeviceVendor(dev, &idVendor);
            (*dev)->GetDeviceProduct(dev, &idProduct);
            
            USB_LOG("The product information for this device is: 0x%04x:0x%04x.\n", idVendor, idProduct);
            
            if(idVendor == Vid) {
                if(idProduct == Pid) {
                     IOUSBConfigurationDescriptorPtr desc;
                     UInt8 intfClass, intfSubClass;
                     UInt8 protocol;
                   
                     USB_LOG("Opening device 0x%08x:0x%04x\n", idVendor, idProduct);
                     
                     rc = (*dev)->GetNumberOfConfigurations(dev, &numConfigurations);
                     if(rc)
                          return NULL;
                              
                     rc = (*dev)->GetDeviceClass(dev, &intfClass);
                     if(rc)
                          return NULL;
                        
                     rc = (*dev)->GetDeviceSubClass(dev, &intfSubClass);
                     if(rc)
                          return NULL;
                        
                     rc = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &desc);
                     if(rc)
                          return NULL;
                        
                     USB_LOG("Device class is %d, subclass %d\n", intfClass, intfSubClass);
                    
                     USB_LOG("Device configuration is already set to %d\n", desc->bConfigurationValue);

                     
                     context.device = dev;
                     context.interface = NULL;
                     context.plugin = NULL;
                     
                     UsbDeviceSetInterface(0, 0);
                     
                     rc = (*context.interface)->GetNumEndpoints(context.interface, &NumEndpoints);
                     if(rc)
                          return NULL;
                     
                     rc = (*context.interface)->GetInterfaceClass(context.interface, &intfClass);
                     if(rc)
                          return NULL;
                     
                     rc = (*context.interface)->GetInterfaceSubClass(context.interface, &intfSubClass);
                     if(rc)
                          return NULL;
                     
                     USB_LOG("Interface class is %d, subclass %d\n", intfClass, intfSubClass);
                     
                     if(intfClass == 8)
                          USB_LOG("This is definitely a mass storage device, 100%% certain.\n");
                     
                     rc = (*dev)->GetDeviceProtocol(dev, &protocol);
                     if(rc) return NULL;
                     
                     USB_LOG("Preferred device protocol is %d\n", protocol);
                     
                     
                     char tmpbuf[8];
                     
                     fUSBDeviceRequest.bmRequestType 	= 0;
                     fUSBDeviceRequest.bRequest 		= 9;
                     fUSBDeviceRequest.wValue			= 0x100;
                     fUSBDeviceRequest.wIndex			= 0;
                     
                     fUSBDeviceRequest.wLength			= sizeof(tmpbuf);
                     fUSBDeviceRequest.pData			= &tmpbuf;
                     
                     rc = (*context.interface)->ControlRequest(context.interface, 0, &fUSBDeviceRequest);
                     
                     if(protocol == kProtocolBulkOnly) {
                          fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
                          fUSBDeviceRequest.bRequest 			= 0xFE;
                          fUSBDeviceRequest.wValue			= 0;
                          fUSBDeviceRequest.wIndex			= 0;
                     
                          fUSBDeviceRequest.wLength			= 1;
                          fUSBDeviceRequest.pData				= &fMaxLogicalUnitNumber;
                     
                          rc = (*context.interface)->ControlRequest(context.interface, 0, &fUSBDeviceRequest);
                     } else {
                          fMaxLogicalUnitNumber = 0;
                     }
                     USB_LOG("Maximum LUN number is %d\n", fMaxLogicalUnitNumber);
                     
                     {
                          int endpoint;
                          
                          for(endpoint = 0; endpoint <= NumEndpoints; endpoint++) {
                               UInt8   transferType;
                               UInt16  maxPacketSize;
                               UInt8   interval;
                               UInt8   number;
                               UInt8   direction;
                               
                               rc = (*context.interface)->GetPipeProperties(context.interface, endpoint, &direction,
                                                                    &number, &transferType, &maxPacketSize, &interval);
                               
                               if (kIOReturnSuccess == rc) {
                                    if (kUSBBulk != transferType)
                                         continue;
                                    
                                    if (kUSBIn == direction)
                                         bulkInNumber = endpoint;
                                    
                                    if (kUSBOut == direction)
                                         bulkOutNumber = endpoint;
                               } else {
                                    USB_LOG("Failure to parse through endpoints: 0x%08x\n", rc);
                                    return NULL;
                               }
                          }
                          
                          USB_LOG("Bulk IN: %d, Bulk OUT: %d\n", bulkInNumber, bulkOutNumber);
                     }

                     

                     return &context;
                }
            }
        }
    }
    
    return NULL;
}

int UsbDeviceSetInterface(int interface, int alt_interface) {
    IOReturn err;
    SInt32 score;
    IOUSBFindInterfaceRequest req;
    io_iterator_t interfaceIterator;
    io_service_t usbInterface;
    
    req.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    USB_LOG("Attempting to set interface to 0x%02x:0x%02x\n", interface, alt_interface);
    
    err = (*context.device)->CreateInterfaceIterator(context.device, &req, &interfaceIterator);
    if(err)
        return -1;
    
    USB_LOG("Creating the interface iterator yielded 0x%08x.\n", err);
    
    while((usbInterface = IOIteratorNext(interfaceIterator))) {
        io_cf_plugin_ref_t **plugInInterface;
        usb_interface_t **intf;
        UInt8 num;
            
        err = IOCreatePlugInInterfaceForService(usbInterface,
                                                kIOUSBInterfaceUserClientTypeID,
                                                kIOCFPlugInInterfaceID,
                                                &plugInInterface, 
                                                &score);
        
        IOObjectRelease(usbInterface);  // done with it
        
        if(!plugInInterface)
            return -1;
        
        USB_LOG("IOCreatePlugInInterfaceForService yielded 0x%08x.\n", err);
        
        err = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID)&intf);
        if(err)
            return -1;
        
        USB_LOG("(*plugInInterface)->QueryInterface yielded 0x%08x.\n", err);
        
        (*plugInInterface)->Release(plugInInterface);
        
        USB_LOG("Released plugin object.\n");
        
        err = (*intf)->GetInterfaceNumber(intf, &num);
        if(err)
            return -1;
        
        USB_LOG("Current interface is 0x%02x.\n", num);
        
        if(intf && num == interface) {
            err = (*intf)->USBInterfaceOpen(intf);
            if(err) {
                 USB_LOG("Interface is %p, %x\n", intf, err);
                return -1;
            }
            
            USB_LOG("Opening the interface yielded 0x%08x.\n", err);
            
            context.interface = intf;
        }
    }
    
    USB_LOG("Attempting to set alternate interface.\n");
    
    // set it
    err = (*context.interface)->SetAlternateInterface(context.interface, alt_interface);
    if(err)
        return -1;
    
    USB_LOG("(*client->handle.interface)->SetAlternateInterface yielded 0x%08x.\n", err);
    
    gInterface = interface;
    gAlt_Interface = alt_interface;

    return 0;
}