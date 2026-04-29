#ifndef SMSBARE_USB_HID_GAMEPAD_H
#define SMSBARE_USB_HID_GAMEPAD_H

#include <circle/device.h>
#include <circle/devicenameservice.h>
#include "debug/debug_runtime_gate.h"
#include <circle/types.h>
#include <circle/usb/usbgamepad.h>

class CUsbHidGamepad
{
public:
	enum TMapSlot
	{
		MapSlotUp = 0,
		MapSlotDown,
		MapSlotLeft,
		MapSlotRight,
		MapSlotButton1,
		MapSlotButton2,
		MapSlotCount
	};

	enum TControlType
	{
		ControlTypeNone = 0u,
		ControlTypeKnownButton = 1u,
		ControlTypeRawButton = 2u,
		ControlTypeHat = 3u,
		ControlTypeAxisNegative = 4u,
		ControlTypeAxisPositive = 5u
	};

	enum TMenuBit
	{
		MenuBitUp = 1u << 0,
		MenuBitDown = 1u << 1,
		MenuBitLeft = 1u << 2,
		MenuBitRight = 1u << 3,
		MenuBitSelect = 1u << 4,
		MenuBitBack = 1u << 5,
		MenuBitPause = 1u << 6
	};

	CUsbHidGamepad(void);

	static u16 ClampControlCode(u16 code);
	static void FormatControlCode(u16 code, char *dst, unsigned dst_size);

	boolean AttachIfAvailable(CDeviceNameService &device_name_service, CLogger &logger);
	boolean IsAttached(void) const;
	boolean HasKnownMapping(void) const;
	boolean ConsumeRemovedEvent(void);
	u8 GetBridgeBits(void) const;
	u8 GetMenuBits(void) const;
	u16 GetActiveControlCode(void) const;
	u16 GetMappingCode(unsigned slot) const;
	void GetMappingCodes(u16 *codes, unsigned count) const;
	void SetMappingCodes(const u16 *codes, unsigned count, boolean user_defined);

private:
	static void GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *state);
	static void GamePadRemovedHandler(CDevice *device, void *context);

	static u16 EncodeControlCode(unsigned type, unsigned payload);
	static unsigned DecodeControlType(u16 code);
	static unsigned DecodeControlPayload(u16 code);

	void UpdateState(const TGamePadState *state);
	boolean SnapshotState(TGamePadState *state, unsigned *properties) const;
	void LoadDefaultMappings(const TGamePadState *state, unsigned properties);
	u16 DetectActiveControlCode(const TGamePadState *state, unsigned properties) const;
	boolean IsControlActive(u16 code, const TGamePadState *state, unsigned properties) const;

private:
	static CUsbHidGamepad *s_Instance;

	CUSBGamePadDevice *m_GamePad;
	CDevice::TRegistrationHandle m_GamePadRemovedHandle;
	volatile boolean m_RemovedEvent;
	volatile unsigned m_StateSeq;
	volatile unsigned m_Properties;
	TGamePadState m_State;
	u16 m_MappingCodes[MapSlotCount];
	boolean m_HasUserMapping;
};

#endif
