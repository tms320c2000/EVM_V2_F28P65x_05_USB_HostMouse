// 파일이름	:	5_USB_HostMouse_Bitfield.c
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
//		   (EPEN/PFLT가 F28P65x에서 GPIO50/GPIO49인 이유는 initUsbGpio() 함수 위
//		   주석 참고 — F2838x용 GPIO121/120과 물리적 커넥터 위치는 같지만 칩이 달라
//		   실제 GPIO 번호가 다릅니다.)
//		   USB 마우스는 (5)번 회로 Top면의 4핀 표준 USB 커넥터(Type-A)에 직접
//		   연결하거나, USB 허브를 통해 연결하세요.
//		>> LED: 개발보드 A Side 핀-헤더 65, 67, 69, 71, 73, 75, 77, 79, 81, 83, 85, 87,
//		   89, 91, 93, 95번(홀수, GPIO0~GPIO15에 순서대로 1:1 대응)을 (8) 범용 LED 16개
//		   블록의 입력 16핀에 순서대로 연결하세요 — DriverLib/FreeRTOS 예제와 배선이 동일합니다.
//
// 예제 폴더의 CCS Expressions 창에 아래 변수를 등록해두면 동작을 실시간으로 확인할 수 있습니다.
// --> usbHostState(0=미연결, 1=마우스 초기화 중, 2=마우스 연결됨, 3=미지원 장치, 4=전원 오류)
// --> ledIndex(0~15, 현재 점등된 LED 위치), mouseCursorX(누적 X 이동량, 디버깅용)
// --> mouseButtons(bit0=좌클릭, bit1=우클릭, bit2=휠클릭)
//
// 본 예제는 28X 칩 MMR(Memory Mapped Register)을 직접 조작하는 TI의 Bit-Field Approach로
// GPIO/클럭/인터럽트를 초기화합니다 — 애플리케이션 코드(이 파일)는 Driverlib API를 한 줄도
// 호출하지 않습니다. 다만 USB 프로토콜 스택 자체는 TI USB Host Library(usblib.lib, 사전
// 빌드된 라이브러리)를 그대로 사용하며, usblib.lib이 내부적으로 DriverLib의 인터럽트/
// 시스템클럭/USB 레지스터 저수준 함수(Interrupt_enable, SysCtl_delay, USBHostMode 등)를
// 호출하도록 미리 빌드되어 있어, 그 심볼을 채워주기 위해 `driverlib.lib`을 링크에만
// 포함시킵니다(우리 애플리케이션 코드에서 직접 호출하는 곳은 없습니다 — usblib.lib
// 자체의 숨은 의존성입니다). usblib API 호출부(USBHCDMain, USBHMouseOpen 등)는
// DriverLib 버전과 100% 동일하며, 차이는 GPIO 핀먹스/AUXPLL 클럭/인터럽트 벡터 등록을
// classic 레지스터로 직접 쓰는지 여부뿐입니다. 같은 동작을 DriverLib API로 구현한
// 5_USB_HostMouse_Driverlib.c, FreeRTOS 태스크로 구현한 5_USB_HostMouse_Freertos.c와
// 함께 세 가지 방식을 비교해 보실 수 있습니다.
//
// b. USB 60MHz 클럭: F28P65x의 USB 컨트롤러는 SYSCLK가 아니라 별도의 AUXPLL 60MHz 클럭이
//    필요합니다. `initUsbAuxClock()`이 보드의 25MHz BAW 오실레이터 기준으로
//    AUXPLLCLK 120MHz(25MHz × 48 ÷ (2×5)) → 실리콘 내부 고정 /2 분주 → 60MHz를 만듭니다
//    (DriverLib 버전의 `SysCtl_setAuxClock(DEVICE_AUXSETCLOCK_CONFIG_USB)`와 동일한
//    레지스터 값을 만드는 축약된 클래식 시퀀스입니다 — DCC 주파수 검증/재시도 로직은
//    생략했습니다).
//
// c. 마우스 좌우 이동 -> LED 위치 매핑: `MouseCallback()`의 `USBH_EVENT_HID_MS_X` 이벤트에서
//    마우스가 보고하는 8비트 부호있는 상대 이동량을 누적하다가, `MOUSE_STEP_THRESHOLD`
//    (기본 12카운트)를 넘을 때마다 `ledIndex`를 한 칸씩 좌/우로 옮기고 `updateLedDisplay()`로
//    실제 GPIO를 갱신합니다. 양쪽 끝(0, 15)에서는 더 이상 넘어가지 않고 멈춥니다.
//
//************************************************************************************************************************************************************************


