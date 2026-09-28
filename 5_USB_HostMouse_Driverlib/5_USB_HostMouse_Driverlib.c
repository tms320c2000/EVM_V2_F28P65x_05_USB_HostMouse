// 파일이름	:	5_USB_HostMouse_Driverlib.c
// 대상장치	:	TMS320F28X EVM V2, TMS320F28P65x(F28P650DK9 산업용 / F28P659DK8-Q1 차량용) 모듈
// 파일버전	:	1.00
// 갱신이력	:	2026-09-20, 버전 1.00
// 예제설명	:

//************************************************************************************************************************************************************************
//
// 본 예제는 TMS320F28P65x 모듈이 탑재된 TMS320F28X 개발보드(EVM) V2를 대상으로 하고 있으며,
// F28P65x의 On-Chip USB(USB0)를 호스트(Host) 모드로 동작시켜 USB 마우스를 연결하고,
// 마우스의 좌우(X축) 움직임에 따라 개발보드 (8) 범용 LED 16개 블록의 점등 위치를
// 좌우로 이동시킵니다.
//
// a. 배선 안내:
//
//		>> USB: 개발보드 (5) USB 통신 회로(USB0, CN4600 계열 커넥터)는 원래
//		   TMS320F2837x/F2838x용 ControlCARD 구성이라(매뉴얼 12장), F28P65x 모듈에서는
//		   "칩 전용 핀이라 점퍼가 필요 없다"는 말이 성립하지 않습니다 — 아래처럼
//		   A/B Side 핀-헤더에서 (5)번 블록의 USB 인터페이스 인출 핀-헤더까지 점퍼
//		   케이블로 직접 연결해야 합니다(2026-09-28 실측 배선으로 확인):
//		     - A Side 핀-헤더 15번(GPIO42, USB0DM) → (5) 블록 "D-"
//		     - A Side 핀-헤더 17번(GPIO43, USB0DP) → (5) 블록 "D+"
//		     - A Side 핀-헤더 19번(GPIO46, VBUS 감지) → (5) 블록 "VBUS"
//		     - A Side 핀-헤더 21번(GPIO47, ID) → (5) 블록 "ID"
//		     - B Side 핀-헤더 45번(GPIO50, EPEN) → (5) 블록 "EPEN"
//		     - B Side 핀-헤더 47번(GPIO49, PFLT) → (5) 블록 "PFLT"
//		   (EPEN/PFLT가 F28P65x에서 GPIO50/GPIO49인 이유는 usb_hal.c의
//		   USBGPIOEnable() 함수 위 주석 참고.)
//		   USB 마우스는 (5)번 회로 Top면의 4핀 표준 USB 커넥터(Type-A)에 직접
//		   연결하거나, USB 허브를 통해 연결하세요.
//		>> LED: 개발보드 A Side 핀-헤더 65, 67, 69, 71, 73, 75, 77, 79, 81, 83, 85, 87,
//		   89, 91, 93, 95번(홀수, GPIO0~GPIO15에 순서대로 1:1 대응)을 (8) 범용 LED 16개
//		   블록의 입력 16핀에 순서대로 연결하세요 — 8_GP_LED16 예제와 배선이 동일합니다.
//
// 예제 폴더의 CCS Expressions 창에 아래 변수를 등록해두면 동작을 실시간으로 확인할 수 있습니다.
// --> usbHostState(0=미연결, 1=마우스 초기화 중, 2=마우스 연결됨, 3=미지원 장치, 4=전원 오류)
// --> ledIndex(0~15, 현재 점등된 LED 위치), mouseCursorX(누적 X 이동량, 디버깅용)
// --> mouseButtons(bit0=좌클릭, bit1=우클릭, bit2=휠클릭)
//
// 본 예제는 TI가 제공하는 Driver API Library(DriverLib) + USB Host Library(usblib) 코드로
// 제작되었습니다 (SysConfig 미사용). TI의 공식 레퍼런스 예제
// `driverlib/f28p65x/examples/c28x/usb/usb_ex5_host_mouse.c`를 기반으로, SysConfig가
// 생성해주던 초기화 코드(Board_init/C2000Ware_libraries_init)를 SysConfig 없이 직접
// 호출하는 형태로 이식하고, SCI 콘솔 출력 대신 LED16 GPIO 출력을 붙였습니다. 같은 동작을
// 레지스터를 직접 조작하는 Bit-Field 방식으로 구현한 5_USB_HostMouse_Bitfield.c,
// FreeRTOS 태스크로 구현한 5_USB_HostMouse_Freertos.c와 함께 세 가지 방식을 비교해
// 보실 수 있습니다.
//
// b. USB 60MHz 클럭: F28P65x의 USB 컨트롤러는 SYSCLK가 아니라 별도의 AUXPLL 60MHz 클럭이
//    필요합니다. `SysCtl_setAuxClock(DEVICE_AUXSETCLOCK_CONFIG_USB)`(usb_hal.h)가 보드의
//    25MHz BAW 오실레이터 기준으로 AUXPLLCLK 120MHz -> /2 -> 60MHz를 만들어 줍니다.
//
// c. 마우스 좌우 이동 -> LED 위치 매핑: `MouseCallback()`의 `USBH_EVENT_HID_MS_X` 이벤트에서
//    마우스가 보고하는 8비트 부호있는 상대 이동량을 누적하다가, `MOUSE_STEP_THRESHOLD`
//    (기본 12카운트)를 넘을 때마다 `ledIndex`를 한 칸씩 좌/우로 옮기고 `updateLedDisplay()`로
//    실제 GPIO를 갱신합니다. 양쪽 끝(0, 15)에서는 더 이상 넘어가지 않고 멈춥니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "driverlib.h"		// TI 제공 Driver API Library 헤더파일 (driverlib)
#include "device.h"
#include "usb_hal.h"		// USBGPIOEnable(), DEVICE_AUXSETCLOCK_CONFIG_USB (TI c2000_specific 헬퍼)
#include "usblib.h"
#include "usbhid.h"
#include "host/usbhost.h"
#include "host/usbhhid.h"
#include "host/usbhhub.h"
#include "host/usbhhidmouse.h"

