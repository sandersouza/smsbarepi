#ifndef SMSBARE_USB_HID_INPUT_QUEUE_H
#define SMSBARE_USB_HID_INPUT_QUEUE_H

#include <circle/types.h>

class CUsbHidInputQueue
{
public:
	struct TKeyboardReport
	{
		unsigned char modifiers;
		unsigned char raw_keys[6];
	};

	struct TStats
	{
		unsigned queued_ui;
		unsigned queued_runtime;
		unsigned dropped_ui;
		unsigned dropped_runtime;
		unsigned high_watermark;
	};

	CUsbHidInputQueue(void);

	void Reset(void);
	void PushKeyboardReport(unsigned char modifiers, const unsigned char raw_keys[6]);
	boolean DrainUiReport(unsigned char *modifiers, unsigned char raw_keys[6]);
	boolean DrainRuntimeReport(unsigned char *modifiers, unsigned char raw_keys[6]);
	void GetStats(TStats *stats) const;

private:
	static const unsigned kCapacity = 32u;

	boolean DrainCursor(volatile unsigned *cursor,
			    volatile unsigned *dropped_counter,
			    unsigned char *modifiers,
			    unsigned char raw_keys[6]);
	unsigned OldestSeq(void) const;

private:
	volatile unsigned m_WriteSeq;
	volatile unsigned m_UiReadSeq;
	volatile unsigned m_RuntimeReadSeq;
	volatile unsigned m_DroppedUi;
	volatile unsigned m_DroppedRuntime;
	volatile unsigned m_HighWatermark;
	volatile TKeyboardReport m_Reports[kCapacity];
};

#endif
