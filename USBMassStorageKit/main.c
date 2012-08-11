//
//  main.c
//  USBMassStorageKit
//
//  Copyright (c) 2012 The Little Beige Box, http://www.beige-box.com
//  All rights reserved.
//

#include "BaseIncludes.h"

int main(int argc, const char * argv[])
{
    uint32_t s1, s2;
    usbstorage_handle* dev = USB_Alloc(sizeof(usbstorage_handle));
    UsbDeviceOpen(4871,357);
    USBStorage_Open(dev, NULL);
    USBStorage_Reset(dev);
    USBStorage_Inquiry(dev, 0);
    USBStorage_ReadCapacity(dev, 0, &s1, &s2);
    
    USB_LOG("%x %x\n", s1, s2);
    // insert code here...
    printf("Hello, World!\n");
    return 0;
}