// 전처리 구문 정의
#define	LED_COUNT				16U		// 제어 대상 LED 개수
#define	MOUSE_STEP_THRESHOLD	12		// 마우스 X 누적 이동량이 이 값을 넘을 때마다 LED가 한 칸 이동
#define	MOUSE_MEMORY_SIZE		128U	// USB 마우스 클래스 드라이버용 메모리 풀 크기(byte)
#define	HCD_MEMORY_SIZE			128U	// USB 호스트 컨트롤러 드라이버(HCD)용 메모리 풀 크기(byte)

// LED 번호(배열 인덱스) - GPIO 번호 대응표
// [2026-09-28 수정] 실제 보드에서 LED 점등 위치가 좌우 반대로 보여 배열을 반전시킴
// (ledIndex=0일 때 GPIO15, ledIndex=15일 때 GPIO0이 켜지도록). updateLedDisplay()가
// 이미 ledGpio[ledIndex]를 참조하므로 이 배열만 바꾸면 됩니다(Bitfield 버전과 달리
// updateLedDisplay() 자체는 손댈 필요 없음).
static const uint16_t ledGpio[LED_COUNT] =
{
    15U, 14U, 13U, 12U, 11U, 10U, 9U, 8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U
};

// 마우스/호스트 컨트롤러 드라이버용 메모리 풀
static uint8_t g_pui8MouseBuffer[MOUSE_MEMORY_SIZE];
static uint8_t g_pui8HCDPool[HCD_MEMORY_SIZE];

// USB 이벤트 드라이버(연결/해제/미지원장치/전원오류 등 일반 이벤트) 선언
DECLARE_EVENT_DRIVER(g_sUSBEventDriver, 0, 0, USBHCDEvents);

// 이 예제가 사용하는 호스트 클래스 드라이버 목록 (HID 마우스 + 일반 이벤트)
static tUSBHostClassDriver const * const g_ppHostClassDrivers[] =
{
    &g_sUSBHIDClassDriver,
    &g_sUSBEventDriver
};
static const uint32_t g_ui32NumHostClassDrivers =
    sizeof(g_ppHostClassDrivers) / sizeof(tUSBHostClassDriver *);

static tUSBHMouse *g_psMouseInstance;

// USB 마우스 연결 상태 머신 (TI 레퍼런스 예제와 동일한 상태 구성)
enum
{
    STATE_NO_DEVICE = 0,		// 장치 없음
    STATE_MOUSE_INIT,			// 마우스 감지됨, 메인 루프에서 초기화 필요
    STATE_MOUSE_CONNECTED,		// 마우스 연결되어 이벤트 대기 중
    STATE_UNKNOWN_DEVICE,		// 미지원 장치가 연결됨(마우스 아님)
    STATE_POWER_FAULT			// 전원 오류 발생
}
g_eUSBState;

// 함수 원형 선언
void initLedGpio(void);
void updateLedDisplay(void);
void USBHCDEvents(void *pvData);
void ModeCallback(uint32_t ui32Index, tUSBMode eMode);
void MouseCallback(tUSBHMouse *psMsInstance, uint32_t ui32Event,
                    uint32_t ui32MsgParam, void *pvMsgData);
__interrupt void usbHostISR(void);


