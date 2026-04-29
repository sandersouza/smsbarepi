#include "usb_hid_keyboard.h"

static const char FromUsbHidKeyboard[] = "usb-hid-kbd";

CUsbHidKeyboard::CUsbHidKeyboard(void)
:	m_Keyboard(0),
	m_KeyboardRemovedHandle(0),
	m_InputQueue(),
	m_RemovedEvent(FALSE)
{
}

boolean CUsbHidKeyboard::AttachIfAvailable(CDeviceNameService &device_name_service,
					    CLogger &logger,
					    TKeyPressedHandler *key_pressed_handler)
{
	if (m_Keyboard != 0)
	{
		return FALSE;
	}

	CUSBKeyboardDevice *keyboard =
		(CUSBKeyboardDevice *) device_name_service.GetDevice("ukbd1", FALSE);
	if (keyboard == 0)
	{
		return FALSE;
	}

	m_Keyboard = keyboard;
	m_KeyboardRemovedHandle = m_Keyboard->RegisterRemovedHandler(KeyboardRemovedHandler, this);
	m_Keyboard->RegisterKeyStatusHandlerRaw(KeyStatusRawHandler, TRUE, this);
	if (key_pressed_handler != 0)
	{
		m_Keyboard->RegisterKeyPressedHandler(key_pressed_handler);
	}

	logger.Write(FromUsbHidKeyboard, LogNotice, "USB keyboard attached");
	return TRUE;
}

boolean CUsbHidKeyboard::IsAttached(void) const
{
	return m_Keyboard != 0 ? TRUE : FALSE;
}

void CUsbHidKeyboard::UpdateLEDs(void)
{
	if (m_Keyboard != 0)
	{
		m_Keyboard->UpdateLEDs();
	}
}

boolean CUsbHidKeyboard::ConsumeLatest(unsigned char *modifiers, unsigned char raw_keys[6])
{
	return m_InputQueue.DrainUiReport(modifiers, raw_keys);
}

boolean CUsbHidKeyboard::ConsumeForRuntime(unsigned char *modifiers, unsigned char raw_keys[6])
{
	return m_InputQueue.DrainRuntimeReport(modifiers, raw_keys);
}

boolean CUsbHidKeyboard::ConsumeRemovedEvent(void)
{
	if (!m_RemovedEvent)
	{
		return FALSE;
	}

	m_RemovedEvent = FALSE;
	return TRUE;
}

void CUsbHidKeyboard::KeyboardRemovedHandler(CDevice *device, void *context)
{
	(void) device;
	CUsbHidKeyboard *self = (CUsbHidKeyboard *) context;
	if (self == 0)
	{
		return;
	}

	self->m_Keyboard = 0;
	self->m_KeyboardRemovedHandle = 0;
	self->m_InputQueue.Reset();
	self->m_RemovedEvent = TRUE;
}

void CUsbHidKeyboard::KeyStatusRawHandler(unsigned char modifiers,
					  const unsigned char raw_keys[6],
					  void *context)
{
	if (context == 0 || raw_keys == 0)
	{
		return;
	}

	CUsbHidKeyboard *self = (CUsbHidKeyboard *) context;
	self->m_InputQueue.PushKeyboardReport(modifiers, raw_keys);
}
