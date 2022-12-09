# USB-HID-BLER
将有线鼠标键盘游戏手柄等USB HID转换为蓝牙设备，附带鼠键宏和指纹解锁功能。[前作](https://oshwhub.com/dnstzzx/c3-hid)

## 主要功能
- 使用esp32-c3的GPIO模拟USB HOST,识别低速USB HID设备并读取其报告描述符和报告
- **TEANSLATE**工作模式： 检测设备类型，尝试对鼠标和键盘的报告描述符进行解析，将其报告翻译成预定义的标准报告并转发
- **PASSTHOUGH**工作模式： 对于其他类型设备和解析失败的鼠标键盘，将其报告描述符和报告原样转发
- 集成电池管理，可使用电池供电、使用USB供电、使用USB为电池供电
- 鼠键宏： 对于工作在**TEANSLATE**模式的设备可以定义任意鼠标/键盘宏(施工中)
- 指纹解锁：集成指纹模块，通过模拟键盘发送密码实现指纹解锁Windows（施工中）
- 图形化管理软件： 通过蓝牙HID协议无线控制设备状态(施工中)

## ESP32-C3固件
开源地址: [https://github.com/dnstzzx/usb-hid-bler](https://github.com/dnstzzx/usb-hid-bler)
固件代码主要由以下几个部分组成：
- 基于[esp32_usb_soft_host](https://github.com/sdima1357/esp32_usb_soft_host)的软低速USB HOST
> 仅支持低速HID设备,以后可能会考虑用esp32-s3的USB PHY支持全速设备
> 识别方法：对设备供电后，D-被拉高的为低速设备，D+被拉高的为全速/高速设备
- 基于[乐鑫官方例程](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/esp_hid_device)的BLE HID Device,用于实现将报告转发到蓝牙主机
- 解析HID报告描述符并尝试将鼠标键盘设备报告翻译为预定义的标准报告,为便于移植该部分代码已分离到[HID-REPORT-TRANSLATER](https://github.com/dnstzzx/hid-report-translater)
- 鼠键宏， 施工中
- 下位机通讯，施工中

### USB软总线
本作品带有两路USB A接口用于接入HID设备，均为通过GPIO进行模拟。实现源自[esp32_usb_soft_host](https://github.com/sdima1357/esp32_usb_soft_host)，根据原作者的描述存在以下注意事项：
1. 需要将Menuconfig->compiler options->optimization level设置为O2
2. 需要将 Component config-> ESP System Setting -> Memory protection关闭
> idf.py set-target命令会重置优化级别到Og，需要重新设置

USB Host实现主要位于[usb_host.c](https://github.com/dnstzzx/usb-hid-bler/blob/master/main/usb_host.c)，大部分代码通过一个周期为1ms的定时器中断执行。定时器中断ISR代码如下：
```c
void IRAM_ATTR usb_process()
{
#if CONFIG_IDF_TARGET_ESP32C3
	cpu_ll_enable_cycle_count();
#endif	
	for(int k=0;k<NUM_USB;k++)
	{
		current = &current_usb[k];
		if(current->isValid)
		{
			setPins(current->DP,current->DM);
			timerCallBack();  
			fsm_Mashine();
		}
	}
}
```
可见每个定时器周期会对每个USB端口分别执行一次timerCallBack和一次fsm_Mashine。timerCallBack根据上一周期的状态机进行NRZI读写，而fsm_Mashine负责更新状态机。你可能会好奇在ISR中如何进行延时操作(USB低速模式时钟周期为1.5MHz即每传输一位需要等待0.667微秒)。实际上这是通过填充空指令NOP实现的(NOP在Risc-v中被拓展为addi x0, x0, 0 而x0寄存器被硬编码为0，即为一次无意义的加法运算)，填充过程如下：
```c
void (*delay_pntA)() =NULL;
#define cpuDelay(x) {(*delay_pntA)();}

void setDelay(uint8_t ticks)
{
// opcodes of void test_delay() {__asm__ (" nop"); __asm__ (" nop"); __asm__ (" nop"); ...}
//36 41 00 3d f0 1d f0 00 // one  nop
//36 41 00 3d f0 3d f0 3d f0 3d f0 3d f0 1d f0 00  // five  nops
//36 41 00 3d f0 3d f0 3d f0 3d f0 3d f0 3d f0 1d  f0 00 00 00 //
int    MAX_DELAY_CODE_SIZE = 0x210;
uint8_t*     pntS;
	// it can't execute but can read & write
	if(!delay_pntA)
	{
		pntS = heap_caps_aligned_alloc(32,MAX_DELAY_CODE_SIZE, MALLOC_CAP_8BIT);
	}
	else
	{
		pntS = heap_caps_realloc(delay_pntA, MAX_DELAY_CODE_SIZE, MALLOC_CAP_8BIT);
		//~ printf("pntS = %p\n",pntS);
	}
	uint8_t* pnt = (uint8_t*)pntS;
	//put head of delay procedure
	for(int k=0;k<ticks;k++)
	{
		//put NOPs
		*pnt++ = 0x1;
		*pnt++ = 0x0;
	}
	//put tail of delay procedure
	*pnt++ = 0x82;
	*pnt++ = 0x80;
	// move it to executable memory segment
	// it can't  write  but can read & execute
	delay_pntA = heap_caps_realloc(pntS,MAX_DELAY_CODE_SIZE,MALLOC_CAP_32BIT | MALLOC_CAP_EXEC);
	if(!delay_pntA)
	{
		printf("idf.py menuconfig\n Component config-> ESP System Setting -> Memory protectiom-> Disable.\n memory prot must be disabled!!!\n delay_pntA = %p\n",delay_pntA);
		exit(0);
	}
}
```
这段代码向代码段中注入了ticks个nop，执行效果即为延迟ticks个nop指令周期。这就是为什么需要关闭内存保护功能(需要写入代码段)。延迟0.667微秒的ticks数初始值为110，但是每次初始化时都会测量nop+GPIO输出执行时间对这个值进行校准。

### 获取报告描述符
在HID协议中，为了让报告内容更为紧凑与灵活，USB-IF设计了一种复杂的协议用于解释报告内容的组织方式及含义，称为报告描述符。其详细定义可以在[此文档](https://www.usb.org/sites/default/files/hid1_11.pdf)中第33页起的部分找到。为了获得报告描述符，我们需要在usb状态机中加入对应[初始化时序](https://github.com/dnstzzx/usb-hid-bler/blob/28c5d8abcae392b7f8c73246b9ec263f39b71753/main/usb_host.c#L1396)。
```c
}else if(current->fsm_state == 660){
		Request(T_SETUP,ASSIGNED_USB_ADDRESS,0b0000,T_DATA0,0x81,0x6,0x2200,0x0000,DEF_BUFF_SIZE,current->hid[current->hid_report_desc_count].wDescriptorLength); // 请求报告描述符
		current->fsm_state    = 661; 
		return;
	 }else if(current->fsm_state == 661){
		int len = current->hid[current->hid_report_desc_count].wDescriptorLength;
		if(current->acc_decoded_resp_counter==len)
		{
			memcpy(&current->hid_report_desc_buffer[current->hid_report_desc_count],current->acc_decoded_resp,len);
			current->hid_report_desc_count ++;
			if(current->hid_report_desc_count == current->hid_count){
				current->ufPrintDesc |= 32;
				current->fsm_state    = 98;
				on_device_in(current);
				return;
			}else{
				current->fsm_state = 660;
				return;
			}
		}else{
			current->fsm_state      = 0;
			return ;
		}
	 }
```
## 翻译模式
获得报告描述符后会首先根据描述符头部的Usage Page和Usage字段判断插入设备的用途。对于鼠标(键盘待实现)设备的描述符进行解析，获取其各按钮、X坐标、Y坐标输入的长度、报告偏移量、报告值范围。然后计算其与我们预定义的鼠标键盘报告的线性变换关系:
```c
typedef struct {
    unsigned defined: 1;
    unsigned data_signed: 1;
    uint8_t byte_offset;
    uint8_t bit_offset;
    uint8_t bit_count;

    // data transform, out = (in + pre_scale_bias) * scale_factor + post_scale_bias
    int32_t pre_scale_bias;
    int32_t post_scale_bias;
    double scale_factor;
}translate_item_t;

typedef struct{
    uint8_t report_id;
    translate_item_t buttons;
    translate_item_t x;
    translate_item_t y;
    translate_item_t wheel;
}mouse_translate_t;
```
预定义的鼠标报告模型：
```c
#pragma pack(1)
typedef struct {
    uint8_t report_id;
    uint8_t buttons;    // 8 buttons available
    int16_t x;  // -32767 to 32767
    int16_t y;  // -32767 to 32767
    int16_t wheel;  // -32767 to 32767
}standard_mouse_report_t;
#pragma pack()
```
其对应的报告描述符为:
```c
0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
0x09, 0x02,        // Usage (Mouse)
0xA1, 0x01,        // Collection (Application)
0x85, 0x01,        //   Report ID (66)
0x09, 0x01,        //   Usage (Pointer)
0xA1, 0x00,        //   Collection (Physical)
0x05, 0x09,        //     Usage Page (Button)
0x19, 0x01,        //     Usage Minimum (0x01)
0x29, 0x08,        //     Usage Maximum (0x08)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x01,        //     Logical Maximum (1)
0x95, 0x08,        //     Report Count (8)
0x75, 0x01,        //     Report Size (1)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
0x16, 0x01, 0x80,  //     Logical Minimum (-32767)
0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
0x09, 0x30,        //     Usage (X)
0x09, 0x31,        //     Usage (Y)
0x09, 0x38,        //     Usage (Wheel)
0x75, 0x10,        //     Report Size (16)
0x95, 0x03,        //     Report Count (3)
0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0xC0,              // End Collection
```
不难看出，报告长度为8字节，第一个字节为report id，第二个字节为8个按钮各占一位。第3-4、5-6、7-8字节为分别为X和Y坐标位移量、鼠标滚轮输入，均为16位有符号整数(USB为小端序协议)。保留8个按钮是为了方便鼠标宏定义及二次开发，常用的左键、右键、中键分别为第0、1、2位。解析的代码主要位于[此文件](https://github.com/dnstzzx/hid-report-translater/blob/master/parse.c)中，可以解析大部分报告描述符不仅限于鼠标键盘。即便少数没有加入的冷门标签也可以通过简单增加几行代码完成解析。同理，[翻译模块](https://github.com/dnstzzx/hid-report-translater/blob/master/translate.c)定义了与设备本身无关的翻译流程，想要加入新的翻译模式设备(如游戏手柄)并不困难。

### 直通模式
对于不支持翻译的设备类型或者解析失败的鼠标键盘，设备会将其报告描述符和报告原样转发给蓝牙主机，这一工作方式称为直通(PASSTHOUGH)模式。由于特殊的USB工作模式该模式会比即插即用的翻译模式稍显麻烦。主要包括以下问题
1. 由于未知原因，开启usb后第一次完成蓝牙连接会导致程序崩溃，因此需要先连接一次蓝牙才会开启USB总线
2. 转发的报告描述符需要在蓝牙连接前确定，因此连接新的直通模式设备后设备会将其报告描述符写入Flash中然后重启，在以后每一次蓝牙连接时转发
3. 至少在Windows系统中(其他没试过)，已配对的设备再次连接时会直接使用以前获取的报告描述符而不会更新，因此插入新的直通模式设备后需要重新配对
4. 综合上述三项，接入一个直通模式设备的正确步骤是：连接蓝牙->插入新设备->(自动)重启->重新配对->连接使用

### 指纹解锁
待更新

## 宏
宏以当前任意翻译模式设备的标准报告作为输入，触发后以任意标准模型报告作为输出。
宏以输入而不是输出作为分类标准，即鼠标宏是以鼠标模型作为输入的宏，但可能会输出键盘报告，反之同理。
目前只完成了鼠标宏，等完成其他部分再来回填（：
宏定义包含以下要素:触发方式、动作方式、动作内容，对于鼠标宏而言其具体形式如下:
```c
typedef struct{
    saved_list_head_t head;     // 链表头
    uint8_t     version;
    uint8_t     trigger_buttons_mask;   // 触发方式
    // 动作方式
    uint8_t    cancel_input_report;
    macro_output_model_t    output_model;
    uint8_t     action_type;
    uint16_t    action_delay;       // in ms
    uint16_t    report_duration;    // for toggle type anction, in ms
    // 动作内容
    union{
        standard_mouse_report_t mouse_output_report;
    };
    
}mouse_macro_t;
```
各字段含义可以在这里找对应（懒得写力
![0a56f68080331c59e524191f464c3b1.png](https://image.lceda.cn/pullimage/eHwQ9LCIZO2EuHs1ERYLI34NoSVvWdEDBr35cUdv.png)

## WEB管理界面
上位机主要用于查看设备状态和配置鼠标宏等，基于vue3+typescript编写。使用浏览器提供的[HID API](https://developer.mozilla.org/en-US/docs/Web/API/WebHID_API)与蓝牙连接的设备进行通信，为纯前端页面不需要后端。目前仍在开发阶段，开源地址：[https://github.com/dnstzzx/HID-BLER-Manager](https://github.com/dnstzzx/HID-BLER-Manager)。
> WEB HID目前仍属于实验性功能，请使用PC端版本号高于89的Chrome/Edge或版本号高于75的Opera浏览器访问。
> ![image.png](https://image.lceda.cn/pullimage/8oWgOt3x2MtNGuIbNSe6PInxrKv2ejxxmdqMfW7i.png)

### 通信协议
设备与管理应用的通信也是通过HID协议来完成的。对应的描述符如下：
```c
0x06, 0x01, 0xFF,  // Usage Page (Vendor Defined 0xFF01)
0x09, 0x01,        // Usage (0x01)
0xA1, 0x01,        // Collection (Application)
0x85, 0x03,        //   Report ID (3)
0x09, 0x01,        //   Usage (0x01)
0x15, 0x00,        //   Logical Minimum (0)
0x26, 0xFF, 0x00,  //   Logical Maximum (255)
0x75, 0x08,        //   Report Size (8)
0x96, 0x80, 0x00,  //   Report Count (128)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0x01,        //   Usage (0x01)
0x25, 0x00,        //   Logical Maximum (0)
0x26, 0xFF, 0x00,  //   Logical Maximum (255)
0x75, 0x08,        //   Report Size (8)
0x96, 0x80, 0x00,  //   Report Count (128)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x85, 0x04,        //   Report ID (4)
0x09, 0x02,        //   Usage (0x02)
0x15, 0x00,        //   Logical Minimum (0)
0x26, 0xFF, 0x00,  //   Logical Maximum (255)
0x75, 0x08,        //   Report Size (8)
0x96, 0x00, 0x02,  //   Report Count (512)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0x02,        //   Usage (0x02)
0x25, 0x00,        //   Logical Maximum (0)
0x26, 0xFF, 0x00,  //   Logical Maximum (255)
0x75, 0x08,        //   Report Size (8)
0x96, 0x00, 0x02,  //   Report Count (512)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              // End Collection
```
上述描述符定义了两个输入报告和输出报告。这是因为HID协议是定长传输，但传输的信息不定长，为了保证长信息能够顺利传输的同时尽可能降低短消息通信延迟所以分开两个通道进行报告。
对于短消息(消息体短于128字节)使用Report ID 3通道一次传输完成，长度固定为128字节。
通信流程采用类似于http的请求-回应模型。其消息体基本结构如下：
```c
#pragma pack(1)
typedef struct{
    uint16_t opcode;    // 0
    uint16_t session;   // 2
    uint16_t length;    // 4
    uint8_t  data[];   // 6
}request_t;

typedef struct{
    uint16_t session;   //  0
    uint16_t length;    //  2
    uint8_t  success;   //  4
    uint8_t  data[];   // 5
}response_t;
#pragma pack()
```
其中opcode为需要请求的功能，类似于http method，session用于区分被响应的请求。
对于request请求的data，可能是文本也可能是JSON Object文本，区分方式为第一个字符是否"{"。
对于response也是类似的，只是当success为0即请求操作失败时data一定是普通文本用于解释失败原因。

而对于更长的消息，我们需要将其进行切片传输，对端收到后在将其重新拼合。每个切片长度为508字节，加上切片包头共512字节(所以存在只有一个切片的情况)。切片包结构为：
```c
typedef struct{
    uint16_t session;       // 0
    uint8_t block_count;   // 2
    uint8_t block_id;      // 3
    uint8_t  data[508]; // 4
}long_msg_block_t;
```
需要注意的是该结构体中data含义与短包中data不同，该data的拼合体还包括了请求/回应中的opcode/session/length/success等字段。

## 实物组装图
![c5d6894954eaa6afc50b0fd3933d925.jpg](https://image.lceda.cn/pullimage/kuXhRXwkGLmbEZr05WZSv1Po48Ym2vikqcEKxglZ.jpeg)
![92124ac7df2f04a0ee18444901f58df.jpg](https://image.lceda.cn/pullimage/fCSHFaImxMIxgWVSMs16gOl2meFz9MIppF5p0RzU.jpeg)
![ef415715918a88928a9e06b4f5b1b15.jpg](https://image.lceda.cn/pullimage/ejI1dkaLiq9PB4zGcJmmvkHVNx6VD28EjBkM9RTr.jpeg)
![67834b9ff32243efd19853806ad6c8f.jpg](https://image.lceda.cn/pullimage/5ipFF1s13PMse63NgD0sZv2jWtU8pQ5chT5oHVL3.jpeg)