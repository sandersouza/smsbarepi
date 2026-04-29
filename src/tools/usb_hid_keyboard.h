#ifndef SMSBARE_USB_HID_KEYBOARD_H
#define SMSBARE_USB_HID_KEYBOARD_H

#include <circle/devicenameservice.h>
#include "debug/debug_runtime_gate.h"
#include <circle/types.h>
#include <circle/usb/usbkeyboard.h>
#include "input/usb_hid_input_queue.h"

class CUsbHidKeyboard
{
public:
	CUsbHidKeyboard(void);

	boolean AttachIfAvailable(CDeviceNameService &device_name_service,
				  CLogger &logger,
				  TKeyPressedHandler *key_pressed_handler);
	boolean IsAttached(void) const;

	void UpdateLEDs(void);

	boolean ConsumeLatest(unsigned char *modifiers, unsigned char raw_keys[6]);
	boolean ConsumeForRuntime(unsigned char *modifiers, unsigned char raw_keys[6]);
	boolean ConsumeRemovedEvent(void);

private:
	static void KeyStatusRawHandler(unsigned char modifiers,
					const unsigned char raw_keys[6],
					void *context);
	static void KeyboardRemovedHandler(CDevice *device, void *context);

private:
	CUSBKeyboardDevice *m_Keyboard;
	void *m_KeyboardRemovedHandle;
	CUsbHidInputQueue m_InputQueue;
	volatile boolean m_RemovedEvent;
};

#endif