// 헤더 파일들
#include "f28x_project.h"		// TI 제공 칩-지원 헤더 통합 Include 용 헤더파일 (bit-field)
#include <stdint.h>				// usblib.h가 uint8_t/uint32_t 등 stdint 타입을 직접 사용함

//
// C28x는 하드웨어에 8비트 타입이 없어 TI stdint.h가 이 아키텍처에서는 uint8_t/int8_t를
// 아예 정의하지 않습니다. DriverLib의 device/driverlib/inc/hw_types.h가 쓰는 것과
// 동일한 호환 shim(uint8_t = uint16_t)을 그대로 재사용합니다.
//
typedef uint16_t uint8_t;

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
// (ledIndex=0일 때 GPIO15, ledIndex=15일 때 GPIO0이 켜지도록). initLedGpio()의 핀
// 설정 순서에는 영향 없고(어차피 GPIO0~15 전부 설정), updateLedDisplay()가 이 배열을
// 실제로 참조하도록 함께 바꿔야 효과가 있음(아래 updateLedDisplay() 참고).
static const Uint16 ledGpio[LED_COUNT] =
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

// USB 마우스 연결 상태 머신
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
void initUsbGpio(void);
void initUsbAuxClock(void);
void USBHCDEvents(void *pvData);
void ModeCallback(uint32_t ui32Index, tUSBMode eMode);
void MouseCallback(tUSBHMouse *psMsInstance, uint32_t ui32Event,
                    uint32_t ui32MsgParam, void *pvMsgData);
interrupt void usbHostISR(void);


// 전역 변수 선언 (CCS Expressions/Watch 창에서 실시간으로 값을 확인할 수 있는 전역 변수)
volatile Uint16	usbHostState = 0U;	// g_eUSBState의 숫자 사본(enum이 Expressions 창에 안 보일 때 대비)
volatile Uint16	ledIndex = 8U;		// 현재 점등된 LED 위치(0~15), 시작 위치는 중앙 부근
volatile int32_t	mouseCursorX = 0;	// 누적 마우스 X 이동량(디버깅/표시용, 범위 제한 없음)
volatile Uint32	mouseButtons = 0UL;	// bit0=좌클릭, bit1=우클릭, bit2=휠클릭

static int32_t motionAccumX = 0;		// LED_STEP 단위로 소비되는 X 누적 이동량 (내부용)


// 메인 함수
void main(void)
{

//	1. 전역 인터럽트 스위치 OFF, CPU 인터럽트 벡터 비-활성화 및 플래그(Flag) 비트 클리어
	DINT;
	IER = 0x0000;
	IFR = 0x0000;


//	2. 시스템 초기화 - InitSysCtrl( ) 함수 호출 (f28p65x_sysctrl.c)
	InitSysCtrl();

//	* 범용 입출력 포트(GPIO) 설정 - InitGpio( ) 함수 호출
	InitGpio();


//	3. 주변회로 인터럽트 확장회로 초기화 - InitPieCtrl( ) 함수 호출 (f28p65x_piectrl.c)
	InitPieCtrl();


//	4. 주변회로 인터럽트 벡터 확장 및 복사 실행 - InitPieVectTable( ) 함수 호출 (f28p65x_pievect.c)
	InitPieVectTable();


//	5. 인터럽트 벡터와 인터럽트 서비스 루틴 연결
	EALLOW;
	PieVectTable.USBA_INT = &usbHostISR;
	EDIS;


//	6. 주변회로 초기화 - LED16 GPIO 출력 + USB0 호스트 모드
	initLedGpio();
	updateLedDisplay();

	// USB 컨트롤러용 60MHz AUXPLL 클럭 설정 (25MHz BAW 오실레이터 기준)
	initUsbAuxClock();

	// USB0DM/DP/VBUS/ID 핀 활성화
	initUsbGpio();

	PieCtrlRegs.PIEIER9.bit.INTx15 = 1;	// PIE 그룹9.15(USBA_INT) 활성화
	IER |= M_INT9;						// CPU 인터럽트 9번 활성화


//	7. 전역 변수 및 S/W 모듈 초기화
	g_eUSBState = STATE_NO_DEVICE;
	usbHostState = (Uint16)STATE_NO_DEVICE;
	mouseCursorX = 0;
	mouseButtons = 0UL;
	motionAccumX = 0;


//	8. 실시간 디버깅 활성화, 전역 인터럽트 스위치 ON
	ERTM;	// Debug Enable Mask 비트 설정
	EINT;	// 전역 인터럽트 스위치 ON (/INTM ON)


//	9. USB 호스트 스택 초기화
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
				break;
		}

		usbHostState = (Uint16)g_eUSBState;
	}
}

