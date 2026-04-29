#include "usb_hid_input_queue.h"

CUsbHidInputQueue::CUsbHidInputQueue(void)
:	m_WriteSeq(0u),
	m_UiReadSeq(0u),
	m_RuntimeReadSeq(0u),
	m_DroppedUi(0u),
	m_DroppedRuntime(0u),
	m_HighWatermark(0u),
	m_Reports{}
{
}

void CUsbHidInputQueue::Reset(void)
{
	m_WriteSeq = 0u;
	m_UiReadSeq = 0u;
	m_RuntimeReadSeq = 0u;
	m_DroppedUi = 0u;
	m_DroppedRuntime = 0u;
	m_HighWatermark = 0u;
	for (unsigned i = 0u; i < kCapacity; ++i)
	{
		m_Reports[i].modifiers = 0u;
		for (unsigned j = 0u; j < 6u; ++j)
		{
			m_Reports[i].raw_keys[j] = 0u;
		}
	}
}

unsigned CUsbHidInputQueue::OldestSeq(void) const
{
	const unsigned write_seq = m_WriteSeq;
	return (write_seq > kCapacity) ? (write_seq - kCapacity) : 0u;
}

void CUsbHidInputQueue::PushKeyboardReport(unsigned char modifiers, const unsigned char raw_keys[6])
{
	const unsigned write_seq = m_WriteSeq;
	const unsigned slot = write_seq % kCapacity;

	m_Reports[slot].modifiers = modifiers;
	for (unsigned i = 0u; i < 6u; ++i)
	{
		m_Reports[slot].raw_keys[i] = (raw_keys != 0) ? raw_keys[i] : 0u;
	}

	m_WriteSeq = write_seq + 1u;

	const unsigned oldest_seq = OldestSeq();
	if (m_UiReadSeq < oldest_seq)
	{
		m_DroppedUi += oldest_seq - m_UiReadSeq;
		m_UiReadSeq = oldest_seq;
	}
	if (m_RuntimeReadSeq < oldest_seq)
	{
		m_DroppedRuntime += oldest_seq - m_RuntimeReadSeq;
		m_RuntimeReadSeq = oldest_seq;
	}

	const unsigned pending_ui = m_WriteSeq - m_UiReadSeq;
	const unsigned pending_runtime = m_WriteSeq - m_RuntimeReadSeq;
	const unsigned high_water = pending_ui > pending_runtime ? pending_ui : pending_runtime;
	if (high_water > m_HighWatermark)
	{
		m_HighWatermark = high_water;
	}
}

boolean CUsbHidInputQueue::DrainCursor(volatile unsigned *cursor,
				       volatile unsigned *dropped_counter,
				       unsigned char *modifiers,
				       unsigned char raw_keys[6])
{
	if (cursor == 0 || dropped_counter == 0 || modifiers == 0 || raw_keys == 0)
	{
		return FALSE;
	}

	const unsigned write_seq = m_WriteSeq;
	unsigned read_seq = *cursor;
	const unsigned oldest_seq = OldestSeq();
	if (read_seq < oldest_seq)
	{
		*dropped_counter += oldest_seq - read_seq;
		read_seq = oldest_seq;
	}

	if (read_seq >= write_seq)
	{
		*cursor = read_seq;
		return FALSE;
	}

	const unsigned slot = read_seq % kCapacity;
	*modifiers = m_Reports[slot].modifiers;
	for (unsigned i = 0u; i < 6u; ++i)
	{
		raw_keys[i] = m_Reports[slot].raw_keys[i];
	}

	*cursor = read_seq + 1u;
	return TRUE;
}

boolean CUsbHidInputQueue::DrainUiReport(unsigned char *modifiers, unsigned char raw_keys[6])
{
	return DrainCursor(&m_UiReadSeq, &m_DroppedUi, modifiers, raw_keys);
}

boolean CUsbHidInputQueue::DrainRuntimeReport(unsigned char *modifiers, unsigned char raw_keys[6])
{
	return DrainCursor(&m_RuntimeReadSeq, &m_DroppedRuntime, modifiers, raw_keys);
}

void CUsbHidInputQueue::GetStats(TStats *stats) const
{
	if (stats == 0)
	{
		return;
	}

	stats->queued_ui = m_WriteSeq - m_UiReadSeq;
	stats->queued_runtime = m_WriteSeq - m_RuntimeReadSeq;
	stats->dropped_ui = m_DroppedUi;
	stats->dropped_runtime = m_DroppedRuntime;
	stats->high_watermark = m_HighWatermark;
}
