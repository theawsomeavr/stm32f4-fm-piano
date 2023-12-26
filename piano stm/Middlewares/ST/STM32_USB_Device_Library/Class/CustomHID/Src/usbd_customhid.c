/**
  ******************************************************************************
  * @file    usbd_customhid.c
  * @author  MCD Application Team
  * @brief   This file provides the CUSTOM_HID core functions.
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  * @verbatim
  *
  *          ===================================================================
  *                                CUSTOM_HID Class  Description
  *          ===================================================================
  *           This module manages the CUSTOM_HID class V1.11 following the "Device Class Definition
  *           for Human Interface Devices (CUSTOM_HID) Version 1.11 Jun 27, 2001".
  *           This driver implements the following aspects of the specification:
  *             - The Boot Interface Subclass
  *             - Usage Page : Generic Desktop
  *             - Usage : Vendor
  *             - Collection : Application
  *
  * @note     In HS mode and when the DMA is used, all variables and data structures
  *           dealing with the DMA during the transaction process should be 32-bit aligned.
  *
  *
  *  @endverbatim
  *
  ******************************************************************************
  */

/* BSPDependencies
- "stm32xxxxx_{eval}{discovery}{nucleo_144}.c"
- "stm32xxxxx_{eval}{discovery}_io.c"
EndBSPDependencies */

/* Includes ------------------------------------------------------------------*/
#include "usbd_customhid.h"
#include "usbd_ctlreq.h"

static uint8_t USB_MIDI_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USB_MIDI_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USB_MIDI_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

static uint8_t USB_MIDI_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USB_MIDI_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USB_MIDI_EP0_RxReady(USBD_HandleTypeDef  *pdev);
#ifndef USE_USBD_COMPOSITE
static uint8_t *USB_MIDI_GetFSCfgDesc(uint16_t *length);
static uint8_t *USB_MIDI_GetHSCfgDesc(uint16_t *length);
static uint8_t *USB_MIDI_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USB_MIDI_GetDeviceQualifierDesc(uint16_t *length);
#endif /* USE_USBD_COMPOSITE  */
/**
  * @}
  */

/** @defgroup USB_MIDI_Private_Variables
  * @{
  */

USBD_ClassTypeDef  USB_MIDI =
{
  USB_MIDI_Init,
  USB_MIDI_DeInit,
  USB_MIDI_Setup,
  NULL, /*EP0_TxSent*/
  USB_MIDI_EP0_RxReady, /*EP0_RxReady*/ /* STATUS STAGE IN */
  USB_MIDI_DataIn, /*DataIn*/
  USB_MIDI_DataOut,
  NULL, /*SOF */
  NULL,
  NULL,
#ifdef USE_USBD_COMPOSITE
  NULL,
  NULL,
  NULL,
  NULL,
#else
  USB_MIDI_GetHSCfgDesc,
  USB_MIDI_GetFSCfgDesc,
  USB_MIDI_GetOtherSpeedCfgDesc,
  USB_MIDI_GetDeviceQualifierDesc,
#endif /* USE_USBD_COMPOSITE  */
};

#ifndef USE_USBD_COMPOSITE
/* USB CUSTOM_HID device FS Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USB_MIDI_CfgDesc[USB_CUSTOM_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
  0x09,                                               /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,                        /* bDescriptorType: Configuration */
  LOBYTE(USB_CUSTOM_HID_CONFIG_DESC_SIZ),             /* wTotalLength: Bytes returned */
  HIBYTE(USB_CUSTOM_HID_CONFIG_DESC_SIZ),
  USBD_MAX_NUM_INTERFACES,                            /* bNumInterfaces: 1 interface */
  0x01,                                               /* bConfigurationValue: Configuration value */
  0x00,                                               /* iConfiguration: Index of string descriptor
                                                         describing the configuration */
#if (USBD_SELF_POWERED == 1U)
  0xC0,                                               /* bmAttributes: Bus Powered according to user configuration */
#else
  0x80,                                               /* bmAttributes: Bus Powered according to user configuration */
#endif /* USBD_SELF_POWERED */
  USBD_MAX_POWER,                                     /* MaxPower (mA) */

  /************** Descriptor of CUSTOM HID interface ****************/
  /* 09 */
  0x09,                                               /* bLength: Interface Descriptor size*/
  USB_DESC_TYPE_INTERFACE,                            /* bDescriptorType: Interface descriptor type */
  0x00,                                               /* bInterfaceNumber: Number of Interface */
  0x00,                                               /* bAlternateSetting: Alternate setting */
  0x00,                                               /* bNumEndpoints*/
  0x01,                                               /* bInterfaceClass: Audio */
  0x01,                                               /* bInterfaceSubClass : MIDI Streaming */
  0x00,                                               /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */
  0x00,                                               /* iInterface: Index of string descriptor */

  // B.3.2 Class-specific AC Interface Descriptor
