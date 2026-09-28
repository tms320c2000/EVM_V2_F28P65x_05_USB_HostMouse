//##############################################################################
//
// FILE:   usb_hal.c
//
// TITLE:  Wrapper for interrupt functions and USB support pins.
//
//##############################################################################
//
//
// 
// C2000Ware v26.00.00.00
//
// Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//##############################################################################

//
// Include Files.
//
#include "usb_hal.h"

//******************************************************************************
//
//! \addtogroup c2000_specific
//! @{
//
//******************************************************************************
//******************************************************************************
//
//! Enables USB related GPIOs to perform their USB function.
//
//******************************************************************************
void USBGPIOEnable(void)
{

    //
    // Set the USB DM and DP.
    //
    GPIO_setControllerCore(42, GPIO_CORE_CPU1);
    GPIO_setAnalogMode(42, GPIO_ANALOG_ENABLED);
    GPIO_setControllerCore(43, GPIO_CORE_CPU1);
    GPIO_setAnalogMode(43, GPIO_ANALOG_ENABLED);

    //
    // Set the direction for VBUS.
    //
    GPIO_setControllerCore(46, GPIO_CORE_CPU1);
    GPIO_setDirectionMode(46, GPIO_DIR_MODE_IN);

    //
    // Set the direction for ID.
    //
    GPIO_setControllerCore(47, GPIO_CORE_CPU1);
    GPIO_setDirectionMode(47, GPIO_DIR_MODE_IN);

    //
    // [2026-09-28 수정] 이 파일은 TI 공식 C2000Ware 예제(driverlib/f28p65x/examples/
    // c28x/usb/usb_hal.c)를 그대로 복사한 것인데, 원본의 GPIO120/121(PFLT/EPEN)은
    // TI 자사 F28P65x Control Card 기준입니다. 이 SyncWorks 개발보드 V2의 (5) USB
    // 통신 회로는 원래 F2837x/F2838x용으로 설계돼 있어서(매뉴얼 12장), 물리적
    // 커넥터 위치(B-Side 45/47번 핀)는 F2838x 기준으로 GPIO121/120이지만, F28P65x
    // 모듈에서는 GPIO121이 이 패키지(256ZEJ)에 존재하지 않는 예약 핀이라 실제로는
    // GPIO50/GPIO49가 옵니다(SYNCWORKS_F28P65X_MODULE.syscfg.json 실측 대조로 확인 —
    // 5_USB_HostMouse_Bitfield.c의 initUsbGpio() 주석과 같은 근거). 원본 그대로
    // GPIO121/120을 쓰면 존재하지 않는 예약 비트에 쓰는 셈이라 전기적으로 아무 데도
    // 연결되지 않아, USB 포트 전원(VBUS)이 안 켜지고 마우스가 열거되지 않습니다.
    //
    GPIO_setControllerCore(49, GPIO_CORE_CPU1);
    GPIO_setDirectionMode(49, GPIO_DIR_MODE_IN);	// PFLT(F28P65x 실제 위치, B-Side 47번 핀)

    GPIO_setControllerCore(50, GPIO_CORE_CPU1);
	GPIO_setDirectionMode(50, GPIO_DIR_MODE_OUT);	// EPEN(F28P65x 실제 위치, B-Side 45번 핀)
	GPIO_writePin(50, 1);	// High 구동 -> (5) USB 회로의 VBUS 전원 스위치 On

	// To use the host mode capabilities, GPIO44 must be pulled high (TI 공식 주석).
	// 원본은 HOST_MODE 매크로로 감싸져 있었는데 이 프로젝트 어디에도 그 매크로가
	// 정의돼 있지 않아(.projectspec에도 없음) 데드코드였습니다 — 이 예제 자체가
	// 호스트 전용이라 무조건 실행되도록 가드를 제거했습니다.
    GPIO_setControllerCore(44, GPIO_CORE_CPU1);
    GPIO_setDirectionMode(44, GPIO_DIR_MODE_OUT);
    GPIO_writePin(44, 1);
}

//******************************************************************************
//
//! Configure the CPU Timer.
//
//******************************************************************************
void CPUTimerInit(void)
{
    //
    // Initialize timer period to maximum.
    //
    CPUTimer_setPeriod(CPUTIMER0_BASE,0xFFFFFFFF);

    //
    // Initialize Pre-scale counter to divide by 1.
    //
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0U);

    //
    // Make sure timer is stopped.
    //
    CPUTimer_stopTimer(CPUTIMER0_BASE);

    //
    // Reload all counter register with period value:
    //
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
}
//******************************************************************************
//
//! Wrapper function to implement mS based delay for USB functions
//
//******************************************************************************
void USBDelay(uint32_t ui32Delay)
{
    DEVICE_DELAY_US(ui32Delay*1000);
}


//******************************************************************************
//
//! Dual mode interrupt service routine wrapper to make ISR compatible with
//! C2000 PIE controller.
//
//******************************************************************************
__interrupt void
f28x_USB0DualModeIntHandler(void)
{
    USB0DualModeIntHandler();
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

//******************************************************************************
//
//! Dual mode interrupt service routine wrapper to make ISR compatible with
//! C2000 PIE controller.
//
//******************************************************************************
__interrupt void
f28x_USB0OTGModeIntHandler(void)
{
    USB0OTGModeIntHandler();
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

//******************************************************************************
//
// Close the c2000_specific Doxygen group.
//! @}
//
//******************************************************************************