//	11. 인터럽트 서비스 루틴 및 기타 함수들

//
// initLedGpio - GPIO0~15를 디지털 출력으로 설정합니다.
//
void initLedGpio(void)
{
    Uint16 i;

    EALLOW;
    for(i = 0U; i < LED_COUNT; i++)
    {
        GPIO_SetupPinMux(ledGpio[i], GPIO_MUX_CPU1, 0);		// Mux=0: GPIO 기능
        GPIO_SetupPinOptions(ledGpio[i], GPIO_OUTPUT, GPIO_PUSHPULL);
    }
    EDIS;
}

//
// updateLedDisplay - 16개 LED를 모두 끈 뒤 ledIndex에 해당하는 LED 하나만 켭니다.
// GPIO0~15는 모두 Port A에 속하므로 GPACLEAR/GPASET 32비트 마스크 쓰기로 한 번에 처리합니다.
//
void updateLedDisplay(void)
{
    GpioDataRegs.GPACLEAR.all = 0x0000FFFFUL;			// GPIO0~15 모두 끄기
    GpioDataRegs.GPASET.all = (1UL << ledGpio[ledIndex]);	// ledGpio[]로 매핑된 실제 GPIO만 켜기(좌우 반전 반영)
}

//
// initUsbGpio - USB0DM(GPIO42)/USB0DP(GPIO43)를 아날로그 모드로, VBUS 감지(GPIO46)/
// ID(GPIO47)를 입력으로 설정합니다 (DriverLib 버전의 USBGPIOEnable()과 동일).
//
// [2026-09-28 수정] EPEN/PFLT 미설정으로 USB 포트 전원이 켜지지 않아 마우스가 열거되지
// 않는 문제를 고쳤습니다. 근거:
//  - 개발보드 매뉴얼(Datasheet_TMS320F28X_EVM_V2_V140.pdf) 12장 "(5) USB 통신 회로"는
//    TMS320F2837x/F2838x용 ControlCARD 구성을 그대로 따른다고 명시되어 있고, F2838x
//    모듈 기준 EPEN=GPIO121(B-Side 45번 핀)/PFLT=GPIO120(B-Side 47번 핀)입니다(매뉴얼
//    9.2.2절 표 9-4).
//  - F28P65x는 GPIO121이 이 패키지(256ZEJ)에 존재하지 않는 예약 핀입니다
//    (f28p65x_gpio.h의 GPDDIR_BITS 25번 비트가 "rsvd3"로 정의됨 — 실측 확인).
//  - 같은 물리적 커넥터 위치(B-Side 45/47번 핀)에 F28P65x 모듈을 꽂으면 실제로는
//    GPIO50(45번 핀)/GPIO49(47번 핀)가 연결됩니다(SYNCWORKS_F28P65X_MODULE.syscfg.json
//    실측 대조로 확인). 즉 F28P65x에서 EPEN=GPIO50, PFLT=GPIO49입니다.
//  - TI 공식 F28P65x 호스트 예제(C2000Ware driverlib/f28p65x/examples/c28x/usb/usb_hal.c
//    ::USBGPIOEnable())도 EPEN을 전용 하드웨어 핀이 아니라 일반 GPIO 출력으로 다루며
//    Init 시 즉시 High로 구동합니다 — 이 신호가 보드의 USB 포트 VBUS 전원 스위치를
//    켜는 역할이라, 안 하면 마우스에 전원이 안 가 열거 자체가 시작되지 않습니다.
//
void initUsbGpio(void)
{
    EALLOW;

    GpioCtrlRegs.GPBAMSEL.bit.GPIO42 = 1;	// USB0DM 아날로그 모드
    GpioCtrlRegs.GPBAMSEL.bit.GPIO43 = 1;	// USB0DP 아날로그 모드

    GpioCtrlRegs.GPBDIR.bit.GPIO46 = 0;	// VBUS 감지 - 입력
    GpioCtrlRegs.GPBDIR.bit.GPIO47 = 0;	// ID - 입력

    GpioCtrlRegs.GPBDIR.bit.GPIO49 = 0;	// PFLT(F28P65x 실제 위치, B-Side 47번 핀) - 입력(전원 오류 감지)

    GpioCtrlRegs.GPBDIR.bit.GPIO50 = 1;	// EPEN(F28P65x 실제 위치, B-Side 45번 핀) - 출력
    GpioDataRegs.GPBSET.bit.GPIO50 = 1;	// High 구동 -> (5) USB 회로의 VBUS 전원 스위치 On

    //
    // TI 공식 예제 주석: "F28P65x Control Card에서 호스트 모드를 쓰려면 GPIO44를
    // High로 당겨야 한다." 이 보드가 TI ControlCARD와 동일한 회로인지는 확인하지
    // 못했지만, GPIO44는 이 보드의 다른 회로블록에 배정되지 않은 일반 GPIO라
    // (A/B Side 핀-헤더 정의 대조로 확인, B-Side 53번 핀) 안전하게 설정 가능하고,
    // 호스트 모드 활성화에 필요할 수 있어 함께 설정합니다.
    GpioCtrlRegs.GPBDIR.bit.GPIO44 = 1;
    GpioDataRegs.GPBSET.bit.GPIO44 = 1;

    EDIS;
}