// The Class-specific AC interface descriptor is always headed by a Header descriptor 
// that contains general information about the AudioControl interface. It contains all 
// the pointers needed to describe the Audio Interface Collection, associated with the 
// described audio function. Only the Header descriptor is present in this device 
// because it does not contain any audio functionality as such.
  /* descriptor follows inline: */
  9,			/* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
  36,			/* descriptor type */
  1,			/* header functional descriptor */
  0x0, 0x01,		/* bcdADC */
  9, 0,			/* wTotalLength */
  1,			/* */
  1,			/* */

// B.4 MIDIStreaming Interface Descriptors

// B.4.1 Standard MS Interface Descriptor
  /* descriptor follows inline: */
  9,			/* length of descriptor in bytes */
  USB_DESC_TYPE_INTERFACE,	/* descriptor type */
  1,			/* index of this interface */
  0,			/* alternate setting for this interface */
  2,			/* endpoints excl 0: number of endpoint descriptors to follow */
  1,			/* AUDIO */
  3,			/* MS */
  0,			/* unused */
  0,			/* string index for interface */

// B.4.2 Class-specific MS Interface Descriptor
  /* descriptor follows inline: */
  7,			/* length of descriptor in bytes */
  36,			/* descriptor type */
  1,			/* header functional descriptor */
  0x0, 0x01,		/* bcdADC */
  65, 0,			/* wTotalLength */

// B.4.3 MIDI IN Jack Descriptor
  /* descriptor follows inline: */
  6,			/* bLength */
  36,			/* descriptor type */
  2,			/* MIDI_IN_JACK desc subtype */
  1,			/* EMBEDDED bJackType */
  1,			/* bJackID */
  0,			/* iJack */

  /* descriptor follows inline: */
  6,			/* bLength */
  36,			/* descriptor type */
  2,			/* MIDI_IN_JACK desc subtype */
  2,			/* EXTERNAL bJackType */
  2,			/* bJackID */
  0,			/* iJack */

//B.4.4 MIDI OUT Jack Descriptor
  /* descriptor follows inline: */
  9,			/* length of descriptor in bytes */
  36,			/* descriptor type */
  3,			/* MIDI_OUT_JACK descriptor */
  1,			/* EMBEDDED bJackType */
  3,			/* bJackID */
  1,			/* No of input pins */
  2,			/* BaSourceID */
  1,			/* BaSourcePin */
  0,			/* iJack */

  /* descriptor follows inline: */
  9,			/* bLength of descriptor in bytes */
  36,			/* bDescriptorType */
  3,			/* MIDI_OUT_JACK bDescriptorSubtype */
  2,			/* EXTERNAL bJackType */
  4,			/* bJackID */
  1,			/* bNrInputPins */
  1,			/* baSourceID (0) */
  1,			/* baSourcePin (0) */
  0,			/* iJack */


// B.5 Bulk OUT Endpoint Descriptors

//B.5.1 Standard Bulk OUT Endpoint Descriptor
  /* descriptor follows inline: */
  9,			/* bLenght */
  USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType = endpoint */
  CUSTOM_HID_EPOUT_ADDR,			/* bEndpointAddress OUT endpoint number 1 */
  2,			/* bmAttributes: 2:Bulk, 3:Interrupt endpoint */
  LOBYTE(CUSTOM_HID_EPOUT_SIZE),
  HIBYTE(CUSTOM_HID_EPOUT_SIZE),
  0,			/* bIntervall in ms */
  0,			/* bRefresh */
  0,			/* bSyncAddress */

// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
  /* descriptor follows inline: */
  5,			/* bLength of descriptor in bytes */
  37,			/* bDescriptorType */
  1,			/* bDescriptorSubtype */
  1,			/* bNumEmbMIDIJack  */
  1,			/* baAssocJackID (0) */

//B.6 Bulk IN Endpoint Descriptors

//B.6.1 Standard Bulk IN Endpoint Descriptor
  /* descriptor follows inline: */
  9,			/* bLenght */
  USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType = endpoint */
  CUSTOM_HID_EPIN_ADDR,			/* bEndpointAddress OUT endpoint number 1 */
  2,			/* bmAttributes: 2:Bulk, 3:Interrupt endpoint */
  LOBYTE(CUSTOM_HID_EPIN_SIZE),
  HIBYTE(CUSTOM_HID_EPIN_SIZE),
  0,			/* bIntervall in ms */
  0,			/* bRefresh */
  0,			/* bSyncAddress */

