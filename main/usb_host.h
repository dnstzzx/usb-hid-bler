#pragma once
#include <assert.h>
#include "report_map.h"

#define TIMER_DIVIDER         2  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.001) // sample test interval for the first timer


// non configured device -  must be zero
#define  ZERO_USB_ADDRESS   0

// any number less 127, but no zero
#define  ASSIGNED_USB_ADDRESS    3

#define DEF_BUFF_SIZE 0x100	


void printState();
void usb_process();

#define  NUM_USB 2
void initStates( int DP0,int DM0,int DP1,int DM1,int DP2,int DM2,int DP3,int DM3);
void usbMess(uint8_t src,uint8_t len,uint8_t *data);
void usbSetFlags(int _usb_num,uint8_t flags);
uint8_t usbGetFlags(int _usb_num);

typedef __packed struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} sDevDesc;

typedef __packed struct
{
    uint8_t bLength;
	uint8_t bType;
	uint16_t wLength;
	uint8_t bNumIntf;
	uint8_t bCV;
	uint8_t bIndex;
	uint8_t bAttr;
	uint8_t bMaxPower;
} sCfgDesc;
typedef __packed struct
{
    uint8_t bLength;
	uint8_t bType;
	uint8_t iNum;
	uint8_t iAltString;
	uint8_t bEndPoints;
	uint8_t iClass;
	uint8_t iSub;
	uint8_t iProto;
	uint8_t iIndex;
} sIntfDesc;
typedef __packed struct
{
    uint8_t bLength;
	uint8_t bType;
	uint8_t bEPAdd;
	uint8_t bAttr;
	uint16_t wPayLoad;               /* low-speed this must be 0x08 */
	uint8_t bInterval;
} sEPDesc;

#pragma pack(1)
typedef  struct
{
  uint8_t   bLength;
  uint8_t   bDescriptorType;
  uint16_t  bcdHID;
  uint8_t   bCountryCode;
  uint8_t   bNumDescriptors;
  uint8_t   bReportDescriptorType;
  uint16_t  wDescriptorLength;
} HIDDescriptor;
#pragma pack()


typedef __packed struct
{
    uint8_t bLength;
	uint8_t bType;
	uint16_t wLang;
} sStrDesc;


typedef __packed struct
{
	uint8_t cmd;
	uint8_t addr;
	uint8_t eop;

	uint8_t  dataCmd;
	uint8_t  bmRequestType;
	uint8_t  bmRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLen;
}Req;
enum    DeviceState 	{ NOT_ATTACHED,ATTACHED,POWERED,DEFAULT,ADDRESS,
					PARSE_CONFIG,PARSE_CONFIG1,PARSE_CONFIG2,PARSE_CONFIG3,
					POST_ATTACHED,RESET_COMPLETE,POWERED_COMPLETE,DEFAULT_COMPL} ;

enum  CallbackCmd {CB_CHECK,CB_RESET,CB_WAIT0,CB_POWER,CB_TICK,CB_2,CB_2Ack,CB_3,CB_4,CB_5,CB_6,CB_7,CB_8,CB_9,CB_WAIT1} ;


typedef struct{

}Mouse_Transform;

typedef struct{
	
}Keyboard_Transform;

typedef union{
	Mouse_Transform mouse_transform;
	Keyboard_Transform keyboard_transform;
}Transform;

//Req rq;
typedef struct
{
int 			isValid;	
int 			selfNum;
int                    epCount;	
int 			cnt;
uint8_t             flags_new;
uint8_t             flags;
	
uint32_t 		DP;
uint32_t 		DM;
volatile enum CallbackCmd   	cb_Cmd;
volatile enum DeviceState    	fsm_state;
volatile 		uint16_t      	wires_last_state;
sDevDesc 	desc;
sCfgDesc  	cfg; 
Req 			rq;

int 			counterNAck;
int 			counterAck;

uint8_t     	descrBuffer[DEF_BUFF_SIZE];
uint8_t     	descrBufferLen;

volatile int    bComplete;
volatile int    in_data_flip_flop;
int     	      cmdTimeOut;
uint32_t  	      ufPrintDesc;
int        	      numb_reps_errors_allowed;

uint8_t    acc_decoded_resp[DEF_BUFF_SIZE];
uint8_t    acc_decoded_resp_counter;

int          asckedReceiveBytes;
int 	       transmitL1Bytes;
uint8_t    transmitL1[DEF_BUFF_SIZE];

uint8_t hid_count;
HIDDescriptor 	hid[4];
Report_Map_BUFF hid_report_desc_buffer[4];
uint8_t hid_report_desc_count;

//Transform transform[4];

//~ uint8_t   Resp0[DEF_BUFF_SIZE];
//~ uint8_t   R0Bytes;
//~ uint8_t   Resp1[DEF_BUFF_SIZE];
//~ uint8_t   R1Bytes;

} sUsbContStruct;

//void on_new_hid_report_map(sUsbContStruct *usb, uint8_t hidid);
void on_device_in(sUsbContStruct *usb);


