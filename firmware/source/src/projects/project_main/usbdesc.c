/*----------------------------------------------------------------------------
 *      U S B  -  K e r n e l
 *----------------------------------------------------------------------------
 * Name:    usbdesc.c
 * Purpose: USB Descriptors
 * Version: V1.20
 *----------------------------------------------------------------------------
 *      This software is supplied "AS IS" without any warranties, express,
 *      implied or statutory, including but not limited to the implied
 *      warranties of fitness for purpose, satisfactory quality and
 *      noninfringement. Keil extends you a royalty-free right to reproduce
 *      and distribute executable files created using this software for use
 *      on NXP Semiconductors LPC microcontroller devices only. Nothing else
 *      gives you the right to use this software.
 *
 * Copyright (c) 2009 Keil - An ARM Company. All rights reserved.
 *----------------------------------------------------------------------------
 * History:
 *          V1.20 Changed string descriptor handling
 *          V1.00 Initial Version
 *---------------------------------------------------------------------------*/
#include "usb.h"
#include "cdc.h"
#include "msc.h"
#include "hid.h"
#include "usbcfg.h"
#include "usbdesc.h"



/* HID Report Descriptor */
const uint8_t HID_ReportDescriptor[] = {
  HID_UsagePageVendor(0x00),
  HID_Usage(0x01),
  HID_Collection(HID_Application),
	HID_UsageMin(1),
	HID_UsageMax(0x40),
	HID_LogicalMin(0),
	HID_LogicalMax(0x40),
	HID_ReportCount(0x40),
	HID_ReportSize(0x08),
	HID_Input(HID_Data),
	HID_UsageMin(0),
	HID_UsageMax(0x40),
	HID_Output(HID_Data),
  HID_EndCollection,
};


const uint16_t HID_ReportDescSize = sizeof(HID_ReportDescriptor);

/* USB Standard Device Descriptor */
const uint8_t USB_DeviceDescriptor[] = {
  USB_DEVICE_DESC_SIZE,              /* bLength */
  USB_DEVICE_DESCRIPTOR_TYPE,        /* bDescriptorType */
  WBVAL(0x0200), /* 2.20 */          /* bcdUSB */
  0x00,                              /* bDeviceClass */
  0x00,                              /* bDeviceSubClass */
  0x00,                              /* bDeviceProtocol */
  USB_MAX_PACKET0,                   /* bMaxPacketSize0 */
  WBVAL(0x0308),                     /* idVendor */
  WBVAL(0x2803),                     /* idProduct */
  WBVAL(0x0200), /* 2.00 */          /* bcdDevice */
  0x04,                              /* iManufacturer */
  0x20,                              /* iProduct */
  0x44,                              /* iSerialNumber */
  0x01                               /* bNumConfigurations */
};

/* USB Configuration Descriptor */
/*   All Descriptors (Configuration, Interface, Endpoint, Class, Vendor */
const uint8_t USB_ConfigDescriptor[] = {
/* Configuration 1 */
  USB_CONFIGUARTION_DESC_SIZE,       /* bLength */
  USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
  WBVAL(                             /* wTotalLength */
    USB_CONFIGUARTION_DESC_SIZE +
    USB_INTERFACE_DESC_SIZE     +
    HID_DESC_SIZE               +
    USB_ENDPOINT_DESC_SIZE			+
		USB_ENDPOINT_DESC_SIZE
  ),
  0x01,                              /* bNumInterfaces */
  0x01,                              /* bConfigurationValue */
  0x00,                              /* iConfiguration */
  USB_CONFIG_POWERED_MASK /*|*/       /* bmAttributes */
/*USB_CONFIG_REMOTE_WAKEUP*/,
  USB_CONFIG_POWER_MA(500),          /* bMaxPower */
/* Interface 0, Alternate Setting 0, HID Class */
  USB_INTERFACE_DESC_SIZE,           /* bLength */
  USB_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
  0x00,                              /* bInterfaceNumber */
  0x00,                              /* bAlternateSetting */
  0x02,                              /* bNumEndpoints */
  USB_DEVICE_CLASS_HUMAN_INTERFACE,  /* bInterfaceClass */
  HID_SUBCLASS_NONE,                 /* bInterfaceSubClass */
  HID_PROTOCOL_NONE,                 /* bInterfaceProtocol */
  0,                              /* iInterface */
/* HID Class Descriptor */
/* HID_DESC_OFFSET = 0x0012 */
  HID_DESC_SIZE,                     /* bLength */
  HID_HID_DESCRIPTOR_TYPE,           /* bDescriptorType */
  WBVAL(0x0111), /* 1.00 */          /* bcdHID */
  0x00,                              /* bCountryCode */
  0x01,                              /* bNumDescriptors */
  HID_REPORT_DESCRIPTOR_TYPE,        /* bDescriptorType */
  WBVAL(HID_REPORT_DESC_SIZE),       /* wDescriptorLength */
/* Endpoint, HID Interrupt In */
  USB_ENDPOINT_DESC_SIZE,            /* bLength */
  USB_ENDPOINT_DESCRIPTOR_TYPE,      /* bDescriptorType */
  USB_ENDPOINT_IN(1),                /* bEndpointAddress */
  USB_ENDPOINT_TYPE_INTERRUPT,       /* bmAttributes */
  WBVAL(0x0040),                     /* wMaxPacketSize */
  0x01,          /* 32ms */          /* bInterval */
/* Terminator */
/* Endpoint, HID Interrupt OUT */
  USB_ENDPOINT_DESC_SIZE,            /* bLength */
  USB_ENDPOINT_DESCRIPTOR_TYPE,      /* bDescriptorType */
  USB_ENDPOINT_OUT(1),                /* bEndpointAddress */
  USB_ENDPOINT_TYPE_INTERRUPT,       /* bmAttributes */
  WBVAL(0x0040),                     /* wMaxPacketSize */
  0x01,          /* 32ms */          /* bInterval */
/* Terminator */
  0                                  /* bLength */
};

/* USB String Descriptor (optional) */
const uint8_t USB_StringDescriptor[] = {
/* Index 0x00: LANGID Codes */
  0x04,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  WBVAL(0x0409), /* US English */    /* wLANGID */
/* Index 0x04: Manufacturer */
  20,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  'A',0,
  'M',0,
  'B',0,
  'O',0,
  't',0,
  'e',0,
  'c',0,
  'h',0,
  ' ',0,
/* Index 0x20: Product */
  26,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  'A',0,
  'M',0,
  'B',0,
  'O',0,
  ' ',0,
  'G',0,
  'T',0,
  '1',0,
  '0',0,
  '2',0,
  '2',0,
  ' ',0,
/* Index 0x44: Serial Number */
  0x1A,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  'A',0,
  'M',0,
  'B',0,
  'O',0,
  ' ',0,
  'V',0,
  '1',0,
  '.',0,
  '0',0,
  '0',0,
  ' ',0,
  ' ',0,
/* Index 0x5E: Interface 0, Alternate Setting 0 */
  0x08,                              /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,        /* bDescriptorType */
  'H',0,
  'I',0,
  'D',0,
};