// B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
  /* descriptor follows inline: */
  5,			/* bLength of descriptor in bytes */
  37,			/* bDescriptorType */
  1,			/* bDescriptorSubtype */
  1,			/* bNumEmbMIDIJack (0) */
  3,			/* baAssocJackID (0) */
};
#endif /* USE_USBD_COMPOSITE  */

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USB_MIDI_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

static uint8_t CUSTOMHIDInEpAdd = CUSTOM_HID_EPIN_ADDR;
static uint8_t CUSTOMHIDOutEpAdd = CUSTOM_HID_EPOUT_ADDR;

static uint8_t USB_MIDI_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);
  USB_MIDI_HandleTypeDef *hhid;

  hhid = (USB_MIDI_HandleTypeDef *)USBD_malloc(sizeof(USB_MIDI_HandleTypeDef));

  if (hhid == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  pdev->pClassDataCmsit[pdev->classId] = (void *)hhid;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    pdev->ep_in[CUSTOMHIDInEpAdd & 0xFU].bInterval = CUSTOM_HID_HS_BINTERVAL;
    pdev->ep_out[CUSTOMHIDOutEpAdd & 0xFU].bInterval = CUSTOM_HID_HS_BINTERVAL;
  }
  else   /* LOW and FULL-speed endpoints */
  {
    pdev->ep_in[CUSTOMHIDInEpAdd & 0xFU].bInterval = CUSTOM_HID_FS_BINTERVAL;
    pdev->ep_out[CUSTOMHIDOutEpAdd & 0xFU].bInterval = CUSTOM_HID_FS_BINTERVAL;
  }

  /* Open EP IN */
  (void)USBD_LL_OpenEP(pdev, CUSTOMHIDInEpAdd, USBD_EP_TYPE_INTR,
                       CUSTOM_HID_EPIN_SIZE);

  pdev->ep_in[CUSTOMHIDInEpAdd & 0xFU].is_used = 1U;

  /* Open EP OUT */
  (void)USBD_LL_OpenEP(pdev, CUSTOMHIDOutEpAdd, USBD_EP_TYPE_INTR,
                       CUSTOM_HID_EPOUT_SIZE);

  pdev->ep_out[CUSTOMHIDOutEpAdd & 0xFU].is_used = 1U;

  hhid->state = CUSTOM_HID_IDLE;

  ((USB_MIDI_ItfTypeDef *)pdev->pUserData[pdev->classId])->Init();

#ifndef USBD_CUSTOMHID_OUT_PREPARE_RECEIVE_DISABLED
  /* Prepare Out endpoint to receive 1st packet */
  (void)USBD_LL_PrepareReceive(pdev, CUSTOMHIDOutEpAdd, hhid->Report_buf,
                               USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);
#endif /* USBD_CUSTOMHID_OUT_PREPARE_RECEIVE_DISABLED */

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USB_MIDI_Init
  *         DeInitialize the CUSTOM_HID layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USB_MIDI_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

#ifdef USE_USBD_COMPOSITE
  /* Get the Endpoints addresses allocated for this class instance */
  CUSTOMHIDInEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
  CUSTOMHIDOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  /* Close CUSTOM_HID EP IN */
  (void)USBD_LL_CloseEP(pdev, CUSTOMHIDInEpAdd);
  pdev->ep_in[CUSTOMHIDInEpAdd & 0xFU].is_used = 0U;
  pdev->ep_in[CUSTOMHIDInEpAdd & 0xFU].bInterval = 0U;

  /* Close CUSTOM_HID EP OUT */
  (void)USBD_LL_CloseEP(pdev, CUSTOMHIDOutEpAdd);
  pdev->ep_out[CUSTOMHIDOutEpAdd & 0xFU].is_used = 0U;
  pdev->ep_out[CUSTOMHIDOutEpAdd & 0xFU].bInterval = 0U;

  /* Free allocated memory */
  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    ((USB_MIDI_ItfTypeDef *)pdev->pUserData[pdev->classId])->DeInit();
    USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USB_MIDI_Setup
  *         Handle the CUSTOM_HID specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t USB_MIDI_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req){ return (uint8_t)USBD_OK; }