// 전역 변수 선언 (CCS Expressions/Watch 창에서 실시간으로 값을 확인할 수 있는 전역 변수)
volatile uint16_t	usbHostState = 0U;	// g_eUSBState의 숫자 사본(enum이 Expressions 창에 안 보일 때 대비)
volatile uint16_t	ledIndex = 8U;		// 현재 점등된 LED 위치(0~15), 시작 위치는 중앙 부근
volatile int32_t	mouseCursorX = 0;	// 누적 마우스 X 이동량(디버깅/표시용, 범위 제한 없음)
volatile uint32_t	mouseButtons = 0UL;	// bit0=좌클릭, bit1=우클릭, bit2=휠클릭

static int32_t motionAccumX = 0;		// LED_STEP 단위로 소비되는 X 누적 이동량 (내부용)


// 메인 함수
void main(void)
{

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
//	   (Interrupt_initModule() 함수가 한 번에 담당합니다)


//	2. 시스템 초기화 - Device_init( ) 함수 호출
	Device_init();

//	* 범용 입출력 포트(GPIO) 핀 락 해제, 내부 풀업 활성화 - Device_initGPIO( ) 함수 호출
	Device_initGPIO();


//	3. 주변회로 인터럽트 확장회로 초기화 - Interrupt_initModule( ) 함수 호출
	Interrupt_initModule();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - Interrupt_initVectorTable( ) 함수 호출
	Interrupt_initVectorTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 연결
	Interrupt_register(INT_USBA, &usbHostISR);


//	6. 주변회로 초기화 - LED16 GPIO 출력 + USB0 호스트 모드
	initLedGpio();
	updateLedDisplay();

	// USB 컨트롤러용 60MHz AUXPLL 클럭 설정 (25MHz BAW 오실레이터 기준)
	SysCtl_setAuxClock(DEVICE_AUXSETCLOCK_CONFIG_USB);

	// USB0DM/DP/VBUS/ID 핀 활성화 (usb_hal.c, TI 표준 헬퍼)
	USBGPIOEnable();

	Interrupt_enable(INT_USBA);


//	7. 전역 변수 및 S/W 모듈 초기화
	g_eUSBState = STATE_NO_DEVICE;
	usbHostState = (uint16_t)STATE_NO_DEVICE;
	mouseCursorX = 0;
	mouseButtons = 0UL;
	motionAccumX = 0;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. USB 호스트 스택 초기화 (SysConfig의 Board_init()/C2000Ware_libraries_init()가
//	   대신 해주던 일을 직접 호출합니다 - TI usb_ex5_host_mouse.syscfg를 SysConfig로
//	   미리 생성해 그 내용을 그대로 옮겼습니다)
	USBStackModeSet(0, eUSBModeForceHost, ModeCallback);

	USBHCDRegisterDrivers(0, g_ppHostClassDrivers, g_ui32NumHostClassDrivers);

	g_psMouseInstance = USBHMouseOpen(MouseCallback, g_pui8MouseBuffer, MOUSE_MEMORY_SIZE);

	USBHCDPowerConfigInit(0, USBHCD_VBUS_AUTO_HIGH | USBHCD_VBUS_FILTER);

	// 2ms 폴링 주기로 OTG 동작을 위한 USB 컨트롤러 초기화
	USBHCDInit(0, g_pui8HCDPool, HCD_MEMORY_SIZE);


//	10. Idle(Background) Loop - USB 호스트 스택 폴링 + 마우스 상태 머신
	for(;;)
	{
		// USB 호스트 컨트롤러 드라이버의 메인 루틴(열거, 클래스 드라이버 폴링 등)
		USBHCDMain();

		switch(g_eUSBState)
		{
			case STATE_MOUSE_INIT:
				// 새로 연결된 마우스를 초기화합니다(콜백 안에서는 호출할 수 없음)
				USBHMouseInit(g_psMouseInstance);
				g_eUSBState = STATE_MOUSE_CONNECTED;
				break;

			default:
				// STATE_NO_DEVICE / STATE_MOUSE_CONNECTED / STATE_UNKNOWN_DEVICE /
				// STATE_POWER_FAULT 상태에서는 메인 루프에서 추가로 할 일이 없습니다.
				break;
		}

		usbHostState = (uint16_t)g_eUSBState;
	}
}

//	11. 인터럽트 서비스 루틴 및 기타 함수들

//
// initLedGpio - GPIO0~15를 디지털 출력으로 설정합니다 (8_GP_LED16_Driverlib.c와 동일).
//
void initLedGpio(void)
{
    uint16_t i;

    for(i = 0U; i < LED_COUNT; i++)
    {
        GPIO_setPadConfig(ledGpio[i], GPIO_PIN_TYPE_STD);
        GPIO_setDirectionMode(ledGpio[i], GPIO_DIR_MODE_OUT);
    }
}

//
// updateLedDisplay - 16개 LED를 모두 끈 뒤 ledIndex에 해당하는 LED 하나만 켭니다.
//
void updateLedDisplay(void)
{
    uint16_t i;

    for(i = 0U; i < LED_COUNT; i++)
    {
        GPIO_writePin(ledGpio[i], 0U);
    }
    GPIO_writePin(ledGpio[ledIndex], 1U);
}

//
// USBHCDEvents - USB 호스트 스택의 일반 이벤트(연결/해제/미지원 장치/전원 오류) 콜백.
// HID 마우스가 연결되면 STATE_MOUSE_INIT으로 전이시켜, 메인 루프에서 USBHMouseInit()이
// 호출되게 합니다(콜백 컨텍스트에서는 USBHMouseInit()을 직접 호출할 수 없습니다).
//
void USBHCDEvents(void *pvData)
{
    tEventInfo *pEventInfo = (tEventInfo *)pvData;

    switch(pEventInfo->ui32Event)
    {
        case USB_EVENT_CONNECTED:
            if((USBHCDDevClass(pEventInfo->ui32Instance, 0) == USB_CLASS_HID) &&
               (USBHCDDevProtocol(pEventInfo->ui32Instance, 0) == USB_HID_PROTOCOL_MOUSE))
            {
                g_eUSBState = STATE_MOUSE_INIT;
            }
            break;

        case USB_EVENT_UNKNOWN_CONNECTED:
            g_eUSBState = STATE_UNKNOWN_DEVICE;
            break;

        case USB_EVENT_DISCONNECTED:
            g_eUSBState = STATE_NO_DEVICE;
            mouseButtons = 0UL;
            break;

        case USB_EVENT_POWER_FAULT:
            g_eUSBState = STATE_POWER_FAULT;
            break;

        default:
            break;
    }
}

//
// ModeCallback - USB OTG 모드 변경 콜백. eUSBModeForceHost로 고정했으므로 실제로는
// 호출되지 않지만, USBStackModeSet()의 필수 인자라 TI 레퍼런스와 동일하게 둡니다.
//
void ModeCallback(uint32_t ui32Index, tUSBMode eMode)
{
    (void)ui32Index;
    (void)eMode;
}

//
// MouseCallback - USB 마우스 버튼/이동 이벤트 콜백. X축 이동량을 누적하다가
// MOUSE_STEP_THRESHOLD를 넘을 때마다 ledIndex를 좌/우로 한 칸씩 옮깁니다.
//
void MouseCallback(tUSBHMouse *psMsInstance, uint32_t ui32Event,
                    uint32_t ui32MsgParam, void *pvMsgData)
{
    int16_t deltaX;
    (void)psMsInstance;
    (void)pvMsgData;

    switch(ui32Event)
    {
        case USBH_EVENT_HID_MS_PRESS:
            mouseButtons |= ui32MsgParam;
            break;

        case USBH_EVENT_HID_MS_REL:
            mouseButtons &= ~ui32MsgParam;
            break;

        case USBH_EVENT_HID_MS_X:
            // 마우스는 8비트 부호있는 상대 이동량을 보내므로 부호 확장이 필요합니다.
            if(ui32MsgParam & 0x80U)
            {
                ui32MsgParam |= 0xFF00U;
            }
            deltaX = (int16_t)ui32MsgParam;
            mouseCursorX += deltaX;

            motionAccumX += deltaX;
            while(motionAccumX >= MOUSE_STEP_THRESHOLD)
            {
                motionAccumX -= MOUSE_STEP_THRESHOLD;
                if(ledIndex < (LED_COUNT - 1U))
                {
                    ledIndex++;
                    updateLedDisplay();
                }
            }
            while(motionAccumX <= -MOUSE_STEP_THRESHOLD)
            {
                motionAccumX += MOUSE_STEP_THRESHOLD;
                if(ledIndex > 0U)
                {
                    ledIndex--;
                    updateLedDisplay();
                }
            }
            break;

        case USBH_EVENT_HID_MS_Y:
            // 이 예제는 좌우(X축) 이동만 사용하므로 Y축은 무시합니다.
            break;

        default:
            break;
    }
}

//
// usbHostISR - USB0 호스트 인터럽트 래퍼. USB 호스트 라이브러리의 실제 처리 함수를
// 호출한 뒤 PIE 그룹9 Acknowledge를 수행합니다.
//
#ifdef _FLASH
#pragma CODE_SECTION(usbHostISR, ".TI.ramfunc");
#endif
__interrupt void usbHostISR(void)
{
    USB0HostIntHandler();
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

//
// 참고: DriverLib ASSERT()/usblib.lib이 호출하는 __error__ 핸들러는 이 프로젝트가
// 복사해 온 device/device.c에 이미 정의되어 있어 별도로 정의하지 않습니다(비트필드
// 버전은 device.c가 없어 직접 정의합니다 - 5_USB_HostMouse_Bitfield.c 참고).

// 파일 끝.