//
// initUsbAuxClock - AUXPLL을 이용해 USB 컨트롤러용 60MHz 클럭을 만듭니다. 보드의
// 25MHz BAW 오실레이터를 기준으로 AUXPLLRAWCLK = 25MHz * IMULT(48) / (REFDIV(2) *
// ODIV(5)) = 120MHz를 만든 뒤, USB 클럭 경로의 고정(실리콘 내부) /2 분주를 거쳐
// 60MHz가 됩니다. REFDIV/ODIV 레지스터 필드에는 "분주값-1"을 씁니다
// (DriverLib의 SYSCTL_REFDIV()/SYSCTL_ODIV() 매크로와 동일한 관례).
//
void initUsbAuxClock(void)
{
    EALLOW;

    // AUXPLL을 끄고 소스를 바꾸기 전에 필요한 최소 대기시간 확보
    ClkCfgRegs.AUXPLLCTL1.bit.PLLCLKEN = 0;
    DELAY_US(10);

    ClkCfgRegs.AUXPLLCTL1.bit.PLLEN = 0;
    DELAY_US(10);

    // 오실레이터 소스: XTAL (보드의 25MHz BAW 오실레이터)
    ClkCfgRegs.CLKSRCCTL2.bit.AUXOSCCLKSRCSEL = 1;		// 1 = XTAL
    DELAY_US(10);

    // AUXPLL 배율/분주 설정
    ClkCfgRegs.AUXPLLMULT.bit.IMULT = 48;
    ClkCfgRegs.AUXPLLMULT.bit.REFDIV = 1;		// 필드값 = 분주값(2) - 1
    ClkCfgRegs.AUXPLLMULT.bit.ODIV = 4;		// 필드값 = 분주값(5) - 1

    // AUXPLL 활성화 후 Lock 대기
    ClkCfgRegs.AUXPLLCTL1.bit.PLLEN = 1;
    while(ClkCfgRegs.AUXPLLSTS.bit.LOCKS != 1)
    {
    }

    // AUXPLLCLK을 AUXPLL 출력으로 전환 -> USB 컨트롤러에 60MHz 공급
    ClkCfgRegs.AUXPLLCTL1.bit.PLLCLKEN = 1;
    DELAY_US(5);

    EDIS;
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
interrupt void usbHostISR(void)
{
    USB0HostIntHandler();
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;
}

//
// __error__ - usblib.lib(그리고 usblib이 내부적으로 의존하는 DriverLib usb.c/sysctl.c
// 저수준 모듈)이 참조하는 ASSERT 실패 핸들러입니다. TI 헤더 주석에 "application's
// responsibility to define the __error__ function"이라고 명시되어 있어 직접 정의합니다.
// 실제 하드웨어에서는 정상 경로에서 호출되지 않아야 하며, 호출되면 무한루프로 멈춰
// 디버거로 원인을 추적할 수 있게 합니다.
//
void __error__(const char *pcFilename, uint32_t ui32Line)
{
    (void)pcFilename;
    (void)ui32Line;
    for(;;)
    {
    }
}

// 파일 끝.