#ifndef USE_USBD_COMPOSITE
/**
  * @brief  USB_MIDI_GetFSCfgDesc
  *         return FS configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USB_MIDI_GetFSCfgDesc(uint16_t *length){
    *length = (uint16_t)sizeof(USB_MIDI_CfgDesc);
    return USB_MIDI_CfgDesc;
}

/**
  * @brief  USB_MIDI_GetHSCfgDesc
  *         return HS configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USB_MIDI_GetHSCfgDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USB_MIDI_CfgDesc);
    return USB_MIDI_CfgDesc;
}

/**
  * @brief  USB_MIDI_GetOtherSpeedCfgDesc
  *         return other speed configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USB_MIDI_GetOtherSpeedCfgDesc(uint16_t *length){
    *length = (uint16_t)sizeof(USB_MIDI_CfgDesc);
    return USB_MIDI_CfgDesc;
}
#endif /* USE_USBD_COMPOSITE  */

/**
  * @brief  USB_MIDI_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USB_MIDI_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  UNUSED(epnum);

  /* Ensure that the FIFO is empty before a new transfer, this condition could
  be caused by  a new transfer before the end of the previous transfer */
  ((USB_MIDI_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId])->state = CUSTOM_HID_IDLE;

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USB_MIDI_DataOut
  *         handle data OUT Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USB_MIDI_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  UNUSED(epnum);
  USB_MIDI_HandleTypeDef *hhid;

  if (pdev->pClassDataCmsit[pdev->classId] == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  uint8_t size = USBD_LL_GetRxDataSize (pdev, epnum);
  hhid = (USB_MIDI_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  /* USB data will be immediately processed, this allow next USB traffic being
  NAKed till the end of the application processing */
  ((USB_MIDI_ItfTypeDef *)pdev->pUserData[pdev->classId])->OutEvent(hhid->Report_buf, size);

  return (uint8_t)USBD_OK;
}


/**
  * @brief  USB_MIDI_ReceivePacket
  *         prepare OUT Endpoint for reception
  * @param  pdev: device instance
  * @retval status
  */
uint8_t USB_MIDI_ReceivePacket(USBD_HandleTypeDef *pdev)
{
  USB_MIDI_HandleTypeDef *hhid;

  if (pdev->pClassDataCmsit[pdev->classId] == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

#ifdef USE_USBD_COMPOSITE
  /* Get OUT Endpoint address allocated for this class instance */
  CUSTOMHIDOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  hhid = (USB_MIDI_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  /* Resume USB Out process */
  (void)USBD_LL_PrepareReceive(pdev, CUSTOMHIDOutEpAdd, hhid->Report_buf,
                               USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);

  return (uint8_t)USBD_OK;
}


/**
  * @brief  USB_MIDI_EP0_RxReady
  *         Handles control request data.
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USB_MIDI_EP0_RxReady(USBD_HandleTypeDef *pdev){
  return (uint8_t)USBD_OK;
}

uint8_t USB_MIDI_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len) {
    USB_MIDI_HandleTypeDef *hhid = (USB_MIDI_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

    if (hhid == NULL)
    {
        return (uint8_t)USBD_FAIL;
    }

#ifdef USE_USBD_COMPOSITE
    /* Get Endpoint IN address allocated for this class instance */
    CUSTOMHIDInEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, ClassId);
#endif /* USE_USBD_COMPOSITE */

    if (pdev->dev_state == USBD_STATE_CONFIGURED)
    {
        if (hhid->state == CUSTOM_HID_IDLE)
        {
            hhid->state = CUSTOM_HID_BUSY;
            (void)USBD_LL_Transmit(pdev, CUSTOMHIDInEpAdd, report, len);
        }
        else
        {
            return (uint8_t)USBD_BUSY;
        }
    }
    return (uint8_t)USBD_OK;
}

#ifndef USE_USBD_COMPOSITE
/**
  * @brief  DeviceQualifierDescriptor
  *         return Device Qualifier descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USB_MIDI_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USB_MIDI_DeviceQualifierDesc);
    return USB_MIDI_DeviceQualifierDesc;
}
#endif /* USE_USBD_COMPOSITE  */
/**
  * @brief  USB_MIDI_RegisterInterface
  * @param  pdev: device instance
  * @param  fops: CUSTOMHID Interface callback
  * @retval status
  */
uint8_t USB_MIDI_RegisterInterface(USBD_HandleTypeDef *pdev,
                                          USB_MIDI_ItfTypeDef *fops){
  if (fops == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  pdev->pUserData[pdev->classId] = fops;

  return (uint8_t)USBD_OK;
}
