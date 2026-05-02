#include "kernel.h"
#include "keyboard_input.h"
#include "combo_locale.h"
#include "viewport_screenshot.h"
#include "backend/runtime/runtime.h"

#include <circle/bcmpropertytags.h>
#include <circle/machineinfo.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/startup.h>
#include <circle/string.h>
#include <circle/synchronize.h>
#include <circle/util.h>
#include <assert.h>

#ifndef SMSBARE_V9990_UART_TRACE
#define SMSBARE_V9990_UART_TRACE 0
#endif

extern "C" unsigned long long smsbare_monotonic_time_us(void)
{
	return CTimer::GetClockTicks64();
}

#if SMSBARE_ENABLE_DEBUG_OVERLAY
static CSerialDevice *g_backend_uart_trace_serial = 0;
static char g_backend_uart_trace_queue[16384];
static unsigned g_backend_uart_trace_queue_len = 0u;
static unsigned g_backend_uart_trace_dropped_bytes = 0u;
static boolean g_backend_uart_trace_drop_notice_pending = FALSE;
static unsigned g_backend_gfx9000_kernel_trace_remaining = 0u;
static unsigned g_backend_gfx9000_present_snapshot_budget = 0u;
static const unsigned kBackendUartTraceFlushChunkBytes = 128u;
static const unsigned kBackendUartTraceFlushBudgetBytes = 256u;
static const unsigned kBackendUartTraceTraceBudgetBytes = 2048u;
extern "C" void smsbare_uart_write(const char *text);

static void BackendUartTraceWriteChar(char c)
{
	char text[2];
	text[0] = c;
	text[1] = '\0';
	smsbare_uart_write(text);
}

static char BackendGfx9000KernelTraceCompactMarker(const char *text)
{
	if (text == 0)
	{
		return '\0';
	}
	if (strcmp(text, "G9K: step gate") == 0) return 'G';
	if (strcmp(text, "G9K: input key end") == 0) return 'K';
	if (strcmp(text, "G9K: input joy end") == 0) return 'J';
	if (strcmp(text, "G9K: step call") == 0) return 'j';
	if (strcmp(text, "G9K: step ret") == 0) return 'R';
	if (strcmp(text, "G9K: status begin") == 0) return 's';
	if (strcmp(text, "G9K: status end") == 0) return 'S';
	if (strcmp(text, "G9K: sync begin") == 0) return 'y';
	if (strcmp(text, "G9K: sync end") == 0) return 'Y';
	if (strcmp(text, "G9K: flush begin") == 0) return 'u';
	if (strcmp(text, "G9K: flush end") == 0) return 'U';
	if (strcmp(text, "G9K: tick end") == 0) return 'T';
	if (strcmp(text, "G9K: blit begin") == 0) return 'b';
	if (strcmp(text, "G9K: blit end") == 0) return 'B';
	if (strcmp(text, "G9K: present begin") == 0) return 'm';
	if (strcmp(text, "G9K: present end") == 0) return 'M';
	if (strcmp(text, "G9K: yield begin") == 0) return 'w';
	if (strcmp(text, "G9K: yield end") == 0) return 'W';
	if (strcmp(text, "G9K: post-yield consume begin") == 0) return 'c';
	if (strcmp(text, "G9K: post-yield consume end") == 0) return 'C';
	if (strcmp(text, "G9K: post-yield flush begin") == 0) return 'f';
	if (strcmp(text, "G9K: post-yield flush end") == 0) return 'F';
	if (strcmp(text, "G9K: loop top") == 0) return 'L';
	if (strcmp(text, "G9K: top flush begin") == 0) return 'h';
	if (strcmp(text, "G9K: top flush end") == 0) return 'H';
	if (strcmp(text, "G9K: usb begin") == 0) return 'x';
	if (strcmp(text, "G9K: usb end") == 0) return 'X';
	if (strcmp(text, "G9K: deferred begin") == 0) return 'd';
	if (strcmp(text, "G9K: deferred end") == 0) return 'D';
	if (strcmp(text, "G9K: pace begin") == 0) return 'q';
	if (strcmp(text, "G9K: pace end") == 0) return 'Q';
	return '\0';
}

extern "C" void smsbare_debug_rearm_kernel_gfx9000_trace(unsigned frames)
{
#if SMSBARE_V9990_UART_TRACE
	if (frames > g_backend_gfx9000_kernel_trace_remaining)
	{
		g_backend_gfx9000_kernel_trace_remaining = frames;
	}
#else
	(void) frames;
#endif
}

extern "C" void smsbare_debug_note_gfx9000_activation(void)
{
#if SMSBARE_V9990_UART_TRACE
	g_backend_gfx9000_present_snapshot_budget = 4u;
#endif
}

static boolean BackendGfx9000KernelTraceEnabled(void)
{
#if SMSBARE_V9990_UART_TRACE
	return g_backend_gfx9000_kernel_trace_remaining > 0u ? TRUE : FALSE;
#else
	return FALSE;
#endif
}

static void BackendGfx9000KernelTrace(const char *text)
{
	char marker;
	if (text == 0 || text[0] == '\0' || !BackendGfx9000KernelTraceEnabled())
	{
		return;
	}

	marker = BackendGfx9000KernelTraceCompactMarker(text);
	if (marker != '\0')
	{
		BackendUartTraceWriteChar(marker);
		return;
	}

	smsbare_uart_write(text);
	smsbare_uart_write("\n");
}

static void BackendGfx9000KernelTraceConsume(void)
{
	if (g_backend_gfx9000_kernel_trace_remaining > 0u)
	{
		--g_backend_gfx9000_kernel_trace_remaining;
	}
}

static void FlushBackendUartTraceBufferBudget(unsigned budget)
{
	if (g_backend_uart_trace_serial == 0)
	{
		return;
	}

	if (g_backend_uart_trace_queue_len != 0u)
	{
		unsigned offset = 0u;
		unsigned remaining = g_backend_uart_trace_queue_len;
		while (remaining > 0u && budget > 0u)
		{
			unsigned request = remaining;
			if (request > kBackendUartTraceFlushChunkBytes)
			{
				request = kBackendUartTraceFlushChunkBytes;
			}
			if (request > budget)
			{
				request = budget;
			}

			const int written = g_backend_uart_trace_serial->Write(g_backend_uart_trace_queue + offset, request);
			if (written <= 0)
			{
				break;
			}

			offset += (unsigned) written;
			remaining -= (unsigned) written;
			if ((unsigned) written >= budget)
			{
				budget = 0u;
			}
			else
			{
				budget -= (unsigned) written;
			}
			if ((unsigned) written < request)
			{
				break;
			}
		}

		if (offset != 0u)
		{
			if (remaining != 0u)
			{
				memmove(g_backend_uart_trace_queue, g_backend_uart_trace_queue + offset, remaining);
			}
			g_backend_uart_trace_queue_len = remaining;
		}
	}

	if (g_backend_uart_trace_drop_notice_pending && g_backend_uart_trace_queue_len == 0u)
	{
		char line[48];
		CString drop_line;
		drop_line.Format("UARTDROP bytes=%u\n", g_backend_uart_trace_dropped_bytes);
		strncpy(line, (const char *) drop_line, sizeof(line) - 1u);
		line[sizeof(line) - 1u] = '\0';
		(void) g_backend_uart_trace_serial->Write(line, strlen(line));
		g_backend_uart_trace_dropped_bytes = 0u;
		g_backend_uart_trace_drop_notice_pending = FALSE;
	}
}

static void FlushBackendUartTraceBuffer(void)
{
	if (BackendGfx9000KernelTraceEnabled())
	{
		return;
	}

	FlushBackendUartTraceBufferBudget(kBackendUartTraceFlushBudgetBytes);
}

static void FlushBackendUartTraceBufferTraceWindow(void)
{
	if (BackendGfx9000KernelTraceEnabled())
	{
		return;
	}

	FlushBackendUartTraceBufferBudget(kBackendUartTraceFlushBudgetBytes);
}

static boolean BackendQueueKernelLogDuringGfx9000Trace(const char *prefix, const char *text)
{
	if (!BackendGfx9000KernelTraceEnabled() || text == 0 || text[0] == '\0')
	{
		return FALSE;
	}

	if (prefix != 0 && prefix[0] != '\0')
	{
		smsbare_uart_write(prefix);
	}
	smsbare_uart_write(text);
	smsbare_uart_write("\n");
	return TRUE;
}

extern "C" void smsbare_uart_write(const char *text)
{
	if (text == 0 || text[0] == '\0')
	{
		return;
	}

	while (*text != '\0')
	{
		if (g_backend_uart_trace_queue_len + 1u >= sizeof(g_backend_uart_trace_queue))
		{
			g_backend_uart_trace_dropped_bytes++;
			g_backend_uart_trace_drop_notice_pending = TRUE;
			++text;
			continue;
		}

		g_backend_uart_trace_queue[g_backend_uart_trace_queue_len++] = *text++;
	}
}

extern "C" void smsbare_debug_trace(const char *text)
{
	if (text == 0 || text[0] == '\0')
	{
		return;
	}

	if (BackendGfx9000KernelTraceEnabled())
	{
		smsbare_uart_write(text);
		return;
	}

	CLogger *const logger = CLogger::Get();
	if (logger != 0)
	{
		logger->Write("combo", LogNotice, "%s", text);
		return;
	}

	smsbare_uart_write(text);
}

extern "C" unsigned smsbare_debug_timer_irq_binding(unsigned stage)
{
	return CComboKernel::DebugRepairTimerIrqBindingCurrent(stage);
}
#else
extern "C" void smsbare_debug_rearm_kernel_gfx9000_trace(unsigned frames)
{
	(void) frames;
}

extern "C" void smsbare_debug_note_gfx9000_activation(void)
{
}

static void FlushBackendUartTraceBuffer(void)
{
}

static void FlushBackendUartTraceBufferTraceWindow(void)
{
}

extern "C" void smsbare_uart_write(const char *text)
{
	(void) text;
}

extern "C" void smsbare_debug_trace(const char *text)
{
	(void) text;
}

extern "C" unsigned smsbare_debug_timer_irq_binding(unsigned stage)
{
	(void) stage;
	return 0u;
}

static boolean BackendGfx9000KernelTraceEnabled(void)
{
	return FALSE;
}

static void BackendGfx9000KernelTrace(const char *text)
{
	(void) text;
}

static void BackendGfx9000KernelTraceConsume(void)
{
}

static boolean BackendQueueKernelLogDuringGfx9000Trace(const char *prefix, const char *text)
{
	(void) prefix;
	(void) text;
	return FALSE;
}
#endif

static const char FromKernel[] = "combo";
static const unsigned kSampleRate = 48000;
static const unsigned kChunkSize = 384 * 10;
static const unsigned kQueueMs = 120;
/* 16667us caps cadence at <=60fps (1000000/60 truncated would run slightly >60). */
static const u64 kFrameInterval60Us = 16667ull;
static const u64 kFrameInterval50Us = 20000ull;
static const u64 kDebugOverlayIntervalUs = 200000ull;
static const u64 kUartPerfLogIntervalUs = 250000ull;
static const u64 kFramePacingGuardUs = 300ull;
static const u64 kFramePacingSleepTargetUs = 1200ull;
static const u64 kFramePacingSpinThresholdUs = 80ull;
static const uintptr kDCacheLineSize = 64u;
static const u16 kLetterboxColor565 = 0x0000u;
static const unsigned kScanlineModeOff = 0u;
static const unsigned kScanlineModeLcd = 1u;
static const unsigned kBootModeNorm = 0u;
static const char kDefaultCoreName[] = "sms";

namespace {
struct InterruptSystemLayoutProbe
{
	TIRQHandler *handlers[IRQ_LINES];
	void *params[IRQ_LINES];
};

struct InterruptBindingBaseline
{
	TIRQHandler *timer_handler;
	TIRQHandler *usb_handler;
	boolean timer_valid;
	boolean usb_valid;
};

static InterruptBindingBaseline g_InterruptBindingBaseline = {0, 0, FALSE, FALSE};
}
static const unsigned kBootModeGame = 2u;
static const unsigned kRamMapperKbDefault = 512u;
static const unsigned kMegaRamKbDefault = 0u;
static const unsigned kDefaultVideoWidth = 256u;
static const unsigned kDefaultVideoHeight = 192u;
static const unsigned kVideoFramebufferScale = 2u;
static const unsigned kVideoOverscanScaleNum = 9u;
static const unsigned kVideoOverscanScaleDen = 4u;
static const char kSettingsDrive[] = "SD:";
static const char kSettingsPath[] = "SD:/sms.cfg";
static const char kBootConfigPath[] = "SD:/config.txt";
static const char kBootConfigKernelStart[] = "#---KERNEL START---";
static const char kBootConfigKernelEnd[] = "#---KERNEL END---";
static const char kStateDirPath[] = "SD:/sms";
static const char kStatePath[] = "SD:/sms/slot-last.sav";
static const unsigned kStateBufferMaxBytes = 2u * 1024u * 1024u;
static const u8 kJoyBridgeRight = (u8) (1u << 0);
static const u8 kJoyBridgeLeft = (u8) (1u << 1);
static const u8 kJoyBridgeDown = (u8) (1u << 2);
static const u8 kJoyBridgeUp = (u8) (1u << 3);
static const u8 kJoyBridgeFireA = (u8) (1u << 4);
static const u8 kJoyBridgeFireB = (u8) (1u << 5);
static const u64 kAutofireHalfPeriodUs = 50000ull;
static const boolean kBootQuiet = TRUE;

#ifndef SMSBARE_BUILD_OWNER
#define SMSBARE_BUILD_OWNER ""
#endif

#ifndef SMSBARE_BUILD_BID
#define SMSBARE_BUILD_BID ""
#endif

static unsigned BuildHash16FromString(const char *text)
{
	unsigned hash = 2166136261u; /* FNV-1a 32-bit basis */
	if (text == 0)
	{
		return 0u;
	}
	while (*text != '\0')
	{
		hash ^= (unsigned) (u8) *text++;
		hash *= 16777619u; /* FNV prime */
	}
	hash ^= (hash >> 16);
	return hash & 0xFFFFu;
}

static unsigned BuildHash16(void)
{
	static unsigned cached = 0xFFFFFFFFu;
	if (cached == 0xFFFFFFFFu)
	{
		cached = BuildHash16FromString(SMSBARE_BUILD_BID);
	}
	return cached;
}

#if SMSBARE_ENABLE_DEBUG_OVERLAY
static char HexNibble(unsigned value)
{
	value &= 0x0Fu;
	return (char) (value < 10u ? ('0' + value) : ('A' + (value - 10u)));
}

static void AppendHexFixed(char *buffer, unsigned *offset, unsigned capacity, unsigned long long value, unsigned digits)
{
	unsigned shift_digits = digits;
	while (shift_digits > 0u && *offset + 1u < capacity)
	{
		--shift_digits;
		buffer[(*offset)++] = HexNibble((unsigned) (value >> (shift_digits * 4u)));
	}
}

static void AppendText(char *buffer, unsigned *offset, unsigned capacity, const char *text)
{
	if (text == 0)
	{
		return;
	}
	while (*text != '\0' && *offset + 1u < capacity)
	{
		buffer[(*offset)++] = *text++;
	}
}

static void EmitIrqProbeLog(const char *tag,
                            unsigned stage,
                            unsigned flags,
                            uintptr handler,
                            uintptr param,
                            uintptr expected)
{
	char line[96];
	unsigned offset = 0u;

	AppendText(line, &offset, sizeof(line), tag);
	AppendText(line, &offset, sizeof(line), " s:");
	AppendHexFixed(line, &offset, sizeof(line), stage, 2u);
	AppendText(line, &offset, sizeof(line), " f:");
	AppendHexFixed(line, &offset, sizeof(line), flags, 2u);
	AppendText(line, &offset, sizeof(line), " h:");
	AppendHexFixed(line, &offset, sizeof(line), (unsigned long long) handler, (unsigned) (sizeof(uintptr) * 2u));
	AppendText(line, &offset, sizeof(line), " p:");
	AppendHexFixed(line, &offset, sizeof(line), (unsigned long long) param, (unsigned) (sizeof(uintptr) * 2u));
	AppendText(line, &offset, sizeof(line), " e:");
	AppendHexFixed(line, &offset, sizeof(line), (unsigned long long) expected, (unsigned) (sizeof(uintptr) * 2u));
	if (offset + 1u < sizeof(line))
	{
		line[offset++] = '\n';
	}
	line[offset] = '\0';
	smsbare_uart_write(line);
}

static unsigned DebugExpectedTimerIrqLine(void)
{
#ifdef USE_PHYSICAL_COUNTER
	return ARM_IRQLOCAL0_CNTPNS;
#else
	return ARM_IRQ_TIMER3;
#endif
}
#endif

static unsigned ClampDebugOverlayMode(unsigned mode)
{
	return (mode <= ComboDebugOverlayMin) ? mode : ComboDebugOverlayOff;
}

static boolean DebugOverlayModeEnabled(unsigned mode)
{
	return ClampDebugOverlayMode(mode) != ComboDebugOverlayOff ? TRUE : FALSE;
}

static unsigned NextDebugOverlayMode(unsigned mode)
{
	switch (ClampDebugOverlayMode(mode))
	{
	case ComboDebugOverlayOff: return ComboDebugOverlayMax;
	case ComboDebugOverlayMax: return ComboDebugOverlayMin;
	case ComboDebugOverlayMin:
	default: return ComboDebugOverlayOff;
	}
}

static const char *DebugOverlayModeLogLabel(unsigned mode)
{
	switch (ClampDebugOverlayMode(mode))
	{
	case ComboDebugOverlayMax: return "MAX";
	case ComboDebugOverlayMin: return "MIN";
	case ComboDebugOverlayOff:
	default: return "OFF";
	}
}

static void BootPhase(CLogger &logger, const char *phase, const char *status)
{
	CString line;
	line.Format("%s: %s", phase != 0 ? phase : "BT?", status != 0 ? status : "");
	logger.Write(FromKernel, LogNotice, line);
}

static void BootPhaseWarn(CLogger &logger, const char *phase, const char *status)
{
	CString line;
	line.Format("%s: %s", phase != 0 ? phase : "BT?", status != 0 ? status : "");
	logger.Write(FromKernel, LogWarning, line);
}

static const char *ThrottleStateLabel(TSystemThrottledState state)
{
	switch (state)
	{
	case SystemStateUnderVoltageOccurred: return "UNDERVOLT";
	case SystemStateFrequencyCappingOccurred: return "FREQCAP";
	case SystemStateThrottlingOccurred: return "THROTTLED";
	case SystemStateSoftTempLimitOccurred: return "SOFTTEMP";
	default: return "MIXED";
	}
}

static unsigned UMax(unsigned a, unsigned b)
{
	return a > b ? a : b;
}

static unsigned UMin(unsigned a, unsigned b)
{
	return a < b ? a : b;
}

static boolean CanUseDirectPresentWithDebug(unsigned dst_x,
							 unsigned dst_y,
							 unsigned dst_w,
							 unsigned dst_h,
							 unsigned screen_w,
							 unsigned screen_h)
{
	(void) dst_x;
	(void) dst_w;
	(void) screen_w;
	(void) screen_h;

	/*
	 * In debug mode the viewport is forced down to a compact 1x-style mailbox,
	 * centered on screen. When its top edge stays well below the text overlay
	 * band, direct present is safe and avoids the extra SetArea() copy.
	 */
	if (dst_h == 0u)
	{
		return FALSE;
	}
	return dst_y >= 144u ? TRUE : FALSE;
}

static void ComputeViewportLayout(unsigned screen_width, unsigned screen_height,
							 unsigned src_width, unsigned src_height,
							 const BackendVideoPresentation *presentation,
							 unsigned *out_box_x,
							 unsigned *out_box_y,
							 unsigned *out_box_w,
							 unsigned *out_box_h,
							 unsigned *out_full_w,
							 unsigned *out_full_h,
							 unsigned *out_crop_x,
							 unsigned *out_crop_y)
{
	unsigned box_x = 0u;
	unsigned box_y = 0u;
	unsigned box_w = screen_width;
	unsigned box_h = screen_height;
	unsigned full_w = screen_width;
	unsigned full_h = screen_height;
	unsigned crop_x = 0u;
	unsigned crop_y = 0u;

	if (screen_width > 0u && screen_height > 0u && src_width > 0u && src_height > 0u)
	{
		(void) presentation;
		full_w = src_width * kVideoFramebufferScale;
		full_h = src_height * kVideoFramebufferScale;
		if (full_w == 0u) full_w = screen_width;
		if (full_h == 0u) full_h = screen_height;
		box_w = full_w > screen_width ? screen_width : full_w;
		box_h = full_h > screen_height ? screen_height : full_h;
		box_x = screen_width > box_w ? ((screen_width - box_w) / 2u) : 0u;
		box_y = screen_height > box_h ? ((screen_height - box_h) / 2u) : 0u;
		crop_x = full_w > box_w ? ((full_w - box_w) / 2u) : 0u;
		crop_y = full_h > box_h ? ((full_h - box_h) / 2u) : 0u;
	}

	if (out_box_x != 0) *out_box_x = box_x;
	if (out_box_y != 0) *out_box_y = box_y;
	if (out_box_w != 0) *out_box_w = box_w;
	if (out_box_h != 0) *out_box_h = box_h;
	if (out_full_w != 0) *out_full_w = full_w;
	if (out_full_h != 0) *out_full_h = full_h;
	if (out_crop_x != 0) *out_crop_x = crop_x;
	if (out_crop_y != 0) *out_crop_y = crop_y;
}

static unsigned MapWovenHiResPhysicalY(unsigned full_y,
									   unsigned full_dst_height,
									   unsigned logical_src_height,
									   unsigned physical_src_height,
									   boolean current_field_odd)
{
	if (full_dst_height == 0u || logical_src_height == 0u || physical_src_height == 0u)
	{
		return 0u;
	}

	const unsigned logical_y =
		(unsigned) (((u64) full_y * (u64) logical_src_height) / (u64) full_dst_height);
	const unsigned clamped_logical_y =
		logical_y >= logical_src_height ? (logical_src_height - 1u) : logical_y;
	const unsigned group_start =
		(unsigned) (((u64) clamped_logical_y * (u64) full_dst_height) / (u64) logical_src_height);
	const unsigned group_end =
		(unsigned) (((u64) (clamped_logical_y + 1u) * (u64) full_dst_height) / (u64) logical_src_height);
	const unsigned group_rows = group_end > group_start ? (group_end - group_start) : 1u;
	const unsigned local_row = full_y > group_start ? (full_y - group_start) : 0u;
	const unsigned field_odd = current_field_odd
		? (((local_row << 1) < group_rows) ? 1u : 0u)
		: (((local_row << 1) >= group_rows) ? 1u : 0u);
	const unsigned physical_y = (clamped_logical_y << 1) | field_odd;

	return physical_y < physical_src_height ? physical_y : (physical_src_height - 1u);
}

static char Port99OpTag(unsigned op)
{
	switch (op)
	{
	case 1u: return 'R';
	case 2u: return 'L';
	case 3u: return 'W';
	case 4u: return 'S';
	case 5u: return 'A';
	default: return '-';
	}
}

static u64 FrameIntervalFromRefreshHz(unsigned hz)
{
	return (hz == 50u) ? kFrameInterval50Us : kFrameInterval60Us;
}

static unsigned ClampRefreshRateHz(unsigned hz)
{
	return (hz == 50u) ? 50u : 60u;
}

static const char *MachineLabelFromProfile(unsigned profile)
{
	return backend_machine_profile_label(profile);
}

static boolean IsSmsFamilyProfile(unsigned profile)
{
	return backend_machine_profile_is_sms_family(profile);
}

static boolean MachineProfileSupportsAllocRam(unsigned profile)
{
	(void) profile;
	return FALSE;
}

static boolean IsMachineProfileSupportedByBuild(unsigned profile)
{
	return IsSmsFamilyProfile(profile);
}

static unsigned FirstSupportedMachineProfile(void)
{
	const unsigned count = backend_machine_profile_count();
	for (unsigned i = 0u; i < count; ++i)
	{
		if (IsMachineProfileSupportedByBuild(i))
		{
			return i;
		}
	}
	return 0u;
}

static unsigned ClampMachineProfileForBuild(unsigned profile)
{
	profile = backend_machine_profile_clamp(profile);
	if (IsMachineProfileSupportedByBuild(profile))
	{
		return profile;
	}
	return FirstSupportedMachineProfile();
}

static unsigned CycleMachineProfileForBuild(unsigned current, boolean reverse)
{
	const unsigned count = backend_machine_profile_count();
	if (count == 0u)
	{
		return 0u;
	}
	current = backend_machine_profile_clamp(current);
	for (unsigned step = 0u; step < count; ++step)
	{
		current = reverse
			? ((current == 0u) ? (count - 1u) : (current - 1u))
			: ((current + 1u) % count);
		if (IsMachineProfileSupportedByBuild(current))
		{
			return current;
		}
	}
	return FirstSupportedMachineProfile();
}

static unsigned ClampProcessorMode(unsigned mode)
{
	(void) mode;
	return 0u;
}

static unsigned CycleProcessorMode(unsigned current)
{
	(void) current;
	return 0u;
}

static const char *ProcessorModeLabel(unsigned mode)
{
	return ClampProcessorMode(mode) == 1u ? "6309" : "6809";
}

static const char *DebugModelNumberLabel(unsigned profile)
{
	(void) profile;
	return "?";
}

static unsigned ClampRamMapperKb(unsigned kb)
{
	switch (kb)
	{
	case 128u:
	case 256u:
	case 512u:
		return kb;
	default:
		return kRamMapperKbDefault;
	}
}

static unsigned CycleRamMapperKb(unsigned kb, boolean reverse)
{
	kb = ClampRamMapperKb(kb);
	if (!reverse)
	{
		return (kb == 128u) ? 256u : ((kb == 256u) ? 512u : 128u);
	}
	return (kb == 128u) ? 512u : ((kb == 512u) ? 256u : 128u);
}

static unsigned ClampMegaRamKb(unsigned kb)
{
	switch (kb)
	{
	case 0u:
	case 256u:
	case 512u:
	case 1024u:
		return kb;
	default:
		return 0u;
	}
}

static unsigned CycleMegaRamKb(unsigned kb, boolean reverse)
{
	kb = ClampMegaRamKb(kb);
	if (!reverse)
	{
		return (kb == 0u) ? 256u : ((kb == 256u) ? 512u : ((kb == 512u) ? 1024u : 0u));
	}
	return (kb == 0u) ? 1024u : ((kb == 1024u) ? 512u : ((kb == 512u) ? 256u : 0u));
}

static unsigned AudioGainEffectivePct(unsigned ui_pct)
{
	if (ui_pct > 100u) ui_pct = 100u;
	/* UI gain is now a direct 0..100% scaler in the final audio stage. */
	return ui_pct;
}

static unsigned AudioGainUiLevelFromPercent(unsigned ui_pct)
{
	if (ui_pct > 100u) ui_pct = 100u;
	return (ui_pct * 9u + 50u) / 100u;
}

static char ToUpperAscii(char ch)
{
	if (ch >= 'a' && ch <= 'z')
	{
		return (char) (ch - 'a' + 'A');
	}
	return ch;
}

static int IsFileChar83(char ch)
{
	if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
	{
		return 1;
	}
	return (ch == '_' || ch == '$' || ch == '~' || ch == '!' || ch == '#' || ch == '%' || ch == '-');
}

static const char *BaseNameFromPath(const char *path)
{
	if (path == 0)
	{
		return 0;
	}

	const char *base = path;
	for (const char *p = path; *p != '\0'; ++p)
	{
		if (*p == '/' || *p == '\\' || *p == ':')
		{
			base = p + 1;
		}
	}
	return base;
}

static void FormatRomName83(const char *path, char out[17])
{
	if (out == 0)
	{
		return;
	}

	if (path == 0 || path[0] == '\0')
	{
		out[0] = '\0';
		return;
	}

	const char *base = BaseNameFromPath(path);
	if (base == 0 || base[0] == '\0')
	{
		out[0] = '\0';
		return;
	}

	unsigned n = 0u;
	for (const char *p = base; *p != '\0' && *p != '.' && n < 16u; ++p)
	{
		char ch = ToUpperAscii(*p);
		if (IsFileChar83(ch))
		{
			out[n++] = ch;
		}
	}
	if (n == 0u)
	{
		out[n++] = '_';
	}
	out[n] = '\0';
}

static unsigned ParseUintValue(const char *text)
{
	if (text == 0)
	{
		return 0u;
	}

	unsigned value = 0u;
	while (*text == ' ' || *text == '\t')
	{
		++text;
	}

	while (*text >= '0' && *text <= '9')
	{
		value = (value * 10u) + (unsigned) (*text - '0');
		++text;
	}

	return value;
}

static unsigned ClampFatFsCode(FRESULT code)
{
	return ((unsigned) code > 999u) ? 999u : (unsigned) code;
}

static void LogSdOpResult(CLogger &logger, const char *op, FRESULT code, const char *path, boolean warning)
{
	CString line;
	if (path != 0 && path[0] != '\0')
	{
		line.Format("SD %s fr=%u path=%s", op != 0 ? op : "OP", ClampFatFsCode(code), path);
	}
	else
	{
		line.Format("SD %s fr=%u", op != 0 ? op : "OP", ClampFatFsCode(code));
	}
	logger.Write(FromKernel, warning ? LogWarning : LogNotice, line);
}

static void ParseTextValueLine(const char *value_start, const char *value_end, char *dst, unsigned dst_size)
{
	auto text_to_upper = [](char c) -> char {
		return (c >= 'a' && c <= 'z') ? (char) (c - ('a' - 'A')) : c;
	};
	auto text_is_space = [](char c) -> boolean {
		return (c == ' ' || c == '\t') ? TRUE : FALSE;
	};
	auto text_is_empty_marker = [&](const char *text) -> boolean {
		if (text == 0)
		{
			return FALSE;
		}
		const char *p = text;
		while (text_is_space(*p))
		{
			++p;
		}
		if (*p == '[')
		{
			++p;
			while (text_is_space(*p))
			{
				++p;
			}
		}
		static const char kEmptyTag[] = "EMPTY";
		for (unsigned i = 0u; kEmptyTag[i] != '\0'; ++i)
		{
			if (text_to_upper(p[i]) != kEmptyTag[i])
			{
				return FALSE;
			}
		}
		p += 5;
		while (text_is_space(*p))
		{
			++p;
		}
		if (*p == ']')
		{
			++p;
		}
		while (text_is_space(*p))
		{
			++p;
		}
		return (*p == '\0') ? TRUE : FALSE;
	};

	if (dst == 0 || dst_size == 0u)
	{
		return;
	}
	dst[0] = '\0';
	if (value_start == 0 || value_end == 0 || value_end < value_start)
	{
		return;
	}

	unsigned n = 0u;
	const char *p = value_start;
	while (p < value_end && n + 1u < dst_size)
	{
		dst[n++] = *p++;
	}
	dst[n] = '\0';
	if (text_is_empty_marker(dst))
	{
		dst[0] = '\0';
	}
}

static int SettingsKeyEquals(const char *key, unsigned key_len, const char *literal)
{
	if (key == 0 || literal == 0)
	{
		return 0;
	}

	unsigned i = 0u;
	for (; i < key_len; ++i)
	{
		if (literal[i] == '\0' || key[i] != literal[i])
		{
			return 0;
		}
	}

	return literal[i] == '\0';
}

static boolean BootConfigKeyEquals(const char *line, unsigned key_len, const char *literal)
{
	if (line == 0 || literal == 0)
	{
		return FALSE;
	}
	for (unsigned i = 0u; i < key_len; ++i)
	{
		if (literal[i] == '\0' || line[i] != literal[i])
		{
			return FALSE;
		}
	}
	return literal[key_len] == '\0' ? TRUE : FALSE;
}

static boolean BootConfigLineEquals(const char *line, const char *line_end, const char *literal)
{
	if (line == 0 || line_end == 0 || literal == 0)
	{
		return FALSE;
	}
	while (line < line_end && (*line == ' ' || *line == '\t'))
	{
		++line;
	}
	while (line_end > line && (line_end[-1] == ' ' || line_end[-1] == '\t'))
	{
		--line_end;
	}
	unsigned i = 0u;
	while (line + i < line_end && literal[i] != '\0')
	{
		if (line[i] != literal[i])
		{
			return FALSE;
		}
		++i;
	}
	return (line + i == line_end && literal[i] == '\0') ? TRUE : FALSE;
}

static boolean FindBootConfigBlock(char *buffer, unsigned buffer_len,
				   const char *start_marker, const char *end_marker,
				   char **content_start, char **content_end)
{
	if (buffer == 0 || start_marker == 0 || end_marker == 0
	 || content_start == 0 || content_end == 0)
	{
		return FALSE;
	}
	char *p = buffer;
	char *end = buffer + buffer_len;
	char *start_content = 0;
	while (p < end)
	{
		char *line = p;
		char *line_end = line;
		while (line_end < end && *line_end != '\r' && *line_end != '\n')
		{
			++line_end;
		}
		char *next = line_end;
		while (next < end && (*next == '\r' || *next == '\n'))
		{
			++next;
		}
		if (start_content == 0)
		{
			if (BootConfigLineEquals(line, line_end, start_marker))
			{
				start_content = next;
			}
		}
		else if (BootConfigLineEquals(line, line_end, end_marker))
		{
			*content_start = start_content;
			*content_end = line;
			return TRUE;
		}
		p = next;
	}
	return FALSE;
}

static boolean FindBootConfigKernelBlock(char *buffer, unsigned buffer_len,
					  char **content_start, char **content_end)
{
	return FindBootConfigBlock(buffer, buffer_len,
				   kBootConfigKernelStart, kBootConfigKernelEnd,
				   content_start, content_end);
}

static char KernelToLowerAscii(char ch)
{
	if (ch >= 'A' && ch <= 'Z')
	{
		return (char) (ch - 'A' + 'a');
	}
	return ch;
}

static int KernelTextEqualsIgnoreCase(const char *a, const char *b)
{
	if (a == 0 || b == 0)
	{
		return 0;
	}
	unsigned i = 0u;
	while (a[i] != '\0' && b[i] != '\0')
	{
		if (KernelToLowerAscii(a[i]) != KernelToLowerAscii(b[i]))
		{
			return 0;
		}
		++i;
	}
	return (a[i] == '\0' && b[i] == '\0') ? 1 : 0;
}

static int KernelEndsWithIgnoreCase(const char *text, const char *suffix)
{
	if (text == 0 || suffix == 0)
	{
		return 0;
	}
	const unsigned text_len = (unsigned) strlen(text);
	const unsigned suffix_len = (unsigned) strlen(suffix);
	if (suffix_len == 0u || text_len < suffix_len)
	{
		return 0;
	}
	const unsigned start = text_len - suffix_len;
	for (unsigned i = 0u; i < suffix_len; ++i)
	{
		if (KernelToLowerAscii(text[start + i]) != KernelToLowerAscii(suffix[i]))
		{
			return 0;
		}
	}
	return 1;
}

static void KernelCopyText(char *dst, unsigned dst_size, const char *src)
{
	if (dst == 0 || dst_size == 0u)
	{
		return;
	}
	if (src == 0)
	{
		dst[0] = '\0';
		return;
	}
	unsigned i = 0u;
	while (src[i] != '\0' && i + 1u < dst_size)
	{
		dst[i] = src[i];
		++i;
	}
	dst[i] = '\0';
}

static void KernelCopyCoreNameFromPath(char *dst, unsigned dst_size, const char *path_start, const char *path_end)
{
	if (dst == 0 || dst_size == 0u)
	{
		return;
	}
	dst[0] = '\0';
	if (path_start == 0 || path_end == 0 || path_start >= path_end)
	{
		return;
	}
	while (path_start < path_end && (*path_start == ' ' || *path_start == '\t'))
	{
		++path_start;
	}
	while (path_end > path_start && (path_end[-1] == ' ' || path_end[-1] == '\t'))
	{
		--path_end;
	}
	const char *base = path_start;
	for (const char *p = path_start; p < path_end; ++p)
	{
		if (*p == '/' || *p == '\\')
		{
			base = p + 1;
		}
	}
	const char *name_end = path_end;
	if ((unsigned) (name_end - base) > 4u
	 && KernelToLowerAscii(name_end[-4]) == '.'
	 && KernelToLowerAscii(name_end[-3]) == 'i'
	 && KernelToLowerAscii(name_end[-2]) == 'm'
	 && KernelToLowerAscii(name_end[-1]) == 'g')
	{
		name_end -= 4;
	}
	unsigned i = 0u;
	for (const char *p = base; p < name_end && i + 1u < dst_size; ++p)
	{
		const char ch = *p;
		if (ch == ' ' || ch == '\t')
		{
			break;
		}
		dst[i++] = ch;
	}
	dst[i] = '\0';
}

static char AsciiUpperChar(char c)
{
	return (c >= 'a' && c <= 'z') ? (char) (c - ('a' - 'A')) : c;
}

static boolean StringContainsCaseFold(const char *text, const char *needle)
{
	if (text == 0 || needle == 0 || needle[0] == '\0')
	{
		return FALSE;
	}

	for (; *text != '\0'; ++text)
	{
		const char *a = text;
		const char *b = needle;
		while (*a != '\0' && *b != '\0' && AsciiUpperChar(*a) == AsciiUpperChar(*b))
		{
			++a;
			++b;
		}
		if (*b == '\0')
		{
			return TRUE;
		}
	}

	return FALSE;
}

static boolean RomPathLooksLikeGfx9000(const char *path)
{
	return StringContainsCaseFold(path, "V9990")
		|| StringContainsCaseFold(path, "GFX9000")
		|| StringContainsCaseFold(path, "CODEINTRUDER")
		|| StringContainsCaseFold(path, "GHOSTGOBLINS")
		|| StringContainsCaseFold(path, "UNDERWATER");
}


static void FlushDCacheRange(void *addr, unsigned bytes)
{
	if (addr == 0 || bytes == 0u)
	{
		return;
	}

	uintptr start = (uintptr) addr;
	uintptr end = start + (uintptr) bytes;
	start &= ~(kDCacheLineSize - 1u);
	end = (end + (kDCacheLineSize - 1u)) & ~(kDCacheLineSize - 1u);

	for (uintptr p = start; p < end; p += kDCacheLineSize)
	{
		asm volatile ("dc cvac, %0" :: "r" (p) : "memory");
	}

	asm volatile ("dsb ish" ::: "memory");
}

static void FlushDCacheRect565(u16 *base,
					 unsigned pitch_pixels,
					 unsigned x,
					 unsigned y,
					 unsigned width,
					 unsigned height)
{
	if (base == 0 || width == 0u || height == 0u)
	{
		return;
	}

	for (unsigned row = 0u; row < height; ++row)
	{
		FlushDCacheRange(base + ((y + row) * pitch_pixels) + x, width * (unsigned) sizeof(u16));
	}
}

static void FlushDCacheRect565Contiguous(u16 *base,
						 unsigned pitch_pixels,
						 unsigned x,
						 unsigned y,
						 unsigned width,
						 unsigned height)
{
	if (base == 0 || width == 0u || height == 0u || pitch_pixels == 0u)
	{
		return;
	}

	u16 *flush_start = base + (y * pitch_pixels) + x;
	const unsigned flush_pixels = ((height - 1u) * pitch_pixels) + width;
	FlushDCacheRange(flush_start, flush_pixels * (unsigned) sizeof(u16));
}

static void FillRun565(u16 *dst, u16 sample, unsigned count)
{
	if (dst == 0 || count == 0u)
	{
		return;
	}

	if ((((uintptr) dst) & 0x2u) != 0u)
	{
		*dst++ = sample;
		if (--count == 0u)
		{
			return;
		}
	}

	const u32 packed = (u32) sample | ((u32) sample << 16);
	while (count >= 2u)
	{
		*(u32 *) (void *) dst = packed;
		dst += 2;
		count -= 2u;
	}
	if (count != 0u)
	{
		*dst = sample;
	}
}

static void BlitMappedRunsRgb565(u16 *dst_line,
						  const u16 *src_line,
						  const u16 *run_src_x,
						  const u16 *run_dst_x,
						  const u16 *run_len,
						  unsigned run_count,
						  unsigned src_x0,
						  unsigned dim_mode)
{
	if (dst_line == 0 || src_line == 0 || run_src_x == 0 || run_dst_x == 0 || run_len == 0)
	{
		return;
	}

	for (unsigned i = 0u; i < run_count; ++i)
	{
		u16 sample = src_line[src_x0 + run_src_x[i]];
		if (dim_mode == 1u)
		{
			sample = (u16) ((sample & 0xF7DEu) >> 1);
		}
		FillRun565(dst_line + run_dst_x[i], sample, run_len[i]);
	}
}

static void BlitLineExpandNxRgb565NoDim(u16 *dst_line,
							const u16 *src_line,
							unsigned src_width,
							unsigned scale)
{
	if (dst_line == 0 || src_line == 0 || src_width == 0u || scale == 0u)
	{
		return;
	}

	for (unsigned x = 0u; x < src_width; ++x)
	{
		FillRun565(dst_line + (x * scale), src_line[x], scale);
	}
}

static void FillRect565(u16 *dst_base,
					  unsigned pitch_pixels,
					  unsigned screen_width,
					  unsigned screen_height,
					  unsigned x,
					  unsigned y,
					  unsigned w,
					  unsigned h,
					  u16 color)
{
	if (dst_base == 0 || w == 0u || h == 0u || pitch_pixels == 0u)
	{
		return;
	}

	if (x >= screen_width || y >= screen_height)
	{
		return;
	}

	const unsigned max_w = screen_width - x;
	const unsigned max_h = screen_height - y;
	if (w > max_w) w = max_w;
	if (h > max_h) h = max_h;
	if (w == 0u || h == 0u)
	{
		return;
	}

	for (unsigned row = 0; row < h; ++row)
	{
		u16 *dst = dst_base + ((y + row) * pitch_pixels) + x;
		for (unsigned col = 0; col < w; ++col)
		{
			dst[col] = color;
		}
	}
}

static void FillRectScreen(CComboScreenDevice *screen,
					   unsigned x,
					   unsigned y,
					   unsigned w,
					   unsigned h,
					   TScreenColor color)
{
	if (screen == 0 || w == 0u || h == 0u)
	{
		return;
	}

	for (unsigned row = 0; row < h; ++row)
	{
		for (unsigned col = 0; col < w; ++col)
		{
			screen->SetPixel(x + col, y + row, color);
		}
	}
}

#ifndef SMSBARE_AUDIO_GAIN
#define SMSBARE_AUDIO_GAIN 1
#endif

#ifndef SMSBARE_AUDIO_GAIN_MAX
#define SMSBARE_AUDIO_GAIN_MAX 16
#endif

#ifndef SMSBARE_AUDIO_TARGET_PEAK
#define SMSBARE_AUDIO_TARGET_PEAK 12000
#endif

#ifndef SMSBARE_AUDIO_PROBE_TONE
#define SMSBARE_AUDIO_PROBE_TONE 0
#endif

#ifndef SMSBARE_AUDIO_BOOT_TONE_MS
#define SMSBARE_AUDIO_BOOT_TONE_MS 0
#endif

#ifndef SMSBARE_AUDIO_BOOT_TONE_AMP
#define SMSBARE_AUDIO_BOOT_TONE_AMP 12000
#endif

#define SMSBARE_AUDIO_DISABLE_AUTO_GAIN 1
#define SMSBARE_AUDIO_DEFAULT_GAIN_PCT 100
#define SMSBARE_AUDIO_DEFAULT_CLIP_MAX 32767

#ifndef SMSBARE_ENABLE_DEBUG_OVERLAY
#define SMSBARE_ENABLE_DEBUG_OVERLAY 0
#endif

CComboKernel *CComboKernel::s_Instance = 0;

CComboKernel::CComboKernel(void)
:	m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
	m_GfxText(&m_Screen),
	m_Timer(&m_Interrupt),
	m_Logger(m_Options.GetLogLevel(), &m_Timer),
	m_USBHCI(&m_Interrupt, &m_Timer, TRUE),
	m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
	m_Sound(0),
	m_UsbGamepad(),
	m_UsbKeyboard(),
	m_AudioPhase(0),
	m_AudioStep((u32)(((u64)440u << 32) / (u64)kSampleRate)),
	m_PlugAndPlayCount(0),
	m_KeyEventCount(0),
	m_LastKeyCode(0),
	m_AudioWriteOps(0),
	m_AudioDropOps(0),
	m_AudioOverwriteSeen(0),
	m_AudioTrimSeen(0),
	m_AudioUnderrunSeen(0),
	m_AudioShortPullSeen(0),
	m_AudioPeak(0),
	m_LastAudioSample(0),
	m_SoundStartAttempts(0),
	m_SoundStartFailures(0),
	m_LastQueueAvail(0),
	m_LastQueueFree(0),
	m_AudioTargetPeakRuntime(SMSBARE_AUDIO_TARGET_PEAK),
	m_AudioClipMaxRuntime(SMSBARE_AUDIO_DEFAULT_CLIP_MAX),
	m_AudioOutputGainPct(SMSBARE_AUDIO_DEFAULT_GAIN_PCT),
	m_BootToneFramesRemaining(0),
	m_UsbInitRetryTick(0u),
	m_AudioInitAttempted(FALSE),
	m_UartReady(FALSE),
	m_UsbReady(FALSE),
	m_JoyBridgeBits(0),
	m_JoyMappedBits(ComboKeyboardJoyMask()),
	m_DebugOverlayMode(ComboDebugOverlayOff),
	m_DebugOverlayUnlocked(TRUE),
	m_ScreenReady(FALSE),
	m_EmulationPaused(FALSE),
	m_LastF12Pressed(FALSE),
	m_LastPauseWasInSettings(FALSE),
	m_LastPausePressed(FALSE),
	m_LastMenuBackPressed(FALSE),
	m_PauseNeedsBackgroundRedraw(FALSE),
	m_PauseNeedsFullViewportClear(FALSE),
	m_ForceViewportFullClearNextFrame(FALSE),
	m_PauseRenderSuppressed(FALSE),
	m_MenuHardResetPending(FALSE),
	m_BackendEnabled(TRUE),
	m_BackendReady(FALSE),
	m_EmulatorBackendName("unknown"),
	m_BackendInitFailCode(0u),
	m_BackendFrames(0),
	m_BackendLastError(0),
	m_BackendLastPC(-1),
	m_BackendPcRepeatCount(0),
	m_BackendPcLoopLogged(FALSE),
	m_BackendVideoSeq(0),
	m_BackendVdpTraceSeq(0u),
	m_BackendVdpTraceDropped(0u),
	m_BackendAudioPortTraceSeq(0u),
	m_BackendAudioPortTraceDropped(0u),
	m_BackendKmgTraceSeq(0u),
	m_BackendKmgTraceDropped(0u),
	m_LastAudioQueueLogUs(0),
	m_LastVideoPacingLogUs(0),
	m_BackendFpsWindowStartUs(0),
	m_LastPauseToggleUs(0),
	m_LastMenuInputPollUs(0),
	m_LastOverlayUs(0),
	m_UartAudioQueueAvailMin(0u),
	m_UartAudioLowWaterHits(0u),
	m_UartAudioRefillUsLast(0u),
	m_UartAudioRefillUsPeak(0u),
	m_UartVideoCatchupLast(0u),
	m_UartVideoCatchupPeak(0u),
	m_UartVideoCatchupCapCount(0u),
	m_UartVideoLateUsLast(0u),
	m_UartVideoLateUsPeak(0u),
	m_UartVideoStepUsLast(0u),
	m_UartVideoStepUsPeak(0u),
	m_UartVideoBlitUsLast(0u),
	m_UartVideoBlitUsPeak(0u),
	m_LastVideoModeScreen(0u),
	m_LastVideoModeLogicalWidth(0u),
	m_LastVideoModeLogicalHeight(0u),
	m_HiresHostTraceFramesRemaining(0u),
	m_BackendFpsWindowStartFrames(0),
	m_BackendFpsX10(0),
	m_FramePacingErrAvgUs(0u),
	m_FramePacingErrMaxUs(0u),
	m_BackendBlankFrames(0),
	m_LastVideoModeLogValid(FALSE),
	m_LastVideoModeHires(FALSE),
	m_LastVideoModeInterlaced(FALSE),
	m_InitramfsFiles(0),
	m_BackendBoxX(0),
	m_BackendBoxY(0),
	m_BackendBoxW(0),
	m_BackendBoxH(0),
	m_BlitMapSrcW(0),
	m_BlitMapSrcH(0),
	m_BlitMapDstW(0),
	m_BlitMapDstH(0),
	m_BlitRunCount(0u),
	m_BlitMapValid(FALSE),
	m_BlitMapWovenHiRes(FALSE),
	m_Language(ComboLanguagePT),
	m_OverscanEnabled(FALSE),
	m_ScanlineMode(kScanlineModeOff),
	m_ColorArtifactsEnabled(TRUE),
	m_Gfx9000Enabled(FALSE),
	m_Gfx9000AutoCfgApplied(FALSE),
	m_RefreshRateHz(60u),
	m_DiskRomEnabled(FALSE),
	m_CoreNames{{0}},
	m_BootCoreName{'s', 'm', 's', '\0'},
	m_CoreCount(1u),
	m_CoreIndex(0u),
	m_CoreListLoaded(FALSE),
	m_CoreSwitchPending(FALSE),
	m_CoreRebootRequested(FALSE),
	m_BootMode(kBootModeNorm),
	m_MachineProfile(backend_machine_profile_default()->id),
	m_ProcessorMode(ClampProcessorMode(0u)),
	m_RamMapperKb(kRamMapperKbDefault),
	m_MegaRamKb(0u),
	m_AutofireEnabled(FALSE),
	m_AutofireButton2Active(FALSE),
	m_AutofireButton2OutputOn(TRUE),
	m_AutofireButton2LastToggleUs(0u),
	m_LastKeyboardModifiers(0u),
	m_LastKeyboardRawKeys{0u, 0u, 0u, 0u, 0u, 0u},
	m_RuntimeKeyboardModifiers(0u),
	m_RuntimeKeyboardRawKeys{0u, 0u, 0u, 0u, 0u, 0u},
	m_RuntimeKeyboardDispatchValid(FALSE),
	m_RuntimeKeyboardReplayBudget(0u),
	m_RuntimeKeyboardDispatchedModifiers(0u),
	m_RuntimeKeyboardDispatchedRawKeys{0u, 0u, 0u, 0u, 0u, 0u},
	m_FmMusicEnabled(TRUE),
	m_FmLayerGainPct(80u),
	m_SccCartEnabled(FALSE),
	m_SccDualCartEnabled(FALSE),
	m_Gfx9000ToggleTracePending(FALSE),
	m_Gfx9000ResumeTracePending(FALSE),
	m_FrameIntervalUs(FrameIntervalFromRefreshHz(60u)),
	m_FramePacingResetPending(FALSE),
	m_BackendFallbackPattern(FALSE),
	m_TestToneEnabled(FALSE),
		m_MmuEnabled(FALSE),
		m_DCacheEnabled(FALSE),
		m_ICacheEnabled(FALSE),
		m_SettingsStorageReady(FALSE),
		m_SettingsFatFsMounted(FALSE),
			m_SettingsFatFsMountPending(FALSE),
			m_UartTelemetryEnabled(FALSE),
	m_SettingsLoadPending(FALSE),
	m_InitramfsDeferredMountPending(FALSE),
	m_SettingsSavePending(FALSE),
	m_StateSavePending(FALSE),
	m_StateLoadPending(FALSE),
	m_MediaCatalogStarted(FALSE),
	m_MediaCatalogSummaryLogged(FALSE),
	m_PendingMenuAction((unsigned) ComboMenuActionNone),
	m_SettingsSaveStage(0u),
	m_SettingsSaveCode(0u),
		m_SettingsSaveAux(0u),
		m_BootStartUs(0u),
		m_BootToBackendStartMs(0u),
		m_BootToBackendMs(0u),
		m_BootFirstBlitMs(0u),
		m_BootUsbReadyMs(0u),
		m_BootBt14Ms(0u),
		m_BootFatDeferredMs(0u),
		m_StateIoBuffer(0),
	m_StateIoBufferCapacity(0u),
	m_BackendPresentBuffer(0),
	m_BackendPresentBufferPixels(0u)
{
	s_Instance = this;
	m_ActLED.Blink(5);
	memset(m_DebugOverlayLineLengths, 0, sizeof(m_DebugOverlayLineLengths));
	NormalizeSettingsForBuild();
	ResetUartPerfTelemetry();
}

unsigned CComboKernel::DebugRepairTimerIrqBindingCurrent(unsigned stage)
{
	if (s_Instance == 0)
	{
		return 0xFFFFFFFFu;
	}

	return s_Instance->DebugRepairTimerIrqBinding(stage);
}

unsigned CComboKernel::DebugRepairTimerIrqBinding(unsigned stage)
{
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	CInterruptSystem *const live_interrupt = CInterruptSystem::Get();
	InterruptSystemLayoutProbe *const irq = reinterpret_cast<InterruptSystemLayoutProbe *>(live_interrupt);
	const unsigned timer_irq_line = DebugExpectedTimerIrqLine();
	TIRQHandler *timer_handler = irq->handlers[timer_irq_line];
	void *timer_param = irq->params[timer_irq_line];
	TIRQHandler *usb_handler = irq->handlers[ARM_IRQ_USB];
	void *usb_param = irq->params[ARM_IRQ_USB];
	TIRQHandler *const timer_handler_observed = timer_handler;
	void *const timer_param_observed = timer_param;
	TIRQHandler *const usb_handler_observed = usb_handler;
	void *const usb_param_observed = usb_param;
	unsigned timer_result = 0u;
	unsigned usb_result = 0u;
	static boolean s_TimerIrqLineLogged = FALSE;
	static unsigned s_LastTimerResult = 0u;
	static unsigned s_LastTimerStage = 0xFFFFFFFFu;
	static unsigned s_LastUsbResult = 0u;
	static unsigned s_LastUsbStage = 0xFFFFFFFFu;

	if (live_interrupt == 0)
	{
		EmitIrqProbeLog("IQLV",
		                stage,
		                1u,
		                0u,
		                0u,
		                0u);
		return 0xFFFFFFFFu;
	}
	if (!s_TimerIrqLineLogged)
	{
		EmitIrqProbeLog("TIQN",
		                stage,
		                timer_irq_line,
		                0u,
		                0u,
		                0u);
		s_TimerIrqLineLogged = TRUE;
	}

	if (!g_InterruptBindingBaseline.timer_valid && timer_handler != 0)
	{
		g_InterruptBindingBaseline.timer_handler = timer_handler;
		g_InterruptBindingBaseline.timer_valid = TRUE;
	}

	if (!g_InterruptBindingBaseline.usb_valid && usb_handler != 0)
	{
		g_InterruptBindingBaseline.usb_handler = usb_handler;
		g_InterruptBindingBaseline.usb_valid = TRUE;
	}

	if (timer_handler == 0)
	{
		timer_result |= 0x01u;
	}
	else if (g_InterruptBindingBaseline.timer_valid
	      && timer_handler != g_InterruptBindingBaseline.timer_handler)
	{
		timer_result |= 0x10u;
	}
	if (timer_param != &m_Timer)
	{
		timer_result |= 0x02u;
	}

	if ((timer_result & 0x11u) != 0u && g_InterruptBindingBaseline.timer_valid)
	{
		DisableIRQs();
		irq->handlers[timer_irq_line] = g_InterruptBindingBaseline.timer_handler;
		EnableIRQs();
		timer_handler = irq->handlers[timer_irq_line];
		timer_result |= 0x04u;
	}
	if ((timer_result & 0x02u) != 0u && (timer_handler != 0 || g_InterruptBindingBaseline.timer_valid))
	{
		DisableIRQs();
		irq->params[timer_irq_line] = &m_Timer;
		EnableIRQs();
		timer_param = irq->params[timer_irq_line];
		timer_result |= 0x08u;
	}

	if (m_UsbReady || g_InterruptBindingBaseline.usb_valid || usb_handler != 0 || usb_param != 0)
	{
		if (usb_handler == 0)
		{
			usb_result |= 0x01u;
		}
		else if (g_InterruptBindingBaseline.usb_valid
		      && usb_handler != g_InterruptBindingBaseline.usb_handler)
		{
			usb_result |= 0x10u;
		}
		if (usb_param != &m_USBHCI)
		{
			usb_result |= 0x02u;
		}

		if ((usb_result & 0x11u) != 0u && g_InterruptBindingBaseline.usb_valid)
		{
			DisableIRQs();
			irq->handlers[ARM_IRQ_USB] = g_InterruptBindingBaseline.usb_handler;
			EnableIRQs();
			usb_handler = irq->handlers[ARM_IRQ_USB];
			usb_result |= 0x04u;
		}
		if ((usb_result & 0x02u) != 0u && (usb_handler != 0 || g_InterruptBindingBaseline.usb_valid))
		{
			DisableIRQs();
			irq->params[ARM_IRQ_USB] = &m_USBHCI;
			EnableIRQs();
			usb_param = irq->params[ARM_IRQ_USB];
			usb_result |= 0x08u;
		}
	}

	if (timer_result != 0u && (timer_result != s_LastTimerResult || stage != s_LastTimerStage))
	{
		EmitIrqProbeLog("TIRQ",
		                stage,
		                timer_result,
		                (uintptr) timer_handler_observed,
		                (uintptr) timer_param_observed,
		                (uintptr) &m_Timer);
		s_LastTimerResult = timer_result;
		s_LastTimerStage = stage;
	}
	else if (timer_result == 0u)
	{
		s_LastTimerResult = 0u;
	}

	if (usb_result != 0u && (usb_result != s_LastUsbResult || stage != s_LastUsbStage))
	{
		EmitIrqProbeLog("UIRQ",
		                stage,
		                usb_result,
		                (uintptr) usb_handler_observed,
		                (uintptr) usb_param_observed,
		                (uintptr) &m_USBHCI);
		s_LastUsbResult = usb_result;
		s_LastUsbStage = stage;
	}
	else if (usb_result == 0u)
	{
		s_LastUsbResult = 0u;
	}

	return (usb_result << 16) | timer_result;
#else
	(void) stage;
	return 0u;
#endif
}

void CComboKernel::NormalizeSettingsForBuild(void)
{
	m_Gfx9000Enabled = FALSE;
	m_MachineProfile = ClampMachineProfileForBuild(m_MachineProfile);
	m_BootMode = kBootModeNorm;

	m_DiskRomEnabled = FALSE;
	m_MegaRamKb = 0u;
	m_FmMusicEnabled = m_FmMusicEnabled ? TRUE : FALSE;
	m_SccCartEnabled = FALSE;
	m_SccDualCartEnabled = FALSE;

	m_RamMapperKb = ClampRamMapperKb(m_RamMapperKb);
	m_ProcessorMode = ClampProcessorMode(m_ProcessorMode);
	m_MegaRamKb = ClampMegaRamKb(m_MegaRamKb);
	m_AutofireEnabled = m_AutofireEnabled ? TRUE : FALSE;
	m_ColorArtifactsEnabled = m_ColorArtifactsEnabled ? TRUE : FALSE;
	if (!MachineProfileSupportsAllocRam(m_MachineProfile))
	{
		m_RamMapperKb = kRamMapperKbDefault;
	}
}

CComboKernel::~CComboKernel(void)
{
	if (m_StateIoBuffer != 0)
	{
		delete[] m_StateIoBuffer;
		m_StateIoBuffer = 0;
		m_StateIoBufferCapacity = 0u;
	}
	if (m_BackendPresentBuffer != 0)
	{
		delete[] m_BackendPresentBuffer;
		m_BackendPresentBuffer = 0;
		m_BackendPresentBufferPixels = 0u;
	}
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	g_backend_uart_trace_serial = 0;
#endif
	s_Instance = 0;
}

boolean CComboKernel::Initialize(void)
{
	boolean ok = TRUE;

	m_UartReady = m_Serial.Initialize(115200);
	if (ok)
	{
		ok = m_Logger.Initialize(m_UartReady ? (CDevice *) &m_Serial : (CDevice *) &m_NullLog);
		if (m_UartReady)
		{
			BootPhase(m_Logger, "BT0", "UART early... OK");
			m_Logger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);
		}
	}

	if (ok) ok = m_Interrupt.Initialize();
	if (ok) ok = m_Timer.Initialize();
	if (ok)
	{
		BootPhase(m_Logger, "BT1", "Interrupt + Timer... OK");
		if (m_CPUThrottle.IsDynamic())
		{
			(void) m_CPUThrottle.SetSpeed(CPUSpeedMaximum, TRUE);
		}
		m_CPUThrottle.RegisterSystemThrottledHandler(
			SystemStateUnderVoltageOccurred
				| SystemStateFrequencyCappingOccurred
				| SystemStateThrottlingOccurred
				| SystemStateSoftTempLimitOccurred,
			SystemThrottledHandler,
			this);
		LogCpuClockStatus("BT1");
		(void) DebugRepairTimerIrqBinding(0x01u);
	}

	return ok;
}

boolean CComboKernel::InitializeSettingsStorage(void)
{
	if (m_SettingsStorageReady)
	{
		return TRUE;
	}

	if (!m_EMMC.Initialize())
	{
		BootPhaseWarn(m_Logger, "BT4", "SD/eMMC driver... FAIL");
		m_Logger.Write(FromKernel, LogWarning, "SD storage init failed; settings persistence disabled");
		return FALSE;
	}

	m_SettingsStorageReady = TRUE;
	BootPhase(m_Logger, "BT4", "SD/eMMC driver... OK");
	return TRUE;
}

boolean CComboKernel::MountSettingsFileSystem(const char *phase)
{
	if (!m_SettingsStorageReady)
	{
		return FALSE;
	}
	if (m_SettingsFatFsMounted)
	{
		return TRUE;
	}

	const FRESULT mount_result = f_mount(&m_SettingsFileSystem, kSettingsDrive, 1);
	if (mount_result == FR_OK)
	{
		m_SettingsFatFsMounted = TRUE;
		BootPhase(m_Logger, phase != 0 ? phase : "BT5", "FATFS mount... OK");
		return TRUE;
	}

	BootPhaseWarn(m_Logger, phase != 0 ? phase : "BT5", "FATFS mount... FAIL");
	LogSdOpResult(m_Logger, "FAT:MOUNT", mount_result, kSettingsDrive, TRUE);
	return FALSE;
}

void CComboKernel::InitializeDebugRuntime(void)
{
#if !SMSBARE_ENABLE_DEBUG_OVERLAY
	return;
#else
	if (g_backend_uart_trace_serial != 0)
	{
		return;
	}

	if (!m_UartReady && !m_Serial.Initialize(115200))
	{
		BootPhaseWarn(m_Logger, "BT11", "UART + Logger... FAIL");
		return;
	}
	m_UartReady = TRUE;

	g_backend_uart_trace_serial = &m_Serial;
	FlushBackendUartTraceBuffer();
	if (m_Logger.GetTarget() != &m_Serial)
	{
		m_Logger.SetNewTarget(&m_Serial);
	}
	BootPhase(m_Logger, "BT11", "UART + Logger... OK");
	m_Logger.Write(FromKernel, LogNotice, "SMSBarePI: HDMI audio + USB keyboard");
	{
		CString mem_line;
		mem_line.Format("MMU/Caches M:%u C:%u I:%u", m_MmuEnabled ? 1u : 0u, m_DCacheEnabled ? 1u : 0u, m_ICacheEnabled ? 1u : 0u);
		m_Logger.Write(FromKernel, LogNotice, mem_line);
	}
	{
		const BackendInfo backend_info = backend_runtime_get_info();
		const char *selected_backend_id = backend_info.selected_backend_id != 0
			? backend_info.selected_backend_id
			: backend_runtime_selected_backend_id();
		CString backend_line;
		backend_line.Format("Emulator backend: %s [%s]",
			m_EmulatorBackendName != 0 ? m_EmulatorBackendName : "unknown",
			selected_backend_id != 0 ? selected_backend_id : "default");
		m_Logger.Write(FromKernel, LogNotice, backend_line);
		if (backend_info.requested_backend_id != 0
			&& backend_info.requested_backend_id[0] != '\0'
			&& !backend_info.requested_backend_found
			&& backend_info.selected_backend_id != 0)
		{
			CString fallback_line;
			fallback_line.Format("Backend fallback: requested=%s -> selected=%s",
				backend_info.requested_backend_id,
				backend_info.selected_backend_id);
			m_Logger.Write(FromKernel, LogWarning, fallback_line);
		}
	}
	{
		CString initramfs_line;
		initramfs_line.Format("initramfs files: %u", m_InitramfsFiles);
		m_Logger.Write(FromKernel, LogNotice, initramfs_line);
	}
	{
		CString boot_line;
		boot_line.Format("Fastboot to backend: %ums", m_BootToBackendMs);
		m_Logger.Write(FromKernel, LogNotice, boot_line);
	}
	{
		CString line;
		line.Format("BOOT_TIME start_to_backend_start_ms=%u", m_BootToBackendStartMs);
		m_Logger.Write(FromKernel, LogNotice, line);
		line.Format("BOOT_TIME start_to_backend_ready_ms=%u", m_BootToBackendMs);
		m_Logger.Write(FromKernel, LogNotice, line);
		line.Format("BOOT_TIME initramfs_source=%s", backend_runtime_initramfs_source());
		m_Logger.Write(FromKernel, LogNotice, line);
		line.Format("BOOT_TIME initramfs_bytes=%u", backend_runtime_initramfs_bytes());
		m_Logger.Write(FromKernel, LogNotice, line);
		if (!m_SettingsFatFsMountPending)
		{
			line.Format("BOOT_TIME fat_mount_ms=%u", m_BootFatDeferredMs);
			m_Logger.Write(FromKernel, LogNotice, line);
		}
	}
	LogCpuClockStatus("BT11");
#endif
}

void CComboKernel::ProcessDeferredBootTasks(void)
{
	if (m_InitramfsDeferredMountPending)
	{
		const unsigned mounted_more = backend_runtime_initramfs_mount_remaining();
		m_InitramfsFiles += mounted_more;
		m_InitramfsDeferredMountPending = FALSE;
		if (mounted_more > 0u)
		{
			CString line;
			line.Format("Boot deferred: initramfs remaining +%u => %u", mounted_more, m_InitramfsFiles);
			m_Logger.Write(FromKernel, LogNotice, line);
		}
	}
	if (m_SettingsFatFsMountPending)
	{
		const u64 fat_start_us = CTimer::GetClockTicks64();
		(void) MountSettingsFileSystem("BT5D");
		m_BootFatDeferredMs = (unsigned) ((CTimer::GetClockTicks64() - fat_start_us) / 1000u);
		m_SettingsFatFsMountPending = FALSE;
#if SMSBARE_ENABLE_DEBUG_OVERLAY
		{
			CString line;
			line.Format("BOOT_TIME fat_mount_ms=%u", m_BootFatDeferredMs);
			m_Logger.Write(FromKernel, LogNotice, line);
		}
#endif
	}
}

TComboShutdownMode CComboKernel::Run(void)
{
	if (!kBootQuiet)
	{
		BootTrace("R0");
	}
	m_BootStartUs = CTimer::GetClockTicks64();
	m_BootToBackendStartMs = 0u;
	m_BootToBackendMs = 0u;
	m_BootFirstBlitMs = 0u;
	m_BootUsbReadyMs = 0u;
	m_BootBt14Ms = 0u;
	m_BootFatDeferredMs = 0u;
	u64 sctlr_el1 = 0;
	asm volatile ("mrs %0, sctlr_el1" : "=r" (sctlr_el1));
	m_MmuEnabled = (sctlr_el1 & (1u << 0)) ? TRUE : FALSE;
	m_DCacheEnabled = (sctlr_el1 & (1u << 2)) ? TRUE : FALSE;
	m_ICacheEnabled = (sctlr_el1 & (1u << 12)) ? TRUE : FALSE;
	const boolean backend_audio_enabled = m_BackendEnabled ? TRUE : FALSE;

	if (!m_Screen.Initialize())
	{
		BootPhaseWarn(m_Logger, "BT2", "screen... FAIL");
		m_Logger.Write(FromKernel, LogPanic, "Screen init failed");
		return ComboShutdownHalt;
	}
	m_ScreenReady = TRUE;
	BootPhase(m_Logger, "BT2", "screen... OK");

	if (m_TestToneEnabled || backend_audio_enabled)
	{
		InitializeAudioOutput(backend_audio_enabled);
		BootPhase(m_Logger, "BT3", "audio... OK");
	}
	else
	{
		BootPhase(m_Logger, "BT3", "audio... OFF");
	}

	if (InitializeSettingsStorage())
	{
		const u64 fat_start_us = CTimer::GetClockTicks64();
		if (MountSettingsFileSystem("BT5"))
		{
			LoadSettingsFromStorage();
		}
		m_BootFatDeferredMs = (unsigned) ((CTimer::GetClockTicks64() - fat_start_us) / 1000u);
	}
	else
	{
		BootPhaseWarn(m_Logger, "BT5", "FATFS mount... SKIP");
	}
	NormalizeSettingsForBuild();
	BootPhase(m_Logger, "BT7", "Normalize settings... OK");
	ApplyScaledScreenMode(kDefaultVideoWidth, kDefaultVideoHeight, "boot");
	if (m_BackendEnabled)
	{
		const boolean backend_has_machine = backend_runtime_is_capability_available(BACKEND_CAP_MACHINE);
		if (!kBootQuiet)
		{
			BootTrace("R1");
		}
		m_InitramfsFiles = backend_runtime_initramfs_mount_boot_minimal();
		m_InitramfsDeferredMountPending = TRUE;
		if (!kBootQuiet)
		{
			BootTrace("R2");
		}
		BootPhase(m_Logger, "BT8", "initramfs boot-minimal... OK");

		if (!kBootQuiet)
		{
			BootTrace("R3");
		}
		if (backend_has_machine)
		{
				m_RefreshRateHz = ClampRefreshRateHz(m_RefreshRateHz);
				backend_runtime_set_refresh_hz(m_RefreshRateHz);
				backend_runtime_set_disk_rom_enabled(m_DiskRomEnabled);
				backend_runtime_set_color_artifacts_enabled(m_ColorArtifactsEnabled);
				backend_runtime_set_gfx9000_enabled(m_Gfx9000Enabled);
			backend_runtime_set_boot_mode(m_BootMode);
			backend_runtime_set_model_profile(m_MachineProfile);
			backend_runtime_set_processor_mode(m_ProcessorMode);
			backend_runtime_set_rammapper_kb(m_RamMapperKb);
			backend_runtime_set_megaram_kb(m_MegaRamKb);
			backend_runtime_set_fm_music_enabled(m_FmMusicEnabled);
			backend_runtime_set_fm_layer_gain_pct(m_FmLayerGainPct);
			backend_runtime_set_scc_plus_enabled(m_SccCartEnabled);
			if (!backend_runtime_set_scc_dual_cart_enabled(m_SccDualCartEnabled))
			{
				m_SccDualCartEnabled = FALSE;
			}
		}
		BootPhase(m_Logger, "BT9", "runtime config... OK");
		{
			const u64 boot_elapsed_us = CTimer::GetClockTicks64() - m_BootStartUs;
			m_BootToBackendStartMs = (unsigned) (boot_elapsed_us / 1000u);
		}
		m_BackendReady = backend_runtime_init();
		{
			const BackendInfo backend_info = backend_runtime_get_info();
			m_EmulatorBackendName = backend_info.active_backend_name;
			m_BackendInitFailCode = (backend_info.init_error_code > 0) ? (unsigned) backend_info.init_error_code : 0u;
		}
		{
			const u64 boot_elapsed_us = CTimer::GetClockTicks64() - m_BootStartUs;
			m_BootToBackendMs = (unsigned) (boot_elapsed_us / 1000u);
		}
		if (!m_BackendReady)
		{
			m_BackendLastError = (int) (m_BackendInitFailCode != 0u ? m_BackendInitFailCode : 1u);
			if (!kBootQuiet)
			{
				BootTrace("R4E");
			}
			CString line;
			line.Format("%s bootstrap failed (init code=%u)",
				m_EmulatorBackendName != 0 ? m_EmulatorBackendName : "backend",
				m_BackendInitFailCode);
			m_Logger.Write(FromKernel, LogWarning, line);
			BootPhaseWarn(m_Logger, "BT10", "emulator backend... FAIL");
		}
		else
		{
			if (!kBootQuiet)
			{
				BootTrace("R4O");
			}
			CString line;
			line.Format("%s bootstrap OK (frame-step)",
				m_EmulatorBackendName != 0 ? m_EmulatorBackendName : "backend");
			m_Logger.Write(FromKernel, LogNotice, line);
			BootPhase(m_Logger, "BT10", "emulator backend... OK");
		}
	}
	InitializeDebugRuntime();
	if (SMSBARE_ENABLE_DEBUG_OVERLAY && m_DebugOverlayUnlocked && DebugOverlayModeEnabled(m_DebugOverlayMode))
	{
		if (!kBootQuiet)
		{
			WriteScreenText("\x1b[2J\x1b[H\x1b[0m\x1b[?25l");
		}
	}
	else
	{
		WriteScreenText("\x1b[?25l");
	}
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	BootPhase(m_Logger, "BT12", "debug overlay... READY");
	m_Logger.Write(FromKernel, LogNotice, "Overlay: F11 | Pause: F12");
#if SMSBARE_V9990_UART_TRACE
	if (m_Gfx9000AutoCfgApplied)
	{
		smsbare_uart_write("G9E cart-auto ON src:cfg\n");
	}
#endif
#else
	BootPhase(m_Logger, "BT12", "debug overlay... OFF");
#endif
	if (m_BackendReady && backend_runtime_is_capability_available(BACKEND_CAP_MACHINE))
	{
		m_FmLayerGainPct = backend_runtime_get_fm_layer_gain_pct();
	}
	m_PauseMenu.SetLanguage(m_Language);
	m_PauseMenu.SetOverscanEnabled(m_OverscanEnabled);
	m_PauseMenu.SetScanlineMode(m_ScanlineMode);
	m_PauseMenu.SetColorArtifactsEnabled(m_ColorArtifactsEnabled);
	m_PauseMenu.SetGfx9000Enabled(m_Gfx9000Enabled);
	m_RefreshRateHz = 60u;
	m_PauseMenu.SetDiskRomEnabled(m_DiskRomEnabled);
	m_PauseMenu.SetCoreName(CurrentCoreName());
	m_PauseMenu.SetBootMode(m_BootMode);
	m_PauseMenu.SetMachineProfile(m_MachineProfile);
	m_PauseMenu.SetProcessorMode(m_ProcessorMode);
	m_PauseMenu.SetAutofireEnabled(m_AutofireEnabled);
	m_PauseMenu.SetRamMapperKb(m_RamMapperKb);
	m_PauseMenu.SetMegaRamKb(m_MegaRamKb);
	{
		u16 joy_codes[CUsbHidGamepad::MapSlotCount];
		m_UsbGamepad.GetMappingCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
		m_PauseMenu.SetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
	}
	m_PauseMenu.SetFmMusicEnabled(m_FmMusicEnabled);
	m_PauseMenu.SetSccCartEnabled(m_SccCartEnabled);
	m_PauseMenu.SetSccDualCartState(m_SccDualCartEnabled,
		backend_runtime_get_scc_dual_cart_available()
	);
	m_PauseMenu.SetAudioGainPercent(m_AudioOutputGainPct);
	SetUartTelemetryEnabled(m_UartTelemetryEnabled);
		m_Logger.Write(FromKernel, LogNotice, "Boot step: apply runtime");
		(void) ApplyGfx9000AutoForActiveCartridge("boot-pre-runtime", FALSE);
		ApplyRuntimeSettingsToBackend();
		m_Logger.Write(FromKernel, LogNotice, "Boot step: runtime OK");
		/* Force first visible frame to use current viewport policy before USB init blocks startup. */
		if (m_BackendReady)
		{
			m_Logger.Write(FromKernel, LogNotice, "Boot step: first blit");
			BlitBackendFrame(TRUE);
			(void) m_Screen.PresentIfDirty(m_DCacheEnabled, TRUE);
			m_BootFirstBlitMs = (unsigned) ((CTimer::GetClockTicks64() - m_BootStartUs) / 1000u);
#if SMSBARE_ENABLE_DEBUG_OVERLAY
			{
				CString line;
				line.Format("BOOT_TIME start_to_first_blit_ms=%u", m_BootFirstBlitMs);
				m_Logger.Write(FromKernel, LogNotice, line);
			}
#endif
			m_Logger.Write(FromKernel, LogNotice, "Boot step: blit OK");
		}
		if (!m_UsbReady)
		{
			m_Logger.Write(FromKernel, LogNotice, "Boot step: init USB");
			if (m_USBHCI.Initialize())
			{
				m_UsbReady = TRUE;
				m_BootUsbReadyMs = (unsigned) ((CTimer::GetClockTicks64() - m_BootStartUs) / 1000u);
				(void) DebugRepairTimerIrqBinding(0x90u);
				m_Logger.Write(FromKernel, LogNotice, "USB host ready");
				m_Logger.Write(FromKernel, LogNotice, "Attach USB keyboard/gamepad");
#if SMSBARE_ENABLE_DEBUG_OVERLAY
				{
					CString line;
					line.Format("BOOT_TIME start_to_usb_ready_ms=%u", m_BootUsbReadyMs);
					m_Logger.Write(FromKernel, LogNotice, line);
				}
#endif
				BootPhase(m_Logger, "BT13", "USB Host... OK");
			}
			else
			{
				m_Logger.Write(FromKernel, LogWarning, "USB host init failed; retrying");
				m_UsbInitRetryTick = 300u;
#if SMSBARE_ENABLE_DEBUG_OVERLAY
				{
					CString line;
					line.Format("BOOT_TIME usb_retry_scheduled_tick=%u", m_UsbInitRetryTick);
					m_Logger.Write(FromKernel, LogNotice, line);
					line.Format("BOOT_TIME start_to_usb_retry_scheduled_ms=%u",
						(unsigned) ((CTimer::GetClockTicks64() - m_BootStartUs) / 1000u));
					m_Logger.Write(FromKernel, LogNotice, line);
				}
#endif
				BootPhaseWarn(m_Logger, "BT13", "USB Host... RETRY");
			}
		}

	m_Logger.Write(FromKernel, LogNotice, "Boot step: main loop");
	BootPhase(m_Logger, "BT14", "main loop... OK");
	m_BootBt14Ms = (unsigned) ((CTimer::GetClockTicks64() - m_BootStartUs) / 1000u);
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	{
		CString line;
		line.Format("BOOT_TIME start_to_bt14_ms=%u", m_BootBt14Ms);
		m_Logger.Write(FromKernel, LogNotice, line);
	}
#endif
	u64 next_frame_us = CTimer::GetClockTicks64();
	for (unsigned tick = 0; ; ++tick)
	{
		u64 now_us = CTimer::GetClockTicks64();
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: loop top");
			BackendGfx9000KernelTrace("G9K: top flush begin");
			FlushBackendUartTraceBufferTraceWindow();
			BackendGfx9000KernelTrace("G9K: top flush end");
		}
		if (m_FramePacingResetPending)
		{
			next_frame_us = now_us + m_FrameIntervalUs;
			m_FramePacingResetPending = FALSE;
		}
		m_CPUThrottle.Update();
		if (!m_AudioInitAttempted && (m_TestToneEnabled || backend_audio_enabled))
		{
			if (m_TestToneEnabled || !m_BackendEnabled || !m_BackendReady || m_BackendFrames >= 2u)
			{
				InitializeAudioOutput(backend_audio_enabled);
			}
		}
		if (!m_UsbReady)
		{
			if (tick >= m_UsbInitRetryTick)
			{
				if (m_USBHCI.Initialize())
				{
					m_UsbReady = TRUE;
					m_BootUsbReadyMs = (unsigned) ((CTimer::GetClockTicks64() - m_BootStartUs) / 1000u);
					(void) DebugRepairTimerIrqBinding(0x91u);
					m_Logger.Write(FromKernel, LogNotice, "USB host ready");
					m_Logger.Write(FromKernel, LogNotice, "Attach USB keyboard/gamepad");
#if SMSBARE_ENABLE_DEBUG_OVERLAY
					{
						CString line;
						line.Format("BOOT_TIME start_to_usb_ready_ms=%u", m_BootUsbReadyMs);
						m_Logger.Write(FromKernel, LogNotice, line);
					}
#endif
					(void) ApplyGfx9000AutoForActiveCartridge("usb-retry", TRUE);
				}
				else
				{
					/* Retry later without blocking the emulation startup path. */
					m_UsbInitRetryTick = tick + 300u;
				}
			}
		}

		if (!m_EmulationPaused && m_BackendReady && m_UsbReady)
		{
			BackendGfx9000KernelTrace("G9K: usb begin");
		}
		const boolean updated = m_UsbReady ? m_USBHCI.UpdatePlugAndPlay() : FALSE;
		if (!m_EmulationPaused && m_BackendReady && m_UsbReady)
		{
			BackendGfx9000KernelTrace("G9K: usb end");
		}
		if (updated)
		{
			m_PlugAndPlayCount++;
		}

		if (updated)
		{
			if (m_UsbGamepad.AttachIfAvailable(m_DeviceNameService, m_Logger))
			{
				u16 joy_codes[CUsbHidGamepad::MapSlotCount];
				m_UsbGamepad.GetMappingCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
				m_PauseMenu.SetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
			}
			(void) m_UsbKeyboard.AttachIfAvailable(m_DeviceNameService, m_Logger, KeyPressedHandler);
		}

		if (m_UsbGamepad.ConsumeRemovedEvent())
		{
			m_Logger.Write(FromKernel, LogNotice, "USB gamepad removed");
		}

		if (m_UsbKeyboard.ConsumeRemovedEvent())
		{
			m_LastKeyboardModifiers = 0u;
			m_RuntimeKeyboardModifiers = 0u;
			for (unsigned i = 0u; i < 6u; ++i)
			{
				m_LastKeyboardRawKeys[i] = 0u;
				m_RuntimeKeyboardRawKeys[i] = 0u;
			}
			m_LastPausePressed = FALSE;
			m_LastMenuBackPressed = FALSE;
			if (m_BackendReady)
			{
				static const unsigned char kEmptyKeyboard[6] = {0u, 0u, 0u, 0u, 0u, 0u};
				backend_runtime_input_push_keyboard(0u, kEmptyKeyboard);
				m_RuntimeKeyboardDispatchValid = TRUE;
				m_RuntimeKeyboardReplayBudget = 0u;
				m_RuntimeKeyboardDispatchedModifiers = 0u;
				for (unsigned i = 0u; i < 6u; ++i)
				{
					m_RuntimeKeyboardDispatchedRawKeys[i] = 0u;
				}
			}
			else
			{
				m_RuntimeKeyboardDispatchValid = FALSE;
				m_RuntimeKeyboardReplayBudget = 0u;
			}
			m_Logger.Write(FromKernel, LogNotice, "USB keyboard removed");
		}

		if (m_UsbKeyboard.IsAttached())
		{
			unsigned char pending_modifiers = 0u;
			unsigned char pending_raw[6] = {0u, 0u, 0u, 0u, 0u, 0u};
			boolean consumed_report = FALSE;
			while (m_UsbKeyboard.ConsumeLatest(&pending_modifiers, pending_raw))
			{
				UpdateJoystickFromRaw(pending_modifiers, pending_raw);
				consumed_report = TRUE;
			}
			m_UsbKeyboard.UpdateLEDs();
			/* Keep menu navigation responsive while a key is held (auto-repeat driven by frame loop). */
			if (!consumed_report)
			{
				UpdateJoystickFromRaw(m_LastKeyboardModifiers, m_LastKeyboardRawKeys);
			}
		}
		else if (m_EmulationPaused)
		{
			UpdateJoystickFromRaw(m_LastKeyboardModifiers, m_LastKeyboardRawKeys);
		}

		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: deferred begin");
		}
		ProcessDeferredBootTasks();
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: deferred end");
		}

		if (m_SettingsSavePending)
		{
			m_SettingsSavePending = FALSE;
			SaveSettingsToStorage();
			m_PauseMenu.ReturnToRoot();
			m_LastPauseWasInSettings = FALSE;
			m_PauseMenu.RequestRedraw();
		}
		if (m_StateSavePending)
		{
			m_StateSavePending = FALSE;
			SaveEmulatorStateToStorage();
		}
		if (m_StateLoadPending)
		{
			m_StateLoadPending = FALSE;
			LoadEmulatorStateFromStorage();
		}
		if (m_PendingMenuAction != (unsigned) ComboMenuActionNone)
		{
			const TComboMenuAction deferred_action = (TComboMenuAction) m_PendingMenuAction;
			m_PendingMenuAction = (unsigned) ComboMenuActionNone;
			HandleMenuAction(deferred_action);
		}
		if (m_CoreRebootRequested)
		{
			return ComboShutdownReboot;
		}
		ProcessPendingMenuHardReset();
		SyncRefreshRateStateFromRuntime();
		if (SMSBARE_ENABLE_DEBUG_OVERLAY && !m_DebugOverlayUnlocked && m_BackendReady)
		{
			const unsigned load_diag = backend_runtime_get_load_diag();
			/* Unlock debug only after SMS backend() has completed in runtime init path. */
			if (load_diag >= 2102u && load_diag < 9000u)
			{
					m_DebugOverlayUnlocked = TRUE;
					m_PauseMenu.RequestRedraw();
				m_Logger.Write(FromKernel, LogNotice, "Debug overlay unlocked (post-SMS backend)");
			}
		}

		/* Hybrid wait: coarse sleep first, then short busy delay for tighter frame deadline. */
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: pace begin");
		}
		if (m_BackendReady && !m_EmulationPaused)
		{
			now_us = CTimer::GetClockTicks64();
			if (next_frame_us > now_us + kFramePacingGuardUs)
			{
				const u64 slack_us = next_frame_us - now_us;
				if (slack_us > kFramePacingSleepTargetUs + kFramePacingGuardUs)
				{
					m_Scheduler.usSleep((unsigned) (slack_us - kFramePacingSleepTargetUs));
				}
				now_us = CTimer::GetClockTicks64();
				const u64 fine_slack_us = next_frame_us > now_us ? (next_frame_us - now_us) : 0ull;
				if (fine_slack_us > kFramePacingSpinThresholdUs + kFramePacingGuardUs)
				{
					CTimer::SimpleusDelay((unsigned) (fine_slack_us - kFramePacingGuardUs));
				}
			}
		}
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: pace end");
		}

		if (m_BackendReady && !m_EmulationPaused)
		{
			BackendGfx9000KernelTrace("G9K: step gate");
			now_us = CTimer::GetClockTicks64();
			boolean stepped_backend = FALSE;
			unsigned catchup_steps = 0;
			/*
			 * SMS gameplay is more sensitive to visible frame skips than to a
			 * small slowdown. Allowing multiple backend steps before a single
			 * blit visibly drops intermediate states, which reads as "jumping"
			 * animation on hardware. Prefer one emulated frame per presented
			 * frame here.
			 */
			const unsigned max_catchup_steps = 1u;
			const boolean uart_perf_enabled = TRUE;
			while (now_us >= next_frame_us && catchup_steps < max_catchup_steps)
			{
				const u64 late_us64 = now_us - next_frame_us;
				const unsigned pacing_err_us = (unsigned) (late_us64 > 999u ? 999u : late_us64);
				if (uart_perf_enabled)
				{
					const unsigned late_us = (unsigned) (late_us64 > 9999u ? 9999u : late_us64);
					m_UartVideoLateUsLast = late_us;
					if (late_us > m_UartVideoLateUsPeak)
					{
						m_UartVideoLateUsPeak = late_us;
					}
				}
				if (m_FramePacingErrAvgUs == 0u)
				{
					m_FramePacingErrAvgUs = pacing_err_us;
				}
				else
				{
					m_FramePacingErrAvgUs = ((m_FramePacingErrAvgUs * 7u) + pacing_err_us) / 8u;
				}
				if (pacing_err_us > m_FramePacingErrMaxUs)
				{
					m_FramePacingErrMaxUs = pacing_err_us;
				}

				u8 joy_bits_runtime = ResolveRuntimeJoystickBits();
				PushKeyboardStateToRuntime();
				BackendGfx9000KernelTrace("G9K: input key end");
				backend_runtime_input_push_joystick(joy_bits_runtime);
				BackendGfx9000KernelTrace("G9K: input joy end");
				BackendGfx9000KernelTrace("G9K: step call");
				FlushBackendUartTraceBufferTraceWindow();
				const u64 step_start_us = uart_perf_enabled ? CTimer::GetClockTicks64() : 0u;
				if (m_Gfx9000ResumeTracePending)
				{
					if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9R: step begin"))
					{
						m_Logger.Write(FromKernel, LogNotice, "G9R: step begin");
					}
				}
				if (backend_runtime_step_frame())
				{
					BackendGfx9000KernelTrace("G9K: step ret");
					if (uart_perf_enabled)
					{
						const u64 step_elapsed_us64 = CTimer::GetClockTicks64() - step_start_us;
						const unsigned step_elapsed_us = (unsigned) (step_elapsed_us64 > 9999u ? 9999u : step_elapsed_us64);
						m_UartVideoStepUsLast = step_elapsed_us;
						if (step_elapsed_us > m_UartVideoStepUsPeak)
						{
							m_UartVideoStepUsPeak = step_elapsed_us;
						}
					}
					stepped_backend = TRUE;
					BackendGfx9000KernelTrace("G9K: status begin");
					const BackendRuntimeStatus runtime_status = backend_runtime_get_status();
					BackendGfx9000KernelTrace("G9K: status end");
					m_BackendFrames = runtime_status.frames;
					m_BackendLastError = runtime_status.last_error;
					BackendGfx9000KernelTrace("G9K: sync begin");
					SyncRefreshRateStateFromRuntime();
					BackendGfx9000KernelTrace("G9K: sync end");

					if (m_BackendFpsWindowStartUs == 0u)
					{
						m_BackendFpsWindowStartUs = now_us;
						m_BackendFpsWindowStartFrames = m_BackendFrames;
					}
					else if (now_us > m_BackendFpsWindowStartUs)
					{
						const u64 elapsed_us = now_us - m_BackendFpsWindowStartUs;
						if (elapsed_us >= 1000000ull)
						{
							const unsigned long elapsed_frames = m_BackendFrames - m_BackendFpsWindowStartFrames;
							m_BackendFpsX10 = (unsigned) (((u64) elapsed_frames * 10000000ull) / elapsed_us);
							m_BackendFpsWindowStartUs = now_us;
							m_BackendFpsWindowStartFrames = m_BackendFrames;
							m_FramePacingErrMaxUs = 0u;
						}
					}
					if (m_Gfx9000ResumeTracePending)
					{
						CString resume_line;
						resume_line.Format("G9R: step OK frame=%lu err=%u",
							(unsigned long) m_BackendFrames,
							(unsigned) m_BackendLastError);
						if (!BackendQueueKernelLogDuringGfx9000Trace(0, (const char *) resume_line))
						{
							m_Logger.Write(FromKernel, LogNotice, resume_line);
						}
					}

					if (runtime_status.last_pc == m_BackendLastPC)
					{
						if (m_BackendPcRepeatCount < 999999u)
						{
							m_BackendPcRepeatCount++;
						}
					}
					else
					{
						m_BackendLastPC = runtime_status.last_pc;
						m_BackendPcRepeatCount = 0;
						m_BackendPcLoopLogged = FALSE;
					}

					if (!m_BackendPcLoopLogged && m_BackendPcRepeatCount >= 1200u)
					{
						CString loop_line;
						loop_line.Format("Backend PC loop? PC=%04X repeat=%u", (unsigned) (m_BackendLastPC & 0xFFFF), m_BackendPcRepeatCount);
						m_Logger.Write(FromKernel, LogWarning, loop_line);
						m_BackendPcLoopLogged = TRUE;
					}

				}
				BackendGfx9000KernelTrace("G9K: flush begin");
				FlushBackendUartTraceBufferTraceWindow();
				BackendGfx9000KernelTrace("G9K: flush end");
				next_frame_us += m_FrameIntervalUs;
				catchup_steps++;
				BackendGfx9000KernelTrace("G9K: tick end");
				now_us = CTimer::GetClockTicks64();
			}
			if (uart_perf_enabled)
			{
				m_UartVideoCatchupLast = catchup_steps;
				if (catchup_steps > m_UartVideoCatchupPeak)
				{
					m_UartVideoCatchupPeak = catchup_steps;
				}
				if (catchup_steps >= max_catchup_steps && now_us >= next_frame_us)
				{
					if (m_UartVideoCatchupCapCount < 999u)
					{
						m_UartVideoCatchupCapCount++;
					}
				}
			}

				if (stepped_backend)
				{
					const boolean force_viewport_clear = m_ForceViewportFullClearNextFrame;
					m_ForceViewportFullClearNextFrame = FALSE;
					const u64 blit_start_us = uart_perf_enabled ? CTimer::GetClockTicks64() : 0u;
					BackendGfx9000KernelTrace("G9K: blit begin");
					if (m_Gfx9000ResumeTracePending)
					{
						if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9R: blit begin"))
						{
							m_Logger.Write(FromKernel, LogNotice, "G9R: blit begin");
						}
					}
					BlitBackendFrame(force_viewport_clear ? TRUE : FALSE);
					BackendGfx9000KernelTrace("G9K: blit end");
					if (m_Gfx9000ResumeTracePending)
					{
						if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9R: blit OK"))
						{
							m_Logger.Write(FromKernel, LogNotice, "G9R: blit OK");
						}
					}
					if (uart_perf_enabled)
					{
					const u64 blit_elapsed_us64 = CTimer::GetClockTicks64() - blit_start_us;
					const unsigned blit_elapsed_us = (unsigned) (blit_elapsed_us64 > 9999u ? 9999u : blit_elapsed_us64);
					m_UartVideoBlitUsLast = blit_elapsed_us;
					if (blit_elapsed_us > m_UartVideoBlitUsPeak)
					{
						m_UartVideoBlitUsPeak = blit_elapsed_us;
					}
				}
			}

			if (now_us > next_frame_us + (m_FrameIntervalUs * 12ull))
			{
				next_frame_us = now_us - (m_FrameIntervalUs * 2ull);
			}
		}

		if (m_BackendReady && m_EmulationPaused && m_PauseNeedsBackgroundRedraw)
		{
			const boolean full_viewport_clear =
				(m_PauseNeedsFullViewportClear || m_ForceViewportFullClearNextFrame) ? TRUE : FALSE;
			m_PauseNeedsBackgroundRedraw = FALSE;
			m_PauseNeedsFullViewportClear = FALSE;
			m_ForceViewportFullClearNextFrame = FALSE;
			if (full_viewport_clear)
			{
				BlitBackendFrame(TRUE);
			}
			const u16 *src_pixels = 0;
			unsigned src_w = 0u;
			unsigned src_h = 0u;
			BackendVideoPresentation presentation = {0};
			(void) backend_runtime_video_frame(&src_pixels, &src_w, &src_h);
			backend_runtime_get_video_presentation(&presentation);
				if (src_pixels != 0 && src_w > 0u && src_h > 0u)
				{
					const unsigned src_pitch =
						(presentation.pitch_pixels != 0u) ? presentation.pitch_pixels : src_w;
				unsigned source_x =
					(presentation.source_x < src_w) ? presentation.source_x : 0u;
				unsigned source_y =
					(presentation.source_y < src_h) ? presentation.source_y : 0u;
				unsigned source_w =
					(presentation.source_width != 0u && presentation.source_x < src_w)
						? presentation.source_width
						: src_w;
				unsigned source_h =
					(presentation.source_height != 0u && presentation.source_y < src_h)
						? presentation.source_height
						: src_h;
				unsigned full_snap_w = 0u;
				unsigned full_snap_h = 0u;
				unsigned crop_x = 0u;
				unsigned crop_y = 0u;
				if (source_x + source_w > src_w)
				{
					source_w = src_w - source_x;
				}
				if (source_y + source_h > src_h)
				{
					source_h = src_h - source_y;
				}
				if (source_w == 0u || source_h == 0u)
					{
						source_x = 0u;
						source_y = 0u;
						source_w = src_w;
						source_h = src_h;
					}
					ApplyScaledScreenMode(source_w, source_h, "pause");
					const unsigned screen_width = m_Screen.GetWidth();
					const unsigned screen_height = m_Screen.GetHeight();
					ComputeViewportLayout(screen_width, screen_height,
						source_w, source_h,
					&presentation,
					&m_BackendBoxX, &m_BackendBoxY, &m_BackendBoxW, &m_BackendBoxH,
					&full_snap_w, &full_snap_h,
					&crop_x, &crop_y);
				(void) combo_viewport_screenshot_capture_from_emulator(
					src_pixels + (source_y * src_pitch) + source_x, source_w, source_h,
					src_pitch,
					m_BackendBoxX, m_BackendBoxY, m_BackendBoxW, m_BackendBoxH,
					full_snap_w, full_snap_h, crop_x, crop_y,
					(presentation.disable_scanline_postfx == 0u) ? m_ScanlineMode : kScanlineModeOff);
			}
			m_PauseMenu.RequestRedraw();
		}

		if (m_Sound != 0)
		{
			const boolean uart_perf_enabled = TRUE;
			const u64 refill_start_us = uart_perf_enabled ? CTimer::GetClockTicks64() : 0u;
			RefillAudioQueue();
			if (uart_perf_enabled)
			{
				const u64 refill_elapsed_us64 = CTimer::GetClockTicks64() - refill_start_us;
				const unsigned refill_elapsed_us = (unsigned) (refill_elapsed_us64 > 9999u ? 9999u : refill_elapsed_us64);
				m_UartAudioRefillUsLast = refill_elapsed_us;
				if (refill_elapsed_us > m_UartAudioRefillUsPeak)
				{
					m_UartAudioRefillUsPeak = refill_elapsed_us;
				}
			}
			if (!m_Sound->IsActive() && ((tick % 500u) == 0u))
			{
				m_SoundStartAttempts++;
				if (m_Sound->Start())
				{
					m_Logger.Write(FromKernel, LogWarning, "HDMI sound recovered after retry");
				}
				else
				{
					m_SoundStartFailures++;
				}
			}
		}
#if SMSBARE_ENABLE_DEBUG_OVERLAY
		if (m_DebugOverlayUnlocked && DebugOverlayModeEnabled(m_DebugOverlayMode))
		{
			now_us = CTimer::GetClockTicks64();
			if (m_LastOverlayUs == 0u || now_us - m_LastOverlayUs >= kDebugOverlayIntervalUs)
			{
				DrawDebugOverlay(tick);
				m_LastOverlayUs = now_us;
			}
		}
#endif
		{
			char slot1_name[17];
			char slot2_name[17];
			char drive1_name[17];
			char drive2_name[17];
			char cassette_name[17];
			const char *slot1_src = m_BackendReady ? backend_runtime_get_cartridge_name(0u) : 0;
			const char *slot2_src = m_BackendReady ? backend_runtime_get_cartridge_name(1u) : 0;
			const char *drive1_src = m_BackendReady ? backend_runtime_get_disk_name(0u) : 0;
			const char *drive2_src = m_BackendReady ? backend_runtime_get_disk_name(1u) : 0;
			const char *cassette_src = m_BackendReady ? backend_runtime_get_cassette_name() : 0;
			FormatRomName83(slot1_src, slot1_name);
			FormatRomName83(slot2_src, slot2_name);
			FormatRomName83(drive1_src, drive1_name);
			FormatRomName83(drive2_src, drive2_name);
			FormatRomName83(cassette_src, cassette_name);
			m_PauseMenu.SetLoadRomSlotNames(slot1_name, slot2_name);
			m_PauseMenu.SetLoadDiskDriveNames(drive1_name, drive2_name);
			m_PauseMenu.SetLoadCassetteName(cassette_name);
		}
		const unsigned menu_anchor_x = (m_BackendBoxW > 0u) ? m_BackendBoxX : 0u;
		const unsigned menu_anchor_y = (m_BackendBoxH > 0u) ? m_BackendBoxY : 0u;
		const unsigned menu_anchor_w = (m_BackendBoxW > 0u) ? m_BackendBoxW : m_Screen.GetWidth();
		const unsigned menu_anchor_h = (m_BackendBoxH > 0u) ? m_BackendBoxH : m_Screen.GetHeight();
		if (!(m_PauseRenderSuppressed && !m_EmulationPaused))
		{
			if (m_Gfx9000ToggleTracePending)
			{
				if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9A: pause render begin"))
				{
					m_Logger.Write(FromKernel, LogNotice, "G9A: pause render begin");
				}
			}
			if (m_Gfx9000ResumeTracePending && !m_EmulationPaused)
			{
				if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9R: present begin"))
				{
					m_Logger.Write(FromKernel, LogNotice, "G9R: present begin");
				}
			}
			m_PauseMenu.Render(m_EmulationPaused, &m_GfxText,
				m_Screen.GetWidth(), m_Screen.GetHeight(),
				menu_anchor_x, menu_anchor_y, menu_anchor_w, menu_anchor_h);
		}
		m_Screen.Rotor(0, tick);
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: present begin");
		}
		(void) m_Screen.PresentIfDirty(m_DCacheEnabled, TRUE);
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: present end");
		}
		FlushBackendUartTraceBuffer();
		if (m_Gfx9000ToggleTracePending)
		{
			if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9A: pause present OK"))
			{
				m_Logger.Write(FromKernel, LogNotice, "G9A: pause present OK");
			}
			m_Gfx9000ToggleTracePending = FALSE;
		}
		if (m_Gfx9000ResumeTracePending && !m_EmulationPaused)
		{
			if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9R: present OK"))
			{
				m_Logger.Write(FromKernel, LogNotice, "G9R: present OK");
			}
			m_Gfx9000ResumeTracePending = FALSE;
		}
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: yield begin");
		}
		m_Scheduler.Yield();
		if (!m_EmulationPaused && m_BackendReady)
		{
			BackendGfx9000KernelTrace("G9K: yield end");
			BackendGfx9000KernelTrace("G9K: post-yield consume begin");
			BackendGfx9000KernelTraceConsume();
			BackendGfx9000KernelTrace("G9K: post-yield consume end");
			BackendGfx9000KernelTrace("G9K: post-yield flush begin");
			FlushBackendUartTraceBufferTraceWindow();
			BackendGfx9000KernelTrace("G9K: post-yield flush end");
		}
	}

	return ComboShutdownHalt;
}

void CComboKernel::InitializeAudioOutput(boolean backend_audio_enabled)
{
	m_AudioInitAttempted = TRUE;
	const char *sound_device = m_Options.GetSoundDevice();
	const unsigned sound_option = m_Options.GetSoundOption();
	const boolean force_av_pwm = strcmp(sound_device, "sndpwm") == 0
		|| (strcmp(sound_device, "sndvchiq") == 0 && sound_option == 1u)
		? TRUE : FALSE;
	const boolean force_hdmi = strcmp(sound_device, "sndhdmi") == 0
		|| (strcmp(sound_device, "sndvchiq") == 0 && sound_option == 2u)
		? TRUE : FALSE;
	const boolean allow_av_fallback = force_hdmi ? FALSE : TRUE;
	boolean use_av_pwm = force_av_pwm;
	boolean started = FALSE;

	for (unsigned attempt = 0u; attempt < 2u && !started; ++attempt)
	{
		m_Sound = use_av_pwm
			? (CSoundBaseDevice *) new CPWMSoundBaseDevice(&m_Interrupt, kSampleRate, kChunkSize)
			: (CSoundBaseDevice *) new CHDMISoundBaseDevice(&m_Interrupt, kSampleRate, kChunkSize);
		if (m_Sound == 0)
		{
			m_Logger.Write(FromKernel, LogWarning,
				use_av_pwm ? "Failed to allocate A/V sound device" : "Failed to allocate HDMI sound device");
		}
		else if (!m_Sound->AllocateQueue(kQueueMs))
		{
			m_Logger.Write(FromKernel, LogWarning,
				use_av_pwm ? "Cannot allocate A/V sound queue" : "Cannot allocate HDMI sound queue");
		}
		else
		{
			m_Sound->SetWriteFormat(SoundFormatSigned16, 2);
			RefillAudioQueue();

			m_SoundStartAttempts++;
			if (m_Sound->Start())
			{
				started = TRUE;
				break;
			}
			m_SoundStartFailures++;
			m_Logger.Write(FromKernel, LogWarning,
				use_av_pwm ? "Cannot start A/V sound" : "Cannot start HDMI sound");
		}

		delete m_Sound;
		m_Sound = 0;
		if (use_av_pwm || !allow_av_fallback)
		{
			break;
		}
		use_av_pwm = TRUE;
		m_Logger.Write(FromKernel, LogWarning, "Audio fallback: HDMI unavailable, trying A/V PWM");
	}

	if (!started)
	{
		m_Logger.Write(FromKernel, LogPanic, "Cannot start any audio output");
	}

	if (m_TestToneEnabled)
	{
		m_Logger.Write(FromKernel, LogNotice,
			use_av_pwm ? "Test tone ON (440Hz A/V)" : "Test tone ON (440Hz HDMI)");
	}
	else if (backend_audio_enabled)
	{
		if (SMSBARE_AUDIO_BOOT_TONE_MS > 0)
		{
			m_BootToneFramesRemaining = (kSampleRate * SMSBARE_AUDIO_BOOT_TONE_MS) / 1000u;
			CString tone_line;
			tone_line.Format("Boot probe tone: %ums", SMSBARE_AUDIO_BOOT_TONE_MS);
			m_Logger.Write(FromKernel, LogNotice, tone_line);
		}
		m_Logger.Write(FromKernel, LogNotice,
			use_av_pwm ? "Emulator audio ON (A/V)" : "Emulator audio ON (HDMI)");
	}
}

void CComboKernel::RefillAudioQueue(void)
{
	if (m_Sound == 0)
	{
		return;
	}

	static const unsigned kFramesPerBatch = 256;
	static const unsigned kRefillBudgetPerCall = 2048;
	static const unsigned kQueueTargetFillPct = 75;
	static const unsigned kQueueLowWaterPct = 25;
	s16 buffer[kFramesPerBatch * 2];
	unsigned call_peak = 0;

	const unsigned queue_size = m_Sound->GetQueueSizeFrames();
	const unsigned queue_avail = m_Sound->GetQueueFramesAvail();
	unsigned free_frames = queue_size > queue_avail
		? (queue_size - queue_avail)
		: 0;
	unsigned refill_budget = kRefillBudgetPerCall;
	m_LastQueueAvail = queue_avail;
	m_LastQueueFree = free_frames;
	if (queue_avail < m_UartAudioQueueAvailMin)
	{
		m_UartAudioQueueAvailMin = queue_avail;
	}
	if (free_frames == 0u)
	{
		return;
	}

	/* Keep queue near a target fill to reduce jitter while avoiding excess latency. */
	const unsigned target_fill = (queue_size * kQueueTargetFillPct) / 100u;
	const unsigned low_water = (queue_size * kQueueLowWaterPct) / 100u;
	if (queue_avail >= target_fill)
	{
		return;
	}

	unsigned desired_write = target_fill - queue_avail;
	if (desired_write > free_frames)
	{
		desired_write = free_frames;
	}
	if (queue_avail <= low_water)
	{
		if (m_UartAudioLowWaterHits < 999u)
		{
			m_UartAudioLowWaterHits++;
		}
		/* After a hiccup, refill aggressively to recover audio continuity quickly. */
		desired_write = free_frames;
	}
	if (refill_budget < desired_write)
	{
		refill_budget = desired_write;
	}

	while (free_frames > 0 && refill_budget > 0)
	{
		unsigned write_frames = free_frames < kFramesPerBatch ? free_frames : kFramesPerBatch;
		if (write_frames > refill_budget)
		{
			write_frames = refill_budget;
		}
		unsigned out_frames = write_frames;
		if (m_EmulationPaused)
		{
			memset(buffer, 0, write_frames * 2u * sizeof(s16));
		}
		else if (m_TestToneEnabled)
		{
			GenerateSamples(buffer, write_frames);
		}
		else if (!m_BackendEnabled || !m_BackendReady)
		{
			memset(buffer, 0, write_frames * 2u * sizeof(s16));
		}
		else
		{
			int16_t mono[kFramesPerBatch];
			const unsigned got = backend_runtime_audio_pull(mono, write_frames);
			const boolean boot_probe_active = m_BootToneFramesRemaining > 0u;
			unsigned src_peak = 0;
			unsigned peak = 0;
			unsigned gain = SMSBARE_AUDIO_GAIN;
			const unsigned effective_gain_pct = AudioGainEffectivePct(m_AudioOutputGainPct);
			out_frames = write_frames;
			for (unsigned i = 0; i < got; ++i)
			{
				const int sample = (int) mono[i];
				const unsigned abs_sample = sample >= 0 ? (unsigned) sample : (unsigned) (-sample);
				if (abs_sample > src_peak)
				{
					src_peak = abs_sample;
				}
			}
			if (!SMSBARE_AUDIO_DISABLE_AUTO_GAIN && src_peak > 0u)
			{
				const unsigned auto_gain = (unsigned) (m_AudioTargetPeakRuntime / src_peak);
				if (auto_gain > gain)
				{
					gain = auto_gain;
				}
			}
			if (gain > SMSBARE_AUDIO_GAIN_MAX)
			{
				gain = SMSBARE_AUDIO_GAIN_MAX;
			}
			for (unsigned i = 0; i < write_frames; ++i)
			{
				int sample32 = 0;
				if (i < got)
				{
					sample32 = (int) mono[i];
				}
				sample32 *= (int) gain;
				if (boot_probe_active && m_BootToneFramesRemaining > 0u)
				{
					m_AudioPhase += m_AudioStep;
					const int probe = (m_AudioPhase & 0x80000000u) ? SMSBARE_AUDIO_BOOT_TONE_AMP : -SMSBARE_AUDIO_BOOT_TONE_AMP;
					sample32 += probe;
					m_BootToneFramesRemaining--;
				}
				else
				{
				#if SMSBARE_AUDIO_PROBE_TONE
					m_AudioPhase += m_AudioStep;
					const int probe = (m_AudioPhase & 0x80000000u) ? 2400 : -2400;
					sample32 += probe;
				#endif
				}
				sample32 = (sample32 * (int) effective_gain_pct) / 100;
				if (effective_gain_pct == 0u)
				{
					sample32 = 0;
				}
				const int clip_max = (int) m_AudioClipMaxRuntime;
				if (sample32 > clip_max) sample32 = clip_max;
				if (sample32 < -clip_max) sample32 = -clip_max;
				const s16 sample = (s16) sample32;
				m_LastAudioSample = sample;
				const unsigned abs_sample = sample >= 0 ? (unsigned) sample : (unsigned) (-sample);
				if (abs_sample > peak) peak = abs_sample;
				buffer[(i * 2u) + 0u] = sample;
				buffer[(i * 2u) + 1u] = sample;
			}
			if (peak > call_peak) call_peak = peak;
		}
		const int bytes = (int)(out_frames * 2u * sizeof(s16));
		const int written = m_Sound->Write(buffer, (size_t)bytes);
		m_AudioWriteOps++;
		if (written != bytes)
		{
			m_AudioDropOps++;
			break;
		}
		free_frames -= out_frames;
		refill_budget -= out_frames;
	}

	if (call_peak > 0u)
	{
		m_AudioPeak = call_peak;
	}
	else if (m_AudioPeak > 64u)
	{
		m_AudioPeak -= 64u;
	}
	else
	{
		m_AudioPeak = 0u;
	}
}

void CComboKernel::PrimeAudioSilenceBeforeReset(void)
{
	if (m_Sound == 0)
	{
		return;
	}

	/* Drop any pending samples and replace with a short silence window before reset. */
	m_Sound->Flush();
	static const unsigned kSilenceFrames = 512u;
	s16 silence[kSilenceFrames * 2u];
	memset(silence, 0, sizeof(silence));
	const int bytes = (int) sizeof(silence);
	const int written = m_Sound->Write(silence, (size_t) bytes);
	if (written == bytes)
	{
		m_AudioWriteOps++;
	}
	else
	{
		m_AudioDropOps++;
	}
}

boolean CComboKernel::PerformMenuHardReset(unsigned requested_diag, unsigned success_diag, unsigned fail_diag)
{
	m_MenuHardResetPending = FALSE;
	UpdateLoadDiagTelemetry(requested_diag);
	PrimeAudioSilenceBeforeReset();
	UpdateLoadDiagTelemetry(3510u); /* shared hard reset path */
	ApplyRuntimeSettingsToBackend();
	const boolean reset_ok = backend_runtime_reset();
	if (reset_ok)
	{
		UpdateLoadDiagTelemetry(success_diag);
		ResetEmulationRuntimeState();
		ApplyRuntimeSettingsToBackend();
	}
	else
	{
		UpdateLoadDiagTelemetry(fail_diag);
	}
	return reset_ok;
}

void CComboKernel::QueueMenuHardReset(void)
{
	if (m_BackendReady)
	{
		m_MenuHardResetPending = TRUE;
	}
}

void CComboKernel::ProcessPendingMenuHardReset(void)
{
	if (!m_MenuHardResetPending || m_EmulationPaused || !m_BackendReady)
	{
		return;
	}

	m_MenuHardResetPending = FALSE;
	const boolean reset_ok = PerformMenuHardReset(3527u, 3528u, 9528u);
	if (reset_ok)
	{
		m_Logger.Write(FromKernel, LogNotice, "Menu: deferred hard reset emulator");
	}
	else
	{
		m_Logger.Write(FromKernel, LogWarning, "Menu: deferred hard reset failed");
	}
}

boolean CComboKernel::PerformMenuSoftReset(unsigned requested_diag, unsigned success_diag, unsigned fail_diag)
{
	UpdateLoadDiagTelemetry(requested_diag);
	const boolean reset_ok = backend_runtime_menu_reset();
	if (reset_ok)
	{
		UpdateLoadDiagTelemetry(success_diag);
		ResetEmulationRuntimeState();
	}
	else
	{
		UpdateLoadDiagTelemetry(fail_diag);
	}
	return reset_ok;
}

void CComboKernel::GenerateSamples(s16 *buffer, unsigned frames)
{
	const s16 amplitude = 6000;

	for (unsigned i = 0; i < frames; ++i)
	{
		m_AudioPhase += m_AudioStep;
		const s16 sample = (m_AudioPhase & 0x80000000u) ? amplitude : (s16)-amplitude;
		buffer[(i * 2u) + 0u] = sample;
		buffer[(i * 2u) + 1u] = sample;
	}
}

void CComboKernel::ResetUartPerfTelemetry(void)
{
	m_LastAudioQueueLogUs = 0u;
	m_LastVideoPacingLogUs = 0u;
	m_UartAudioQueueAvailMin = 0xFFFFFFFFu;
	m_UartAudioLowWaterHits = 0u;
	m_UartAudioRefillUsLast = 0u;
	m_UartAudioRefillUsPeak = 0u;
	m_UartVideoCatchupLast = 0u;
	m_UartVideoCatchupPeak = 0u;
	m_UartVideoCatchupCapCount = 0u;
	m_UartVideoLateUsLast = 0u;
	m_UartVideoLateUsPeak = 0u;
	m_UartVideoStepUsLast = 0u;
	m_UartVideoStepUsPeak = 0u;
	m_UartVideoBlitUsLast = 0u;
	m_UartVideoBlitUsPeak = 0u;
}

void CComboKernel::LogAudioQueueUart(void)
{
	return;
}

void CComboKernel::LogVideoPacingUart(void)
{
	return;
}

void CComboKernel::LogVideoModeTransitionUart(void)
{
	return;
}

void CComboKernel::LogVideoHiresHostDebugUart(void)
{
	return;
}

void CComboKernel::KeyPressedHandler(const char *text)
{
	if (s_Instance == 0 || text == 0)
	{
		return;
	}

	CComboKernel *self = s_Instance;
	self->m_KeyEventCount++;
	if (text[0] != '\0')
	{
		self->m_LastKeyCode = (unsigned) (u8) text[0];
	}
}

void CComboKernel::LogVdpTraceUart(void)
{
#if !SMSBARE_ENABLE_DEBUG_OVERLAY
	return;
#else
	if (!m_UartTelemetryEnabled)
	{
		return;
	}

	BackendVdpTraceView trace_view = { 0, 0u, 0u, 0u, 0u };
	auto event_tag = [](unsigned event_type) -> const char *
	{
		switch (event_type)
		{
		case BACKEND_VDP_EVENT_IE0_FIRE:      return "I0";
		case BACKEND_VDP_EVENT_IE1_ARM:       return "I1A";
		case BACKEND_VDP_EVENT_IE1_FIRE:      return "I1F";
		case BACKEND_VDP_EVENT_R19_WRITE:     return "R19";
		case BACKEND_VDP_EVENT_R23_WRITE:     return "R23";
		case BACKEND_VDP_EVENT_S1_READ:       return "S1";
		case BACKEND_VDP_EVENT_S2_READ:       return "S2";
		case BACKEND_VDP_EVENT_CMD_START:     return "CS";
		case BACKEND_VDP_EVENT_CMD_END:       return "CE";
		case BACKEND_VDP_EVENT_PORT98_WRITE:  return "98W";
		case BACKEND_VDP_EVENT_PORT99_ADDR:   return "99A";
		case BACKEND_VDP_EVENT_PORT99_REG:    return "99R";
		default:                           return "--";
		}
	};
	auto trace_window_tag = [](unsigned window) -> const char *
	{
		switch (window)
		{
		case BACKEND_VDP_TRACE_TITLE:       return "TI";
		case BACKEND_VDP_TRACE_SAVE_SELECT: return "SV";
		case BACKEND_VDP_TRACE_STARDATE:    return "SD";
		case BACKEND_VDP_TRACE_KMG:         return "KG";
		default:                         return "--";
		}
	};

	backend_runtime_get_vdp_trace_view(&trace_view);
	if (trace_view.dropped != m_BackendVdpTraceDropped)
	{
		CString drop_line;
		drop_line.Format("VDP DROP count=%u", trace_view.dropped);
		m_Logger.Write(FromKernel, LogWarning, drop_line);
		m_BackendVdpTraceDropped = trace_view.dropped;
	}
	if (trace_view.events == 0 || trace_view.count == 0u || trace_view.capacity == 0u)
	{
		return;
	}

	const unsigned start =
		(trace_view.head + trace_view.capacity - trace_view.count) % trace_view.capacity;
	for (unsigned i = 0u; i < trace_view.count; ++i)
	{
		const BackendVdpTraceEvent &event = trace_view.events[(start + i) % trace_view.capacity];
		if (event.seq <= m_BackendVdpTraceSeq)
		{
			continue;
		}
		if (event.capture_window == BACKEND_VDP_TRACE_NONE)
		{
			m_BackendVdpTraceSeq = event.seq;
			continue;
		}

		CString line;
		line.Format("VDP EVT #%lu W=%s E=%s PC=%04X SL=%u LP=%u A=%05lX R=%02X V=%02X C=%02X T=%03u D=%d S=%02X/%02X M=%u",
			(unsigned long) event.seq,
			trace_window_tag((unsigned) event.capture_window),
			event_tag((unsigned) event.event_type),
			(unsigned) event.pc,
			(unsigned) event.scanline,
			(unsigned) event.line_phase,
			(unsigned long) event.aux_addr,
			(unsigned) event.aux_reg,
			(unsigned) event.aux_value,
			(unsigned) event.aux_ctrl,
			(unsigned) event.ie1_target_line,
			(int) event.ie1_target_delta,
			(unsigned) event.status1,
			(unsigned) event.status2,
			(unsigned) event.scr_mode);
		m_Logger.Write(FromKernel, LogNotice, line);
		m_BackendVdpTraceSeq = event.seq;
	}
#endif
}

void CComboKernel::LogAudioPortTraceUart(void)
{
	return;
}

void CComboKernel::LogKmgTraceUart(void)
{
#if !SMSBARE_ENABLE_DEBUG_OVERLAY
	return;
#else
	if (!m_UartTelemetryEnabled)
	{
		return;
	}

	BackendKmgTraceView trace_view = { 0, 0u, 0u, 0u, 0u };

	backend_runtime_get_kmg_trace_view(&trace_view);
	if (trace_view.dropped != m_BackendKmgTraceDropped)
	{
		CString drop_line;
		drop_line.Format("KMG DROP count=%u", trace_view.dropped);
		m_Logger.Write(FromKernel, LogWarning, drop_line);
		m_BackendKmgTraceDropped = trace_view.dropped;
	}
	if (trace_view.events == 0 || trace_view.count == 0u || trace_view.capacity == 0u)
	{
		return;
	}

	const unsigned start =
		(trace_view.head + trace_view.capacity - trace_view.count) % trace_view.capacity;
	for (unsigned i = 0u; i < trace_view.count; ++i)
	{
		const BackendKmgTraceEvent &event = trace_view.events[(start + i) % trace_view.capacity];
		if (event.seq <= m_BackendKmgTraceSeq)
		{
			continue;
		}

		CString line;
		line.Format("KMG EVT #%lu PC=%04X A=%04X V=%02X E=%02X F=%02X M=%02X H=%02X",
			(unsigned long) event.seq,
			(unsigned) event.pc,
			(unsigned) event.addr,
			(unsigned) event.value,
			(unsigned) event.l220e,
			(unsigned) event.l220f,
			(unsigned) event.l2212,
			(unsigned) event.l2219);
		m_Logger.Write(FromKernel, LogNotice, line);
		m_BackendKmgTraceSeq = event.seq;
	}
#endif
}

void CComboKernel::DrawDebugOverlay(unsigned tick)
{
#if !SMSBARE_ENABLE_DEBUG_OVERLAY
	(void) tick;
	return;
#else
	CString line;
	BackendAudioStats audio_stats = { 0 };
	BackendTimingDebug timing_stats = { 0 };
	BackendVideoPresentation presentation = { 0 };
	auto trace_event_tag = [](unsigned event_type) -> const char *
	{
		switch (event_type)
		{
		case BACKEND_VDP_EVENT_IE0_FIRE:  return "I0";
		case BACKEND_VDP_EVENT_IE1_ARM:   return "I1A";
		case BACKEND_VDP_EVENT_IE1_FIRE:  return "I1F";
		case BACKEND_VDP_EVENT_R19_WRITE: return "R19";
		case BACKEND_VDP_EVENT_R23_WRITE: return "R23";
		case BACKEND_VDP_EVENT_S1_READ:   return "S1";
		case BACKEND_VDP_EVENT_S2_READ:   return "S2";
		case BACKEND_VDP_EVENT_CMD_START: return "CS";
		case BACKEND_VDP_EVENT_CMD_END:   return "CE";
		case BACKEND_VDP_EVENT_PORT98_WRITE: return "98W";
		case BACKEND_VDP_EVENT_PORT99_ADDR:  return "99A";
		case BACKEND_VDP_EVENT_PORT99_REG:   return "99R";
		default:                       return "--";
		}
	};
	auto trace_window_tag = [](unsigned window) -> const char *
	{
		switch (window)
		{
		case BACKEND_VDP_TRACE_TITLE:       return "TI";
		case BACKEND_VDP_TRACE_SAVE_SELECT: return "SV";
		case BACKEND_VDP_TRACE_STARDATE:    return "SD";
		default:                         return "--";
		}
	};
	auto flash_op_tag = [](unsigned op) -> const char *
	{
		switch (op)
		{
		case 1u: return "RD";
		case 2u: return "RS";
		case 3u: return "AS";
		case 4u: return "PG";
		case 5u: return "ER";
		case 6u: return "CM";
		default: return "--";
		}
	};
		unsigned load_diag = 0u;
		unsigned active_boot_mode = 0u;
		unsigned active_model_profile = 0u;
		unsigned active_processor_mode = 0u;
		const char *backend_name = m_EmulatorBackendName != 0 ? m_EmulatorBackendName : "unknown";
		backend_runtime_get_audio_stats(&audio_stats);
		backend_runtime_get_timing_debug(&timing_stats);
		load_diag = backend_runtime_get_load_diag();
		active_boot_mode = backend_runtime_get_active_boot_mode();
		active_model_profile = backend_runtime_get_active_model_profile();
		active_processor_mode = backend_runtime_get_active_processor_mode();
		backend_runtime_get_video_presentation(&presentation);

	if (ClampDebugOverlayMode(m_DebugOverlayMode) == ComboDebugOverlayMin)
	{
			const unsigned build_hash16 = BuildHash16();
			const unsigned cpu_mhz = m_CPUThrottle.GetClockRate() / 1000000u;
			const char *dfs_label = (timing_stats.reg9 & 0x02u) ? "PAL " : "NTSC";

			line.Format("TXTDBG BUILD:%04X FREQ:%4uMHz",
				build_hash16 & 0xFFFFu,
			cpu_mhz > 9999u ? 9999u : cpu_mhz);
		WriteDebugOverlayLine(1u, line);

		line.Format("DFS:%s FPS:%02u.%u F:%lu",
			dfs_label,
			(m_BackendFpsX10 / 10u) > 99u ? 99u : (m_BackendFpsX10 / 10u),
			m_BackendFpsX10 % 10u,
			m_BackendFrames);
			WriteDebugOverlayLine(2u, line);

			line.Format("MODEL: %s CPU: %s",
				DebugModelNumberLabel(active_model_profile),
				ProcessorModeLabel(active_processor_mode));
			WriteDebugOverlayLine(3u, line);
			return;
		}

	const unsigned os_tick = tick % 1000u;
	const unsigned build_hash16 = BuildHash16();
	line.Format("TXTDBG BUILD:%04X OS:%03u M:%u C:%u I:%u PAU:%u BK:%.7s R:%u K:%u S:%u H:%u/%u LD:%04u BT:%04u",
		build_hash16 & 0xFFFFu,
		os_tick,
		m_MmuEnabled ? 1u : 0u,
		m_DCacheEnabled ? 1u : 0u,
		m_ICacheEnabled ? 1u : 0u,
		m_EmulationPaused ? 1u : 0u,
		backend_name,
		m_BackendReady ? 1u : 0u,
		m_UsbKeyboard.IsAttached() ? 1u : 0u,
		m_Sound != 0 && m_Sound->IsActive() ? 1u : 0u,
		m_SoundStartAttempts > 99u ? 99u : m_SoundStartAttempts,
		m_SoundStartFailures > 99u ? 99u : m_SoundStartFailures,
		load_diag > 9999u ? 9999u : load_diag,
		m_BootToBackendMs > 9999u ? 9999u : m_BootToBackendMs);
	WriteDebugOverlayLine(1u, line);

	line.Format("AUD1 Q:%06u E:%06u O:%05u T:%05u U:%04u/%04u B:%04u/%04u P:%04u/%04u R:%04u/%04u V:%02u.%u",
		m_AudioWriteOps,
		m_AudioDropOps,
		audio_stats.overwrite_samples % 100000u,
		audio_stats.trim_samples % 100000u,
		audio_stats.short_pulls % 10000u,
		audio_stats.underrun_samples % 10000u,
		audio_stats.last_render_batch > 9999u ? 9999u : audio_stats.last_render_batch,
		audio_stats.max_render_batch > 9999u ? 9999u : audio_stats.max_render_batch,
		audio_stats.last_pull_returned > 9999u ? 9999u : audio_stats.last_pull_returned,
		audio_stats.last_pull_requested > 9999u ? 9999u : audio_stats.last_pull_requested,
		audio_stats.ring_level > 9999u ? 9999u : audio_stats.ring_level,
		audio_stats.ring_peak > 9999u ? 9999u : audio_stats.ring_peak,
		(m_BackendFpsX10 / 10u) > 99u ? 99u : (m_BackendFpsX10 / 10u),
		m_BackendFpsX10 % 10u);
	WriteDebugOverlayLine(2u, line);

	line.Format("SYS BJ:%02X MJ:%02X F:%06lu PC:%04X R:%04u L:%04u D:%03u/%04u JP:%03u/%03u SV:%02u C:%03u A:%04u CA:%u CB:%u BM:%u MP:%u RM:%4u MR:%4u IN:%03u",
		(unsigned) m_JoyBridgeBits,
		(unsigned) m_JoyMappedBits,
		m_BackendFrames,
		(unsigned) (m_BackendLastPC & 0xFFFF),
		m_BackendPcRepeatCount > 9999u ? 9999u : m_BackendPcRepeatCount,
		audio_stats.ring_level > 9999u ? 9999u : audio_stats.ring_level,
		audio_stats.last_trim_samples > 999u ? 999u : audio_stats.last_trim_samples,
		audio_stats.drop_count > 9999u ? 9999u : audio_stats.drop_count,
		m_FramePacingErrAvgUs > 999u ? 999u : m_FramePacingErrAvgUs,
		m_FramePacingErrMaxUs > 999u ? 999u : m_FramePacingErrMaxUs,
		m_SettingsSaveStage > 99u ? 99u : m_SettingsSaveStage,
		m_SettingsSaveCode > 999u ? 999u : m_SettingsSaveCode,
		m_SettingsSaveAux > 9999u ? 9999u : m_SettingsSaveAux,
		(backend_runtime_get_cartridge_name(0u) != 0) ? 1u : 0u,
		(backend_runtime_get_cartridge_name(1u) != 0) ? 1u : 0u,
		active_boot_mode > 9u ? 9u : active_boot_mode,
		active_model_profile > 9u ? 9u : active_model_profile,
		backend_runtime_get_rammapper_kb(),
		backend_runtime_get_megaram_kb(),
		m_InitramfsFiles > 999u ? 999u : m_InitramfsFiles);
	WriteDebugOverlayLine(3u, line);

	m_FmLayerGainPct = backend_runtime_get_fm_layer_gain_pct();
	line.Format("AC1 FM:%u/%u FG:%03u SCC:%u/%u AG:%u AE:%03u L1:%u L2:%u L3:%u P7:%02X MS:%05X",
		m_FmMusicEnabled ? 1u : 0u,
		backend_runtime_get_audio_layer_muted(2u) ? 0u : 1u,
		m_FmLayerGainPct > 999u ? 999u : m_FmLayerGainPct,
		m_SccCartEnabled ? 1u : 0u,
		backend_runtime_get_audio_layer_muted(3u) ? 0u : 1u,
		AudioGainUiLevelFromPercent(m_AudioOutputGainPct),
		AudioGainEffectivePct(m_AudioOutputGainPct) > 999u ? 999u : AudioGainEffectivePct(m_AudioOutputGainPct),
		backend_runtime_get_audio_layer_muted(1u) ? 1u : 0u,
		backend_runtime_get_audio_layer_muted(2u) ? 1u : 0u,
		backend_runtime_get_audio_layer_muted(3u) ? 1u : 0u,
		backend_runtime_get_psg_mixer_reg7() & 0xFFu,
		backend_runtime_get_master_switch() & 0xFFFFFu);
	WriteDebugOverlayLine(4u, line);

	line.Format("TM HP:%05u VP:%05u IP:%05u SL:%03u %s L212:%u T:%03u%% | SM:%02u R2:%02X R25:%02X R26:%02X R27:%02X HS:%03u MP:%u DP:%u EP:%u I1:%u F1:%u",
		timing_stats.h_period > 99999u ? 99999u : timing_stats.h_period,
		timing_stats.v_period > 99999u ? 99999u : timing_stats.v_period,
		timing_stats.i_period > 99999u ? 99999u : timing_stats.i_period,
		timing_stats.scanline > 999u ? 999u : timing_stats.scanline,
		(timing_stats.reg9 & 0x02u) ? "PAL " : "NTSC",
		timing_stats.lines_212 ? 1u : 0u,
		timing_stats.turbo_percent > 999u ? 999u : timing_stats.turbo_percent,
		timing_stats.scr_mode > 99u ? 99u : timing_stats.scr_mode,
		timing_stats.reg2 & 0xFFu,
		timing_stats.reg25 & 0xFFu,
		timing_stats.reg26 & 0xFFu,
		timing_stats.reg27 & 0xFFu,
		timing_stats.hscroll > 999u ? 999u : timing_stats.hscroll,
		timing_stats.multipage_enabled ? 1u : 0u,
		timing_stats.display_page > 3u ? 3u : timing_stats.display_page,
		timing_stats.render_page > 3u ? 3u : timing_stats.render_page,
		timing_stats.ie1_enabled ? 1u : 0u,
		timing_stats.ie1_flag ? 1u : 0u);
	WriteDebugOverlayLine(5u, line);

		line.Format("TM C:%X CE:%u TR:%u CS:%03u CP:%03u KB:%u/%u/%u/%u P1:%u-%u D:%u/%u S:%u | I1:%03u/%03u IA:%03u IH:%03u KJ:%u KR:%03u KW:%03u R1:%02X R7:%02X SO:%u",
			timing_stats.vdp_cmd & 0x0Fu,
			timing_stats.vdp_ce ? 1u : 0u,
			timing_stats.vdp_tr ? 1u : 0u,
			timing_stats.vdp_cmd_starts > 999u ? 999u : timing_stats.vdp_cmd_starts,
			timing_stats.vdp_copy_starts > 999u ? 999u : timing_stats.vdp_copy_starts,
			timing_stats.kanji_basic ? 1u : 0u,
			timing_stats.kanji_basic_map_ok ? 1u : 0u,
			timing_stats.kanji_basic_selected_p1 ? 1u : 0u,
			timing_stats.kanji_basic_seen_p1 ? 1u : 0u,
			timing_stats.p1_ps > 9u ? 9u : timing_stats.p1_ps,
			timing_stats.p1_ss > 9u ? 9u : timing_stats.p1_ss,
			timing_stats.disk_rom_enabled ? 1u : 0u,
			timing_stats.disk_rom_loaded ? 1u : 0u,
			timing_stats.hscroll512 ? 1u : 0u,
			timing_stats.ie1_events > 999u ? 999u : timing_stats.ie1_events,
			timing_stats.ie1_acks > 999u ? 999u : timing_stats.ie1_acks,
			timing_stats.ie1_armed > 999u ? 999u : timing_stats.ie1_armed,
			timing_stats.ie1_fired_hblank > 999u ? 999u : timing_stats.ie1_fired_hblank,
			timing_stats.kanji_available ? 1u : 0u,
			timing_stats.kanji_reads > 999u ? 999u : timing_stats.kanji_reads,
			timing_stats.kanji_addr_writes > 999u ? 999u : timing_stats.kanji_addr_writes,
			timing_stats.reg1 & 0xFFu,
			timing_stats.reg7 & 0xFFu,
			timing_stats.screen_on ? 1u : 0u);
		WriteDebugOverlayLine(6u, line);

		line.Format("TM I0:%u S0:%u HR:%u VR:%u LP:%u TL:%03u TD:%+04d EO:%u RP:%u | KQ:%03u/%03u KD:%03u KC:%03u/%03u/%03u KR:%03u KP:%03u",
			timing_stats.ie0_enabled ? 1u : 0u,
			timing_stats.vblank_flag ? 1u : 0u,
			timing_stats.status2_hr ? 1u : 0u,
			timing_stats.status2_vr ? 1u : 0u,
			timing_stats.line_phase > 9u ? 9u : timing_stats.line_phase,
			timing_stats.ie1_target_line > 999u ? 999u : timing_stats.ie1_target_line,
			timing_stats.ie1_target_delta,
			timing_stats.effective_odd_page ? 1u : 0u,
			timing_stats.render_page > 9u ? 9u : timing_stats.render_page,
			timing_stats.kbd_runtime_queued > 999u ? 999u : timing_stats.kbd_runtime_queued,
			timing_stats.kbd_runtime_high_water > 999u ? 999u : timing_stats.kbd_runtime_high_water,
			timing_stats.kbd_runtime_dropped > 999u ? 999u : timing_stats.kbd_runtime_dropped,
			timing_stats.kbd_candidate_created > 999u ? 999u : timing_stats.kbd_candidate_created,
			timing_stats.kbd_candidate_promoted > 999u ? 999u : timing_stats.kbd_candidate_promoted,
			timing_stats.kbd_candidate_discarded > 999u ? 999u : timing_stats.kbd_candidate_discarded,
			timing_stats.kbd_immediate_releases > 999u ? 999u : timing_stats.kbd_immediate_releases,
			timing_stats.kbd_immediate_gameplay_presses > 999u ? 999u : timing_stats.kbd_immediate_gameplay_presses);
		WriteDebugOverlayLine(7u, line);

		line.Format("TM 6A 9:%04u 15:%04u 18:%04u 19:%04u 25:%04u 26:%04u 27:%04u | 6B EV:%s W:%s P9:%04X SL:%03u LP:%u B:%03u/%03u R1:%02X S2:%02X SR:%X/%02X 99:%c/%u/%X/%02X/%02X",
			timing_stats.vdp_r9_writes > 9999u ? 9999u : timing_stats.vdp_r9_writes,
			timing_stats.vdp_r15_writes > 9999u ? 9999u : timing_stats.vdp_r15_writes,
			timing_stats.vdp_r18_writes > 9999u ? 9999u : timing_stats.vdp_r18_writes,
			timing_stats.vdp_r19_writes > 9999u ? 9999u : timing_stats.vdp_r19_writes,
			timing_stats.vdp_r25_writes > 9999u ? 9999u : timing_stats.vdp_r25_writes,
			timing_stats.vdp_r26_writes > 9999u ? 9999u : timing_stats.vdp_r26_writes,
			timing_stats.vdp_r27_writes > 9999u ? 9999u : timing_stats.vdp_r27_writes,
			trace_event_tag(timing_stats.trace_last_event_type),
			trace_window_tag(timing_stats.trace_last_window),
			timing_stats.last_port99_pc & 0xFFFFu,
			timing_stats.trace_last_scanline > 999u ? 999u : timing_stats.trace_last_scanline,
			timing_stats.trace_last_line_phase > 9u ? 9u : timing_stats.trace_last_line_phase,
			timing_stats.trace_last_bank_p1_lo > 999u ? 999u : timing_stats.trace_last_bank_p1_lo,
			timing_stats.trace_last_bank_p2_lo > 999u ? 999u : timing_stats.trace_last_bank_p2_lo,
			timing_stats.trace_last_reg1 & 0xFFu,
			timing_stats.trace_last_status2 & 0xFFu,
			timing_stats.last_status_read_reg & 0x0Fu,
			timing_stats.last_status_read_value & 0xFFu,
			Port99OpTag(timing_stats.last_port99_op),
			timing_stats.last_port99_vkey ? 1u : 0u,
			timing_stats.last_port99_r15 & 0x0Fu,
			timing_stats.last_port99_alatch & 0xFFu,
			timing_stats.last_port99_value & 0xFFu);
		WriteDebugOverlayLine(8u, line);

			line.Format("TM 6C A8:%02X EW:%u C:%u T:%02X/%u B:%02X/%02X/%02X/%02X F:%s S:%u M:%u A:%04X O:%05X V:%02X/%02X | 6D R17:%02X 9B:%02X>%02X>%02X V:%02X S:%02X/%02X P9:%04u",
				timing_stats.slot_reg & 0xFFu,
				timing_stats.p1_write_enabled ? 1u : 0u,
			timing_stats.cart_slot_p1 & 0xFFu,
			timing_stats.cart_type_p1 & 0xFFu,
			timing_stats.cart_type_source_p1 > 9u ? 9u : timing_stats.cart_type_source_p1,
			timing_stats.cart_bank_p1_lo & 0xFFu,
			timing_stats.cart_bank_p1_hi & 0xFFu,
			timing_stats.cart_bank_p2_lo & 0xFFu,
			timing_stats.cart_bank_p2_hi & 0xFFu,
			flash_op_tag(timing_stats.flash_last_op),
			timing_stats.flash_last_state > 9u ? 9u : timing_stats.flash_last_state,
				timing_stats.flash_last_mode > 9u ? 9u : timing_stats.flash_last_mode,
				timing_stats.flash_last_addr & 0xFFFFu,
				timing_stats.flash_last_offset > 0xFFFFFu ? 0xFFFFFu : timing_stats.flash_last_offset,
				timing_stats.flash_last_value & 0xFFu,
				timing_stats.flash_last_read_value & 0xFFu,
				timing_stats.reg17 & 0xFFu,
				timing_stats.vdp_port9b_last_reg17_before & 0xFFu,
				timing_stats.vdp_port9b_last_target & 0xFFu,
				timing_stats.vdp_port9b_last_reg17_after & 0xFFu,
				timing_stats.vdp_port9b_last_value & 0xFFu,
				timing_stats.status8 & 0xFFu,
				timing_stats.status9 & 0xFFu,
				timing_stats.vdp_port9b_writes > 9999u ? 9999u : timing_stats.vdp_port9b_writes);
			WriteDebugOverlayLine(9u, line);

		WriteScreenText("\x1b[0m");
#endif
}

void CComboKernel::ComputeViewportBox(unsigned src_width, unsigned src_height,
									const BackendVideoPresentation *presentation,
									unsigned *out_x, unsigned *out_y,
									unsigned *out_w, unsigned *out_h) const
{
	unsigned box_x = 0u;
	unsigned box_y = 0u;
	unsigned box_w = m_Screen.GetWidth();
	unsigned box_h = m_Screen.GetHeight();
	const unsigned screen_width = m_Screen.GetWidth();
	const unsigned screen_height = m_Screen.GetHeight();

	if (screen_width > 0u && screen_height > 0u)
	{
		ComputeViewportLayout(screen_width, screen_height,
			src_width, src_height,
			presentation,
			&box_x, &box_y, &box_w, &box_h,
			0, 0, 0, 0);
	}

	if (out_x != 0) *out_x = box_x;
	if (out_y != 0) *out_y = box_y;
	if (out_w != 0) *out_w = box_w;
	if (out_h != 0) *out_h = box_h;
}

void CComboKernel::ResolveViewportBox(unsigned src_width, unsigned src_height,
									const BackendVideoPresentation *presentation,
									unsigned *out_x, unsigned *out_y,
									unsigned *out_w, unsigned *out_h)
{
	ComputeViewportBox(src_width, src_height, presentation, out_x, out_y, out_w, out_h);
}

void CComboKernel::ClearPresentationSurfaces(void)
{
	if (!m_ScreenReady)
	{
		return;
	}

	const unsigned screen_width = m_Screen.GetWidth();
	const unsigned screen_height = m_Screen.GetHeight();
	if (screen_width == 0u || screen_height == 0u)
	{
		return;
	}

	CBcmFrameBuffer *frame_buffer = m_Screen.GetFrameBuffer();
	if (frame_buffer == 0 || frame_buffer->GetDepth() != 16)
	{
		FillRectScreen(&m_Screen, 0u, 0u, screen_width, screen_height, (TScreenColor) kLetterboxColor565);
		return;
	}

	m_Screen.ClearAllPages((TScreenColor) kLetterboxColor565, m_DCacheEnabled);
	m_Screen.ClearDirty();
}

void CComboKernel::BlitBackendFrame(boolean force_redraw)
{
	const u16 *pixels = 0;
	unsigned width = 0;
	unsigned height = 0;
	BackendVideoPresentation presentation = { 0 };
	const unsigned sequence = backend_runtime_video_frame(&pixels, &width, &height);
	backend_runtime_get_video_presentation(&presentation);

	if ((!force_redraw && sequence == m_BackendVideoSeq) || pixels == 0 || width == 0 || height == 0)
	{
		return;
	}

	const unsigned src_pitch = (presentation.pitch_pixels != 0u) ? presentation.pitch_pixels : width;
	unsigned src_x0 =
		(presentation.source_x < width) ? presentation.source_x : 0u;
	unsigned src_y0 =
		(presentation.source_y < height) ? presentation.source_y : 0u;
	unsigned src_w =
		(presentation.source_width != 0u && presentation.source_x < width)
			? presentation.source_width
			: width;
	unsigned src_h =
		(presentation.source_height != 0u && presentation.source_y < height)
			? presentation.source_height
			: height;
	unsigned dst_width = 0u;
	unsigned dst_height = 0u;
	unsigned full_dst_width = 0u;
	unsigned full_dst_height = 0u;
	unsigned crop_x = 0u;
	unsigned crop_y = 0u;
	if (src_x0 + src_w > width)
	{
		src_w = width - src_x0;
	}
	if (src_y0 + src_h > height)
	{
		src_h = height - src_y0;
	}
	if (src_w == 0u || src_h == 0u)
	{
		src_x0 = 0u;
		src_y0 = 0u;
		src_w = width;
		src_h = height;
	}
	ApplyScaledScreenMode(src_w, src_h, "video");
	m_BackendVideoSeq = sequence;
	const unsigned screen_width = m_Screen.GetWidth();
	const unsigned screen_height = m_Screen.GetHeight();
	const boolean woven_hires =
		(presentation.woven_hires != 0u)
		&& (presentation.logical_height > 0u)
		&& (src_h == (presentation.logical_height << 1));
	const unsigned logical_src_height =
		woven_hires ? presentation.logical_height : src_h;

	const unsigned scanline_mode =
		((presentation.disable_scanline_postfx == 0u) && (m_ScanlineMode != kScanlineModeOff))
			? kScanlineModeLcd
			: kScanlineModeOff;
	unsigned offset_x = 0u;
	unsigned offset_y = 0u;
	ComputeViewportLayout(screen_width, screen_height,
		src_w, src_h,
		&presentation,
		&offset_x, &offset_y, &dst_width, &dst_height,
		&full_dst_width, &full_dst_height,
		&crop_x, &crop_y);
#if SMSBARE_ENABLE_DEBUG_OVERLAY && SMSBARE_V9990_UART_TRACE
	if (g_backend_gfx9000_present_snapshot_budget != 0u)
	{
		CString line;
		line.Format("G9D vp src:%ux%u log:%ux%u pitch:%u woven:%u fld:%u dis:%u dst:%ux%u full:%ux%u crop:%u,%u scan:%u",
			src_w, src_h,
			presentation.logical_width, presentation.logical_height,
			presentation.pitch_pixels,
			presentation.woven_hires ? 1u : 0u,
			presentation.field_parity_odd ? 1u : 0u,
			presentation.disable_scanline_postfx ? 1u : 0u,
			dst_width, dst_height,
			full_dst_width, full_dst_height,
			crop_x, crop_y,
			scanline_mode);
		smsbare_uart_write((const char *) line);
		smsbare_uart_write("\n");
		--g_backend_gfx9000_present_snapshot_budget;
	}
#endif
	const boolean hires_mode = width >= 512u ? TRUE : FALSE;
	const unsigned old_box_x = m_BackendBoxX;
	const unsigned old_box_y = m_BackendBoxY;
	const unsigned old_box_w = m_BackendBoxW;
	const unsigned old_box_h = m_BackendBoxH;
	const boolean box_changed = (old_box_x != offset_x)
						  || (old_box_y != offset_y)
						  || (old_box_w != dst_width)
						  || (old_box_h != dst_height);
	m_BackendBoxX = offset_x;
	m_BackendBoxY = offset_y;
	m_BackendBoxW = dst_width;
	m_BackendBoxH = dst_height;
	m_BackendFallbackPattern = FALSE;

	CBcmFrameBuffer *frame_buffer = m_Screen.GetFrameBuffer();
	if (frame_buffer != 0 && frame_buffer->GetDepth() == 16)
	{
		u16 *dst_base = m_Screen.GetDrawBuffer16();
		const unsigned pitch_pixels = m_Screen.GetPitchPixels();
		const unsigned scanline_mode =
			((presentation.disable_scanline_postfx == 0u) && (m_ScanlineMode != kScanlineModeOff))
				? kScanlineModeLcd
				: kScanlineModeOff;
		u16 *present_base = dst_base;
		const unsigned present_pitch_pixels = pitch_pixels;
		const unsigned present_offset_pixels = (offset_y * pitch_pixels) + offset_x;
		if (force_redraw)
		{
			FillRect565(dst_base, pitch_pixels, screen_width, screen_height,
				0u, 0u, screen_width, screen_height, kLetterboxColor565);
			m_Screen.MarkDirtyRect(0u, 0u, screen_width, screen_height);
		}
		if (box_changed && old_box_w > 0u && old_box_h > 0u)
		{
			const unsigned old_x1 = old_box_x + old_box_w;
			const unsigned old_y1 = old_box_y + old_box_h;
			const unsigned new_x1 = offset_x + dst_width;
			const unsigned new_y1 = offset_y + dst_height;
			const unsigned ix0 = UMax(old_box_x, offset_x);
			const unsigned iy0 = UMax(old_box_y, offset_y);
			const unsigned ix1 = UMin(old_x1, new_x1);
			const unsigned iy1 = UMin(old_y1, new_y1);
			if (ix0 >= ix1 || iy0 >= iy1)
			{
				FillRect565(dst_base, pitch_pixels, screen_width, screen_height,
					old_box_x, old_box_y, old_box_w, old_box_h, kLetterboxColor565);
				m_Screen.MarkDirtyRect(old_box_x, old_box_y, old_box_w, old_box_h);
			}
			else
			{
				const unsigned top_h = iy0 - old_box_y;
				const unsigned bottom_h = old_y1 - iy1;
				const unsigned mid_h = iy1 - iy0;
				const unsigned left_w = ix0 - old_box_x;
				const unsigned right_w = old_x1 - ix1;

				FillRect565(dst_base, pitch_pixels, screen_width, screen_height,
					old_box_x, old_box_y, old_box_w, top_h, kLetterboxColor565);
				FillRect565(dst_base, pitch_pixels, screen_width, screen_height,
					old_box_x, iy1, old_box_w, bottom_h, kLetterboxColor565);
				FillRect565(dst_base, pitch_pixels, screen_width, screen_height,
					old_box_x, iy0, left_w, mid_h, kLetterboxColor565);
				FillRect565(dst_base, pitch_pixels, screen_width, screen_height,
					ix1, iy0, right_w, mid_h, kLetterboxColor565);
				if (top_h > 0u) m_Screen.MarkDirtyRect(old_box_x, old_box_y, old_box_w, top_h);
				if (bottom_h > 0u) m_Screen.MarkDirtyRect(old_box_x, iy1, old_box_w, bottom_h);
				if (left_w > 0u && mid_h > 0u) m_Screen.MarkDirtyRect(old_box_x, iy0, left_w, mid_h);
				if (right_w > 0u && mid_h > 0u) m_Screen.MarkDirtyRect(ix1, iy0, right_w, mid_h);
			}
		}
		const unsigned exact_scale_x =
			(src_w > 0u && full_dst_width > 0u && (full_dst_width % src_w) == 0u) ? (full_dst_width / src_w) : 0u;
		const unsigned exact_scale_y =
			(src_h > 0u && full_dst_height > 0u && (full_dst_height % src_h) == 0u) ? (full_dst_height / src_h) : 0u;
		const boolean exact_integer_physical =
			!woven_hires &&
			(scanline_mode == kScanlineModeOff)
			&& (exact_scale_x >= 2u)
			&& (exact_scale_x == exact_scale_y)
			&& (full_dst_width == (src_w * exact_scale_x))
			&& (full_dst_height == (src_h * exact_scale_y));
		if (exact_integer_physical)
		{
			const unsigned scale = exact_scale_x;
			unsigned prev_src_y = 0xFFFFFFFFu;
			for (unsigned y = 0u; y < dst_height; ++y)
			{
				const unsigned full_y = y + crop_y;
				const unsigned src_rel_y = full_y / scale;
				const unsigned src_y = src_y0 + src_rel_y;
				u16 *dst_line =
					present_base + present_offset_pixels + (y * present_pitch_pixels);
				if (prev_src_y == src_y)
				{
					memcpy(dst_line, dst_line - present_pitch_pixels,
						dst_width * sizeof(u16));
				}
				else
				{
					const u16 *src_line = pixels + (src_y * src_pitch) + src_x0;
					BlitLineExpandNxRgb565NoDim(dst_line, src_line + (crop_x / scale), dst_width / scale + ((dst_width % scale) != 0u ? 1u : 0u), scale);
				}
				prev_src_y = src_y;
			}
			m_Screen.MarkDirtyRect(offset_x, offset_y, dst_width, dst_height);
			return;
		}
		const boolean can_use_maps = (dst_width <= kBlitMapMaxAxis) && (dst_height <= kBlitMapMaxAxis);
		if (can_use_maps)
		{
			if (woven_hires)
			{
				m_BlitMapValid = FALSE;
			}
			if (!m_BlitMapValid
			 || m_BlitMapSrcW != src_w
			 || m_BlitMapSrcH != src_h
			 || m_BlitMapDstW != dst_width
			 || m_BlitMapDstH != dst_height)
				{
					/* X/Y are intentionally scaled independently to fill the fixed framebuffer. */
					for (unsigned x = 0; x < dst_width; ++x)
					{
						m_BlitXMap[x] = (u16) (((u64) (x + crop_x) * (u64) src_w) / (u64) full_dst_width);
				}
				for (unsigned y = 0; y < dst_height; ++y)
				{
					if (woven_hires)
					{
						m_BlitYMap[y] = (u16) MapWovenHiResPhysicalY(
							y + crop_y,
							full_dst_height,
							logical_src_height,
							src_h,
							presentation.field_parity_odd);
					}
					else
					{
						m_BlitYMap[y] = (u16) (((u64) (y + crop_y) * (u64) src_h) / (u64) full_dst_height);
					}
				}
				m_BlitRunCount = 0u;
				if (dst_width > 0u)
				{
					unsigned run_start = 0u;
					unsigned run_src_x = m_BlitXMap[0];
					for (unsigned x = 1u; x <= dst_width; ++x)
					{
						if (x == dst_width || m_BlitXMap[x] != run_src_x)
						{
							m_BlitRunSrcX[m_BlitRunCount] = (u16) run_src_x;
							m_BlitRunDstX[m_BlitRunCount] = (u16) run_start;
							m_BlitRunLen[m_BlitRunCount] = (u16) (x - run_start);
							++m_BlitRunCount;
							if (x < dst_width)
							{
								run_start = x;
								run_src_x = m_BlitXMap[x];
							}
						}
					}
				}
				m_BlitMapSrcW = src_w;
				m_BlitMapSrcH = src_h;
				m_BlitMapDstW = dst_width;
				m_BlitMapDstH = dst_height;
				m_BlitMapValid = TRUE;
			}

			unsigned prev_src_rel_y = 0xFFFFFFFFu;
			unsigned prev_dim_mode = 0xFFFFFFFFu;
			boolean prev_line_valid = FALSE;
			for (unsigned y = 0; y < dst_height; ++y)
			{
				const unsigned src_rel_y = m_BlitYMap[y];
				const u16 *src_line = pixels + ((src_y0 + src_rel_y) * src_pitch) + src_x0;
				u16 *dst_line = present_base + present_offset_pixels + (y * present_pitch_pixels);
				const boolean dark_lcd = (scanline_mode == kScanlineModeLcd) && ((y & 1u) != 0u);
				const unsigned dim_mode = dark_lcd ? 1u : 0u;
				if (prev_line_valid && src_rel_y == prev_src_rel_y && dim_mode == prev_dim_mode)
				{
					memcpy(dst_line, dst_line - present_pitch_pixels, dst_width * sizeof(u16));
					continue;
				}
				if (dim_mode == 0u)
				{
					BlitMappedRunsRgb565(dst_line, src_line,
						m_BlitRunSrcX, m_BlitRunDstX, m_BlitRunLen, m_BlitRunCount,
						0u, 0u);
				}
				else
				{
					BlitMappedRunsRgb565(dst_line, src_line,
						m_BlitRunSrcX, m_BlitRunDstX, m_BlitRunLen, m_BlitRunCount,
						0u, dim_mode);
				}
				prev_src_rel_y = src_rel_y;
				prev_dim_mode = dim_mode;
				prev_line_valid = TRUE;
			}
		}
		else
		{
			unsigned prev_src_rel_y = 0xFFFFFFFFu;
			unsigned prev_dim_mode = 0xFFFFFFFFu;
			boolean prev_line_valid = FALSE;
			for (unsigned y = 0; y < dst_height; ++y)
			{
				const unsigned src_rel_y = woven_hires
					? MapWovenHiResPhysicalY(
						y + crop_y,
						full_dst_height,
						logical_src_height,
						src_h,
						presentation.field_parity_odd)
					: (unsigned) (((u64) (y + crop_y) * (u64) src_h) / (u64) full_dst_height);
				const u16 *src_line = pixels + ((src_y0 + src_rel_y) * src_pitch) + src_x0;
				u16 *dst_line = present_base + present_offset_pixels + (y * present_pitch_pixels);
				const boolean dark_lcd = (scanline_mode == kScanlineModeLcd) && ((y & 1u) != 0u);
				const unsigned dim_mode = dark_lcd ? 1u : 0u;
				if (prev_line_valid && src_rel_y == prev_src_rel_y && dim_mode == prev_dim_mode)
				{
					memcpy(dst_line, dst_line - present_pitch_pixels, dst_width * sizeof(u16));
					continue;
				}
				if (dark_lcd)
				{
						for (unsigned x = 0; x < dst_width; ++x)
						{
							const unsigned src_x = (unsigned) (((u64) (x + crop_x) * (u64) src_w) / (u64) full_dst_width);
							dst_line[x] = (u16) ((src_line[src_x] & 0xF7DEu) >> 1);
						}
				}
				else
				{
					for (unsigned x = 0; x < dst_width; ++x)
					{
						const unsigned src_x = (unsigned) (((u64) (x + crop_x) * (u64) src_w) / (u64) full_dst_width);
						dst_line[x] = src_line[src_x];
					}
				}
				prev_src_rel_y = src_rel_y;
				prev_dim_mode = dim_mode;
				prev_line_valid = TRUE;
			}
			m_BlitMapValid = FALSE;
		}
		m_Screen.MarkDirtyRect(offset_x, offset_y, dst_width, dst_height);
		return;
	}

	if (force_redraw)
	{
		FillRectScreen(&m_Screen, 0u, 0u, screen_width, screen_height, (TScreenColor) kLetterboxColor565);
	}
	if (box_changed && old_box_w > 0u && old_box_h > 0u)
	{
		const unsigned old_x1 = old_box_x + old_box_w;
		const unsigned old_y1 = old_box_y + old_box_h;
		const unsigned new_x1 = offset_x + dst_width;
		const unsigned new_y1 = offset_y + dst_height;
		const unsigned ix0 = UMax(old_box_x, offset_x);
		const unsigned iy0 = UMax(old_box_y, offset_y);
		const unsigned ix1 = UMin(old_x1, new_x1);
		const unsigned iy1 = UMin(old_y1, new_y1);
		if (ix0 >= ix1 || iy0 >= iy1)
		{
			FillRectScreen(&m_Screen, old_box_x, old_box_y, old_box_w, old_box_h, (TScreenColor) kLetterboxColor565);
		}
		else
		{
			const unsigned top_h = iy0 - old_box_y;
			const unsigned bottom_h = old_y1 - iy1;
			const unsigned mid_h = iy1 - iy0;
			const unsigned left_w = ix0 - old_box_x;
			const unsigned right_w = old_x1 - ix1;
			FillRectScreen(&m_Screen, old_box_x, old_box_y, old_box_w, top_h, (TScreenColor) kLetterboxColor565);
			FillRectScreen(&m_Screen, old_box_x, iy1, old_box_w, bottom_h, (TScreenColor) kLetterboxColor565);
			FillRectScreen(&m_Screen, old_box_x, iy0, left_w, mid_h, (TScreenColor) kLetterboxColor565);
			FillRectScreen(&m_Screen, ix1, iy0, right_w, mid_h, (TScreenColor) kLetterboxColor565);
		}
	}

	for (unsigned y = 0; y < dst_height; ++y)
	{
		const unsigned src_rel_y = (unsigned) (((u64) y * (u64) src_h) / (u64) dst_height);
		const unsigned src_y = src_y0 + src_rel_y;
		const u16 *line = pixels + (src_y * src_pitch);
		const boolean dark_lcd = (scanline_mode == kScanlineModeLcd) && ((y & 1u) != 0u);
		for (unsigned x = 0; x < dst_width; ++x)
		{
			const unsigned src_x = src_x0 + (unsigned) (((u64) x * (u64) src_w) / (u64) dst_width);
			u16 sample = line[src_x];
			if (dark_lcd)
			{
				sample = (u16) ((sample & 0xF7DEu) >> 1);
			}
			m_Screen.SetPixel(offset_x + x, offset_y + y, (TScreenColor) sample);
		}
	}

}

void CComboKernel::ResetEmulationRuntimeState(void)
{
	m_BackendFrames = 0;
	m_BackendLastPC = -1;
	m_BackendPcRepeatCount = 0;
	m_BackendPcLoopLogged = FALSE;
	m_BackendVdpTraceSeq = 0u;
	m_BackendVdpTraceDropped = 0u;
	m_BackendAudioPortTraceSeq = 0u;
	m_BackendAudioPortTraceDropped = 0u;
	m_BackendKmgTraceSeq = 0u;
	m_BackendKmgTraceDropped = 0u;
	m_RuntimeKeyboardModifiers = 0u;
	for (unsigned i = 0u; i < 6u; ++i)
	{
		m_RuntimeKeyboardRawKeys[i] = 0u;
		m_RuntimeKeyboardDispatchedRawKeys[i] = 0u;
	}
	m_RuntimeKeyboardDispatchedModifiers = 0u;
	m_RuntimeKeyboardDispatchValid = TRUE;
	m_RuntimeKeyboardReplayBudget = 0u;
	ResetUartPerfTelemetry();
	{
		static const unsigned char kEmptyKeyboard[6] = {0u, 0u, 0u, 0u, 0u, 0u};
		backend_runtime_input_push_keyboard(0u, kEmptyKeyboard);
	}
	m_EmulationPaused = FALSE;
	m_ForceViewportFullClearNextFrame = FALSE;
	m_PauseRenderSuppressed = FALSE;
	m_PauseMenu.Reset();
}

void CComboKernel::ClearRuntimeKeyboardState(boolean drain_pending_reports)
{
	m_RuntimeKeyboardModifiers = 0u;
	for (unsigned i = 0u; i < 6u; ++i)
	{
		m_RuntimeKeyboardRawKeys[i] = 0u;
		m_RuntimeKeyboardDispatchedRawKeys[i] = 0u;
	}
	m_RuntimeKeyboardDispatchedModifiers = 0u;
	m_RuntimeKeyboardDispatchValid = TRUE;
	m_RuntimeKeyboardReplayBudget = 0u;

	if (drain_pending_reports && m_UsbKeyboard.IsAttached())
	{
		unsigned char pending_modifiers = 0u;
		unsigned char pending_raw[6] = {0u, 0u, 0u, 0u, 0u, 0u};
		while (m_UsbKeyboard.ConsumeForRuntime(&pending_modifiers, pending_raw))
		{
		}
	}
	if (m_BackendReady)
	{
		static const unsigned char kEmptyKeyboard[6] = {0u, 0u, 0u, 0u, 0u, 0u};
		backend_runtime_input_push_keyboard(0u, kEmptyKeyboard);
	}
}

void CComboKernel::ExitPauseForBackendAction(void)
{
	if (!m_EmulationPaused)
	{
		return;
	}

	ClearRuntimeKeyboardState(TRUE);
	m_EmulationPaused = FALSE;
	HandleSettingsMenuClosed();
	m_PauseNeedsBackgroundRedraw = FALSE;
	m_PauseNeedsFullViewportClear = FALSE;
	m_PauseRenderSuppressed = TRUE;
	m_LastPauseWasInSettings = FALSE;
	m_PauseMenu.Reset();
	m_LastPauseToggleUs = CTimer::GetClockTicks64();
}

void CComboKernel::RefreshSettingsUiAfterAction(void)
{
	m_PauseMenu.RequestRedraw();
}

void CComboKernel::ApplyRuntimeSettingsToBackend(void)
{
	if (!m_BackendReady)
	{
		return;
	}
	if (!backend_runtime_is_capability_available(BACKEND_CAP_MACHINE))
	{
		return;
	}
	backend_runtime_set_overscan(m_OverscanEnabled);
	backend_runtime_set_disk_rom_enabled(m_DiskRomEnabled);
	backend_runtime_set_color_artifacts_enabled(m_ColorArtifactsEnabled);
	backend_runtime_set_gfx9000_enabled(m_Gfx9000Enabled);
	backend_runtime_set_boot_mode(m_BootMode);
	backend_runtime_set_model_profile(m_MachineProfile);
	backend_runtime_set_processor_mode(m_ProcessorMode);
	backend_runtime_set_rammapper_kb(m_RamMapperKb);
	backend_runtime_set_megaram_kb(m_MegaRamKb);
	backend_runtime_set_fm_music_enabled(m_FmMusicEnabled);
	backend_runtime_set_fm_layer_gain_pct(m_FmLayerGainPct);
	backend_runtime_set_scc_plus_enabled(m_SccCartEnabled);
	if (!backend_runtime_set_scc_dual_cart_enabled(m_SccDualCartEnabled))
	{
		m_SccDualCartEnabled = FALSE;
	}
	SyncRefreshRateStateFromRuntime();
}

boolean CComboKernel::ApplyGfx9000AutoForActiveCartridge(const char *source, boolean reset_backend)
{
	(void) source;
	(void) reset_backend;
	return FALSE;
}

void CComboKernel::SyncRefreshRateStateFromRuntime(void)
{
	if (!m_BackendReady || !backend_runtime_is_capability_available(BACKEND_CAP_MACHINE))
	{
		return;
	}

	const unsigned runtime_refresh_hz = ClampRefreshRateHz(backend_runtime_get_refresh_hz());
	const u64 runtime_frame_interval_us = backend_runtime_get_frame_interval_us_exact();

	if (runtime_refresh_hz != m_RefreshRateHz)
	{
		m_RefreshRateHz = runtime_refresh_hz;
		m_FrameIntervalUs = FrameIntervalFromRefreshHz(m_RefreshRateHz);
		m_FramePacingResetPending = TRUE;
	}

	if (runtime_frame_interval_us != 0u && runtime_frame_interval_us != m_FrameIntervalUs)
	{
		m_FrameIntervalUs = runtime_frame_interval_us;
		m_FramePacingResetPending = TRUE;
	}
}

void CComboKernel::UpdateSettingsSaveTelemetry(unsigned stage, unsigned code, unsigned aux)
{
	m_SettingsSaveStage = stage;
	m_SettingsSaveCode = code;
	m_SettingsSaveAux = aux;
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	if (m_ScreenReady && m_DebugOverlayUnlocked && DebugOverlayModeEnabled(m_DebugOverlayMode))
	{
		DrawDebugOverlay((unsigned) (m_BackendFrames & 0xFFFFFFFFu));
	}
#endif
}

void CComboKernel::UpdateLoadDiagTelemetry(unsigned stage)
{
	backend_runtime_set_load_diag(stage);
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	if (m_ScreenReady && m_DebugOverlayUnlocked && DebugOverlayModeEnabled(m_DebugOverlayMode))
	{
		DrawDebugOverlay((unsigned) (m_BackendFrames & 0xFFFFFFFFu));
	}
#endif
}

void CComboKernel::LoadSettingsFromStorage(void)
{
	if (!m_SettingsFatFsMounted)
	{
		return;
	}
	UpdateLoadDiagTelemetry(5101u); /* CFG stat */

	FILINFO file_info;
	const char *settings_path = kSettingsPath;
	FRESULT stat_result = f_stat(settings_path, &file_info);
	if (stat_result == FR_NO_FILE)
	{
		UpdateLoadDiagTelemetry(5102u); /* CFG not found */
		BootPhaseWarn(m_Logger, "BT6", "settings cfg... DEFAULTS");
		m_Logger.Write(FromKernel, LogNotice, "Settings file not found; using defaults");
		return;
	}
	if (stat_result != FR_OK)
	{
		UpdateLoadDiagTelemetry(5190u + (ClampFatFsCode(stat_result) % 10u)); /* CFG stat fail */
		LogSdOpResult(m_Logger, "CFG:STAT", stat_result, settings_path, TRUE);
		BootPhaseWarn(m_Logger, "BT6", "settings cfg... FALLBACK");
		m_Logger.Write(FromKernel, LogWarning, "Settings stat failed; using defaults");
		return;
	}

	FIL file;
	UpdateLoadDiagTelemetry(5201u); /* CFG open */
	const FRESULT open_result = f_open(&file, settings_path, FA_READ);
	if (open_result != FR_OK)
	{
		UpdateLoadDiagTelemetry(5290u + (ClampFatFsCode(open_result) % 10u)); /* CFG open fail */
		LogSdOpResult(m_Logger, "CFG:OPEN", open_result, settings_path, TRUE);
		BootPhaseWarn(m_Logger, "BT6", "settings cfg... FALLBACK");
		m_Logger.Write(FromKernel, LogWarning, "Settings open failed; using defaults");
		return;
	}

	char buffer[1024];
	UINT bytes_read = 0;
	UpdateLoadDiagTelemetry(5301u); /* CFG read */
	const FRESULT read_result = f_read(&file, buffer, sizeof(buffer) - 1u, &bytes_read);
	(void) f_close(&file);
	if (read_result != FR_OK || bytes_read == 0u)
	{
		UpdateLoadDiagTelemetry(5390u + (ClampFatFsCode(read_result) % 10u)); /* CFG read fail */
		if (read_result != FR_OK)
		{
			LogSdOpResult(m_Logger, "CFG:READ", read_result, settings_path, TRUE);
		}
		BootPhaseWarn(m_Logger, "BT6", "settings cfg... FALLBACK");
		m_Logger.Write(FromKernel, LogWarning, "Settings read failed; using defaults");
		return;
	}
	UpdateLoadDiagTelemetry(5302u); /* CFG read ok */
	buffer[bytes_read] = '\0';

	/* Corruption guard: only parse files written by SaveSettingsToStorage(). */
	if (strstr(buffer, "#END") == 0)
	{
		BootPhaseWarn(m_Logger, "BT6", "settings cfg... FALLBACK");
		m_Logger.Write(FromKernel, LogWarning, "Settings file invalid/corrupt; using defaults");
		return;
	}

	boolean has_backend_id = FALSE;
	boolean has_joy_map = FALSE;
	char cfg_backend_id[32];
	u16 joy_map_codes[CUsbHidGamepad::MapSlotCount];
	cfg_backend_id[0] = '\0';
	m_UsbGamepad.GetMappingCodes(joy_map_codes, CUsbHidGamepad::MapSlotCount);

	char *line = buffer;
	while (*line != '\0')
	{
		while (*line == '\r' || *line == '\n')
		{
			++line;
		}
		if (*line == '\0')
		{
			break;
		}

		char *line_end = line;
		while (*line_end != '\0' && *line_end != '\r' && *line_end != '\n')
		{
			++line_end;
		}

		char *eq = line;
		while (eq < line_end && *eq != '=')
		{
			++eq;
		}

			if (eq < line_end)
			{
				const unsigned key_len = (unsigned) (eq - line);
				const unsigned value = ParseUintValue(eq + 1);
				if (SettingsKeyEquals(line, key_len, "scale")
				 || SettingsKeyEquals(line, key_len, "scale_max"))
				{
					(void) value;
				}
				else if (SettingsKeyEquals(line, key_len, "overscan"))
				{
					m_OverscanEnabled = value ? TRUE : FALSE;
				}
				else if (SettingsKeyEquals(line, key_len, "language"))
				{
					m_Language = combo_locale_clamp_language(value);
				}
				else if (SettingsKeyEquals(line, key_len, "proportion")
				      || SettingsKeyEquals(line, key_len, "resolution"))
				{
					(void) value;
				}
					else if (SettingsKeyEquals(line, key_len, "scanline"))
					{
						m_ScanlineMode = value != 0u ? kScanlineModeLcd : kScanlineModeOff;
					}
				else if (SettingsKeyEquals(line, key_len, "color_artifacts"))
				{
					m_ColorArtifactsEnabled = value ? TRUE : FALSE;
				}
				else if (SettingsKeyEquals(line, key_len, "machine"))
				{
					m_MachineProfile = ClampMachineProfileForBuild(value);
				}
				else if (SettingsKeyEquals(line, key_len, "processor"))
				{
					m_ProcessorMode = ClampProcessorMode(value);
				}
				else if (SettingsKeyEquals(line, key_len, "fm_music")
				      || SettingsKeyEquals(line, key_len, "fm_audio")
				      || SettingsKeyEquals(line, key_len, "fm_sound"))
				{
					m_FmMusicEnabled = value ? TRUE : FALSE;
				}
				else if (SettingsKeyEquals(line, key_len, "autofire"))
				{
					m_AutofireEnabled = value ? TRUE : FALSE;
				}
				else if (SettingsKeyEquals(line, key_len, "rammapper_kb"))
				{
					m_RamMapperKb = ClampRamMapperKb(value);
				}
			else if (SettingsKeyEquals(line, key_len, "debugger"))
			{
#if SMSBARE_ENABLE_DEBUG_OVERLAY
				m_DebugOverlayMode = ClampDebugOverlayMode(value);
#else
				(void) value;
				m_DebugOverlayMode = ComboDebugOverlayOff;
#endif
			}
			else if (SettingsKeyEquals(line, key_len, "audio_mute"))
			{
				/* Backward compatibility: old audio_mute maps to gain=0. */
				if (value != 0u)
				{
					m_AudioOutputGainPct = 0u;
				}
			}
			else if (SettingsKeyEquals(line, key_len, "audio_gain"))
			{
				m_AudioOutputGainPct = (value > 100u) ? 100u : value;
			}
			else if (SettingsKeyEquals(line, key_len, "backend_id"))
			{
				ParseTextValueLine(eq + 1, line_end, cfg_backend_id, sizeof(cfg_backend_id));
				has_backend_id = TRUE;
			}
			else if (SettingsKeyEquals(line, key_len, "joy_map_up"))
			{
				joy_map_codes[CUsbHidGamepad::MapSlotUp] = CUsbHidGamepad::ClampControlCode((u16) value);
				has_joy_map = TRUE;
			}
			else if (SettingsKeyEquals(line, key_len, "joy_map_down"))
			{
				joy_map_codes[CUsbHidGamepad::MapSlotDown] = CUsbHidGamepad::ClampControlCode((u16) value);
				has_joy_map = TRUE;
			}
			else if (SettingsKeyEquals(line, key_len, "joy_map_left"))
			{
				joy_map_codes[CUsbHidGamepad::MapSlotLeft] = CUsbHidGamepad::ClampControlCode((u16) value);
				has_joy_map = TRUE;
			}
			else if (SettingsKeyEquals(line, key_len, "joy_map_right"))
			{
				joy_map_codes[CUsbHidGamepad::MapSlotRight] = CUsbHidGamepad::ClampControlCode((u16) value);
				has_joy_map = TRUE;
			}
			else if (SettingsKeyEquals(line, key_len, "joy_map_button1"))
			{
				joy_map_codes[CUsbHidGamepad::MapSlotButton1] = CUsbHidGamepad::ClampControlCode((u16) value);
				has_joy_map = TRUE;
			}
			else if (SettingsKeyEquals(line, key_len, "joy_map_button2"))
			{
				joy_map_codes[CUsbHidGamepad::MapSlotButton2] = CUsbHidGamepad::ClampControlCode((u16) value);
				has_joy_map = TRUE;
			}
		}

		line = line_end;
	}

	NormalizeSettingsForBuild();
	backend_runtime_set_requested_backend_id(has_backend_id && cfg_backend_id[0] != '\0' ? cfg_backend_id : 0);
	if (has_joy_map)
	{
		m_UsbGamepad.SetMappingCodes(joy_map_codes, CUsbHidGamepad::MapSlotCount, TRUE);
	}

	m_RefreshRateHz = ClampRefreshRateHz(m_RefreshRateHz);
	m_FrameIntervalUs = FrameIntervalFromRefreshHz(m_RefreshRateHz);
	m_PauseMenu.SetLanguage(m_Language);
	m_PauseMenu.SetOverscanEnabled(m_OverscanEnabled);
	m_PauseMenu.SetScanlineMode(m_ScanlineMode);
	m_PauseMenu.SetColorArtifactsEnabled(m_ColorArtifactsEnabled);
	m_PauseMenu.SetGfx9000Enabled(m_Gfx9000Enabled);
	m_PauseMenu.SetDiskRomEnabled(m_DiskRomEnabled);
	m_PauseMenu.SetCoreName(CurrentCoreName());
	m_PauseMenu.SetBootMode(m_BootMode);
	m_PauseMenu.SetMachineProfile(m_MachineProfile);
	m_PauseMenu.SetProcessorMode(m_ProcessorMode);
	m_PauseMenu.SetAutofireEnabled(m_AutofireEnabled);
	m_PauseMenu.SetRamMapperKb(m_RamMapperKb);
	m_PauseMenu.SetMegaRamKb(m_MegaRamKb);
	{
		u16 joy_codes[CUsbHidGamepad::MapSlotCount];
		m_UsbGamepad.GetMappingCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
		m_PauseMenu.SetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
	}
	m_PauseMenu.SetFmMusicEnabled(m_FmMusicEnabled);
	m_PauseMenu.SetSccCartEnabled(m_SccCartEnabled);
	m_PauseMenu.SetSccDualCartState(FALSE, FALSE);
	m_PauseMenu.SetAudioGainPercent(m_AudioOutputGainPct);
	backend_runtime_set_overscan(m_OverscanEnabled);
		backend_runtime_set_refresh_hz(m_RefreshRateHz);
		backend_runtime_set_disk_rom_enabled(m_DiskRomEnabled);
		backend_runtime_set_color_artifacts_enabled(m_ColorArtifactsEnabled);
		backend_runtime_set_gfx9000_enabled(m_Gfx9000Enabled);
	backend_runtime_set_boot_mode(m_BootMode);
	backend_runtime_set_model_profile(m_MachineProfile);
	backend_runtime_set_processor_mode(m_ProcessorMode);
	backend_runtime_set_rammapper_kb(m_RamMapperKb);
	backend_runtime_set_megaram_kb(m_MegaRamKb);
	backend_runtime_set_fm_music_enabled(m_FmMusicEnabled);
	backend_runtime_set_fm_layer_gain_pct(m_FmLayerGainPct);
	backend_runtime_set_scc_plus_enabled(FALSE);
	(void) backend_runtime_set_scc_dual_cart_enabled(FALSE);
	m_PauseMenu.SetSccDualCartState(FALSE, FALSE);

	CString line_log;
	line_log.Format("Settings loaded from %s", settings_path);
	m_Logger.Write(FromKernel, LogNotice, line_log);
	BootPhase(m_Logger, "BT6", "settings cfg... OK");
}

unsigned CComboKernel::BuildSettingsPayload(char *buffer, unsigned buffer_size)
{
	if (buffer == 0 || buffer_size < 1024u)
	{
		return 0u;
	}

	unsigned offset = 0u;
	const char *saved_backend_id = backend_runtime_selected_backend_id();
	if (saved_backend_id == 0 || saved_backend_id[0] == '\0')
	{
		saved_backend_id = backend_runtime_default_id();
	}

#define APPEND_LINE_FMT(fmt, value) \
	do { \
		CString line; \
		line.Format(fmt, value); \
		const char *text = (const char *) line; \
		if (text != 0) \
		{ \
			const unsigned len = (unsigned) strlen(text); \
			if (offset + len < buffer_size) \
			{ \
				memcpy(buffer + offset, text, len); \
				offset += len; \
			} \
		} \
	} while (0)

#define APPEND_LINE_TEXT(key, text_value) \
	do { \
		const char *v = (text_value) != 0 ? (text_value) : ""; \
		const unsigned key_len = (unsigned) strlen(key); \
		const unsigned value_len = (unsigned) strlen(v); \
		if (offset + key_len + value_len + 2u < buffer_size) \
		{ \
			memcpy(buffer + offset, key, key_len); \
			offset += key_len; \
			buffer[offset++] = '='; \
			memcpy(buffer + offset, v, value_len); \
			offset += value_len; \
			buffer[offset++] = '\n'; \
		} \
	} while (0)

	APPEND_LINE_TEXT("backend_id", saved_backend_id);
	APPEND_LINE_FMT("language=%u\n", m_Language);
	APPEND_LINE_FMT("overscan=%u\n", m_OverscanEnabled ? 1u : 0u);
	APPEND_LINE_FMT("scanline=%u\n", m_ScanlineMode);
	APPEND_LINE_FMT("color_artifacts=%u\n", m_ColorArtifactsEnabled ? 1u : 0u);
	APPEND_LINE_FMT("machine=%u\n", m_MachineProfile);
	APPEND_LINE_FMT("processor=%u\n", m_ProcessorMode);
	APPEND_LINE_FMT("fm_music=%u\n", m_FmMusicEnabled ? 1u : 0u);
	APPEND_LINE_FMT("autofire=%u\n", m_AutofireEnabled ? 1u : 0u);
	APPEND_LINE_FMT("rammapper_kb=%u\n", m_RamMapperKb);
	{
		u16 joy_codes[CUsbHidGamepad::MapSlotCount];
		m_PauseMenu.GetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
		APPEND_LINE_FMT("joy_map_up=%u\n", (unsigned) joy_codes[CUsbHidGamepad::MapSlotUp]);
		APPEND_LINE_FMT("joy_map_down=%u\n", (unsigned) joy_codes[CUsbHidGamepad::MapSlotDown]);
		APPEND_LINE_FMT("joy_map_left=%u\n", (unsigned) joy_codes[CUsbHidGamepad::MapSlotLeft]);
		APPEND_LINE_FMT("joy_map_right=%u\n", (unsigned) joy_codes[CUsbHidGamepad::MapSlotRight]);
		APPEND_LINE_FMT("joy_map_button1=%u\n", (unsigned) joy_codes[CUsbHidGamepad::MapSlotButton1]);
		APPEND_LINE_FMT("joy_map_button2=%u\n", (unsigned) joy_codes[CUsbHidGamepad::MapSlotButton2]);
	}
	APPEND_LINE_FMT("debugger=%u\n", ClampDebugOverlayMode(m_DebugOverlayMode));
	APPEND_LINE_FMT("audio_gain=%u\n", m_AudioOutputGainPct > 100u ? 100u : m_AudioOutputGainPct);

#undef APPEND_LINE_FMT
#undef APPEND_LINE_TEXT

	static const char kEndMarker[] = "#END\n";
	const unsigned end_len = (unsigned) (sizeof(kEndMarker) - 1u);
	if (offset + end_len >= buffer_size)
	{
		offset = (buffer_size > (end_len + 1u)) ? (unsigned) (buffer_size - end_len - 1u) : 0u;
	}
	memcpy(buffer + offset, kEndMarker, end_len);
	offset += end_len;

	const unsigned fixed_write_bytes = 1024u;
	if (offset > fixed_write_bytes)
	{
		offset = fixed_write_bytes;
	}
	memset(buffer + offset, '\n', fixed_write_bytes - offset);
	return fixed_write_bytes;
}

void CComboKernel::SaveSettingsToStorage(void)
{
	UpdateSettingsSaveTelemetry(1u, 0u, 0u); /* begin */
	char buffer[1024];
	const unsigned fixed_write_bytes = BuildSettingsPayload(buffer, sizeof(buffer));
	if (fixed_write_bytes == 0u)
	{
		UpdateSettingsSaveTelemetry(90u, 1u, 0u); /* payload build failed */
		m_Logger.Write(FromKernel, LogWarning, "Settings save unavailable (payload build failed)");
		return;
	}
	if (!m_SettingsFatFsMounted && m_SettingsStorageReady)
	{
		(void) MountSettingsFileSystem("BT5S");
	}
	if (!m_SettingsFatFsMounted)
	{
		UpdateSettingsSaveTelemetry(90u, 0u, 0u); /* storage unavailable */
		m_Logger.Write(FromKernel, LogWarning, "Settings FAT save unavailable");
		return;
	}

	UpdateSettingsSaveTelemetry(2u, fixed_write_bytes, 0u); /* open/create */
	FIL file;
	const FRESULT open_result = f_open(&file, kSettingsPath, FA_WRITE | FA_OPEN_ALWAYS);
	if (open_result != FR_OK)
	{
		UpdateSettingsSaveTelemetry(91u, open_result, 0u); /* open error */
		UpdateLoadDiagTelemetry(6190u + (ClampFatFsCode(open_result) % 10u));
		LogSdOpResult(m_Logger, "CFG:OPENW", open_result, kSettingsPath, TRUE);
		m_Logger.Write(FromKernel, LogWarning, "Settings save failed (cannot open config file)");
		return;
	}
	const FRESULT seek_result = f_lseek(&file, 0);
	if (seek_result != FR_OK)
	{
		(void) f_close(&file);
		UpdateSettingsSaveTelemetry(97u, seek_result, 0u);
		UpdateLoadDiagTelemetry(6290u + (ClampFatFsCode(seek_result) % 10u));
		LogSdOpResult(m_Logger, "CFG:SEEK", seek_result, kSettingsPath, TRUE);
		m_Logger.Write(FromKernel, LogWarning, "Settings save failed (seek error)");
		return;
	}

	UpdateSettingsSaveTelemetry(3u, fixed_write_bytes, 0u); /* write begin */
	UINT bytes_written = 0;
	const FRESULT write_result = f_write(&file, buffer, fixed_write_bytes, &bytes_written);
	UpdateSettingsSaveTelemetry(4u, bytes_written > 999u ? 999u : bytes_written, fixed_write_bytes); /* write end */

	UpdateSettingsSaveTelemetry(5u, bytes_written > 999u ? 999u : bytes_written, 0u); /* close begin */
	const FRESULT close_result = f_close(&file);
	const unsigned close_ok = (close_result == FR_OK) ? 1u : 0u;
	UpdateSettingsSaveTelemetry(6u, close_ok, bytes_written > 999u ? 999u : bytes_written); /* close end */

	const boolean cfg_saved_ok = (write_result == FR_OK && bytes_written == fixed_write_bytes && close_ok != 0u) ? TRUE : FALSE;
	if (cfg_saved_ok)
	{
		UpdateSettingsSaveTelemetry(7u, bytes_written, fixed_write_bytes); /* success */
		CString line_log;
		line_log.Format("Settings saved to %s", kSettingsPath);
		m_Logger.Write(FromKernel, LogNotice, line_log);
	}
	else
	{
		UpdateSettingsSaveTelemetry(92u, write_result, close_ok); /* write/close error */
		if (write_result != FR_OK)
		{
			UpdateLoadDiagTelemetry(6390u + (ClampFatFsCode(write_result) % 10u));
			LogSdOpResult(m_Logger, "CFG:WRITE", write_result, kSettingsPath, TRUE);
		}
		else if (!close_ok)
		{
			UpdateLoadDiagTelemetry(6490u + (ClampFatFsCode(close_result) % 10u));
			LogSdOpResult(m_Logger, "CFG:CLOSE", close_result, kSettingsPath, TRUE);
		}
		m_Logger.Write(FromKernel, LogWarning, "Settings save failed (write error)");
	}
}

const char *CComboKernel::CurrentCoreName(void) const
{
	if (m_CoreCount > 0u && m_CoreIndex < m_CoreCount && m_CoreNames[m_CoreIndex][0] != '\0')
	{
		return m_CoreNames[m_CoreIndex];
	}
	if (m_BootCoreName[0] != '\0')
	{
		return m_BootCoreName;
	}
	return kDefaultCoreName;
}

void CComboKernel::RefreshCoreListFromStorage(void)
{
	char selected_before[kCoreNameMax];
	KernelCopyText(selected_before, sizeof(selected_before), CurrentCoreName());

	char boot_core[kCoreNameMax];
	KernelCopyText(boot_core, sizeof(boot_core), kDefaultCoreName);
	if (m_SettingsFatFsMounted)
	{
		FIL file;
		if (f_open(&file, kBootConfigPath, FA_READ) == FR_OK)
		{
			char buffer[4096];
			UINT bytes_read = 0u;
				if (f_read(&file, buffer, sizeof(buffer) - 1u, &bytes_read) == FR_OK
				 && bytes_read > 0u
				 && bytes_read < sizeof(buffer) - 1u)
				{
					buffer[bytes_read] = '\0';
					char *block_start = 0;
					char *block_end = 0;
					char *line = 0;
					char *scan_end = 0;
					if (FindBootConfigKernelBlock(buffer, (unsigned) bytes_read, &block_start, &block_end))
					{
						line = block_start;
						scan_end = block_end;
					}
					while (line != 0 && line < scan_end)
					{
						while (line < scan_end && (*line == '\r' || *line == '\n'))
						{
							++line;
						}
						if (line >= scan_end)
						{
							break;
						}
						char *line_end = line;
						while (line_end < scan_end && *line_end != '\r' && *line_end != '\n')
						{
							++line_end;
						}
					char *key_start = line;
					while (key_start < line_end && (*key_start == ' ' || *key_start == '\t'))
					{
						++key_start;
					}
					char *eq = key_start;
					while (eq < line_end && *eq != '=')
					{
						++eq;
					}
					if (eq < line_end)
					{
						const unsigned key_len = (unsigned) (eq - key_start);
						if (BootConfigKeyEquals(key_start, key_len, "kernel"))
						{
							char parsed[kCoreNameMax];
							KernelCopyCoreNameFromPath(parsed, sizeof(parsed), eq + 1, line_end);
							if (parsed[0] != '\0')
							{
								KernelCopyText(boot_core, sizeof(boot_core), parsed);
							}
							break;
						}
					}
					line = line_end;
				}
			}
			(void) f_close(&file);
		}
	}
	if (boot_core[0] == '\0')
	{
		KernelCopyText(boot_core, sizeof(boot_core), kDefaultCoreName);
	}
	KernelCopyText(m_BootCoreName, sizeof(m_BootCoreName), boot_core);

	m_CoreCount = 0u;
	m_CoreIndex = 0u;

#define ADD_CORE_NAME(candidate_text) \
	do { \
		char candidate[kCoreNameMax]; \
		KernelCopyText(candidate, sizeof(candidate), (candidate_text)); \
		if (candidate[0] != '\0') \
		{ \
			boolean duplicate = FALSE; \
			for (unsigned _i = 0u; _i < m_CoreCount; ++_i) \
			{ \
				if (KernelTextEqualsIgnoreCase(m_CoreNames[_i], candidate)) \
				{ \
					duplicate = TRUE; \
					break; \
				} \
			} \
			if (!duplicate && m_CoreCount < kCoreListMax) \
			{ \
				KernelCopyText(m_CoreNames[m_CoreCount], sizeof(m_CoreNames[m_CoreCount]), candidate); \
				++m_CoreCount; \
			} \
		} \
	} while (0)

	if (m_SettingsFatFsMounted)
	{
		DIR dir;
		if (f_opendir(&dir, "SD:/") == FR_OK)
		{
			for (;;)
			{
				FILINFO info;
				memset(&info, 0, sizeof(info));
				const FRESULT read_result = f_readdir(&dir, &info);
				if (read_result != FR_OK || info.fname[0] == '\0')
				{
					break;
				}
				if ((info.fattrib & AM_DIR) != 0u || info.fname[0] == '.')
				{
					continue;
				}
				if (!KernelEndsWithIgnoreCase(info.fname, ".img"))
				{
					continue;
				}
				char core_name[kCoreNameMax];
				KernelCopyCoreNameFromPath(core_name, sizeof(core_name),
					info.fname,
					info.fname + strlen(info.fname));
				ADD_CORE_NAME(core_name);
			}
			(void) f_closedir(&dir);
		}
	}

#undef ADD_CORE_NAME

	if (m_CoreCount == 0u)
	{
		KernelCopyText(m_CoreNames[0], sizeof(m_CoreNames[0]), kDefaultCoreName);
		m_CoreCount = 1u;
	}

	const char *wanted = m_CoreSwitchPending ? selected_before : m_BootCoreName;
	m_CoreIndex = 0u;
	for (unsigned i = 0u; i < m_CoreCount; ++i)
	{
		if (KernelTextEqualsIgnoreCase(m_CoreNames[i], wanted))
		{
			m_CoreIndex = i;
			break;
		}
	}
	m_CoreSwitchPending =
		!KernelTextEqualsIgnoreCase(CurrentCoreName(), m_BootCoreName) ? TRUE : FALSE;
	m_CoreListLoaded = m_SettingsFatFsMounted ? TRUE : FALSE;
	m_PauseMenu.SetCoreName(CurrentCoreName());
}

void CComboKernel::CycleCoreSelection(boolean reverse)
{
	if (!m_CoreListLoaded)
	{
		RefreshCoreListFromStorage();
	}
	if (m_CoreCount == 0u)
	{
		return;
	}
	if (m_CoreCount > 1u)
	{
		if (reverse)
		{
			m_CoreIndex = (m_CoreIndex == 0u) ? (m_CoreCount - 1u) : (m_CoreIndex - 1u);
		}
		else
		{
			m_CoreIndex = (m_CoreIndex + 1u) % m_CoreCount;
		}
	}
	m_CoreSwitchPending =
		!KernelTextEqualsIgnoreCase(CurrentCoreName(), m_BootCoreName) ? TRUE : FALSE;
	m_PauseMenu.SetCoreName(CurrentCoreName());
}

boolean CComboKernel::PersistSelectedCoreToBootConfig(void)
{
	const char *core_name = CurrentCoreName();
	if (core_name == 0 || core_name[0] == '\0')
	{
		return FALSE;
	}
	if (!m_SettingsFatFsMounted && m_SettingsStorageReady)
	{
		(void) MountSettingsFileSystem("CORE");
	}
	if (!m_SettingsFatFsMounted)
	{
		m_Logger.Write(FromKernel, LogWarning, "Core switch failed: FATFS unavailable");
		return FALSE;
	}

	char input[4096];
	unsigned input_len = 0u;
	input[0] = '\0';
	FIL in;
	const FRESULT read_open_result = f_open(&in, kBootConfigPath, FA_READ);
	if (read_open_result != FR_OK)
	{
		LogSdOpResult(m_Logger, "CORE:OPENR", read_open_result, kBootConfigPath, TRUE);
		return FALSE;
	}
	{
		UINT bytes_read = 0u;
		if (f_read(&in, input, sizeof(input) - 1u, &bytes_read) == FR_OK
		 && bytes_read < sizeof(input) - 1u)
		{
			input_len = (unsigned) bytes_read;
			input[input_len] = '\0';
		}
		(void) f_close(&in);
	}
	if (input_len == 0u || input_len >= sizeof(input) - 1u)
	{
		m_Logger.Write(FromKernel, LogWarning, "Core switch failed: config.txt too large");
		return FALSE;
	}

	CString kernel_line;
	CString initramfs_line;
	kernel_line.Format("kernel=%s.img\n", core_name);
	initramfs_line.Format("initramfs %s.cpio followkernel\n", core_name);

	char *block_start = 0;
	char *block_end = 0;
	if (!FindBootConfigKernelBlock(input, input_len, &block_start, &block_end))
	{
		m_Logger.Write(FromKernel, LogWarning, "Core switch failed: kernel block markers missing");
		return FALSE;
	}

	char output[4096];
	unsigned out_len = 0u;

	#define APPEND_OUTPUT_TEXT(text_value) \
	do { \
		const char *_text = (const char *) (text_value); \
		const unsigned _len = (unsigned) strlen(_text); \
		if (out_len + _len >= sizeof(output)) \
		{ \
			return FALSE; \
		} \
		memcpy(output + out_len, _text, _len); \
		out_len += _len; \
	} while (0)

	#define APPEND_OUTPUT_BYTES(ptr_value, len_value) \
	do { \
		const char *_ptr = (const char *) (ptr_value); \
		const unsigned _len = (unsigned) (len_value); \
		if (out_len + _len >= sizeof(output)) \
		{ \
			return FALSE; \
		} \
		memcpy(output + out_len, _ptr, _len); \
		out_len += _len; \
	} while (0)

	APPEND_OUTPUT_BYTES(input, (unsigned) (block_start - input));
	APPEND_OUTPUT_TEXT((const char *) kernel_line);
	APPEND_OUTPUT_TEXT((const char *) initramfs_line);
	APPEND_OUTPUT_TEXT(block_end);

	#undef APPEND_OUTPUT_BYTES
	#undef APPEND_OUTPUT_TEXT

	FIL out;
	const FRESULT open_result = f_open(&out, kBootConfigPath, FA_WRITE | FA_CREATE_ALWAYS);
	if (open_result != FR_OK)
	{
		LogSdOpResult(m_Logger, "CORE:OPENW", open_result, kBootConfigPath, TRUE);
		return FALSE;
	}
	UINT written = 0u;
	const FRESULT write_result = f_write(&out, output, out_len, &written);
	const FRESULT truncate_result = f_truncate(&out);
	const FRESULT close_result = f_close(&out);
	if (write_result != FR_OK || truncate_result != FR_OK || close_result != FR_OK || written != out_len)
	{
		if (write_result != FR_OK) LogSdOpResult(m_Logger, "CORE:WRITE", write_result, kBootConfigPath, TRUE);
		if (truncate_result != FR_OK) LogSdOpResult(m_Logger, "CORE:TRUNC", truncate_result, kBootConfigPath, TRUE);
		if (close_result != FR_OK) LogSdOpResult(m_Logger, "CORE:CLOSE", close_result, kBootConfigPath, TRUE);
		return FALSE;
	}

	KernelCopyText(m_BootCoreName, sizeof(m_BootCoreName), core_name);
	m_CoreSwitchPending = FALSE;
	CString line_log;
	line_log.Format("Core switch saved: %s.img + %s.cpio", core_name, core_name);
	m_Logger.Write(FromKernel, LogNotice, line_log);
	return TRUE;
}

void CComboKernel::HandleSettingsMenuClosed(void)
{
	const boolean settings_was_active =
		(m_LastPauseWasInSettings || m_PauseMenu.IsInSettings()) ? TRUE : FALSE;
	{
		u16 joy_codes[CUsbHidGamepad::MapSlotCount];
		m_PauseMenu.GetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
		m_UsbGamepad.SetMappingCodes(joy_codes, CUsbHidGamepad::MapSlotCount, TRUE);
		m_PauseMenu.SetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
	}

	if (settings_was_active && !m_SettingsSavePending)
	{
		m_SettingsSavePending = TRUE;
		UpdateSettingsSaveTelemetry(8u, 0u, 0u);
		m_Logger.Write(FromKernel, LogNotice, "Settings save queued");
	}

	if (!m_CoreSwitchPending)
	{
		return;
	}
	if (PersistSelectedCoreToBootConfig())
	{
		m_CoreRebootRequested = TRUE;
		m_Logger.Write(FromKernel, LogNotice, "Core switch: reboot requested");
	}
	else
	{
		m_Logger.Write(FromKernel, LogWarning, "Core switch: config update failed; reboot canceled");
	}
}

void CComboKernel::ApplyScaledScreenMode(unsigned source_width, unsigned source_height, const char *source)
{
	if (!m_ScreenReady)
	{
		return;
	}

	if (source_width == 0u)
	{
		source_width = kDefaultVideoWidth;
	}
	if (source_height == 0u)
	{
		source_height = kDefaultVideoHeight;
	}

	const unsigned content_width = source_width * kVideoFramebufferScale;
	const unsigned content_height = source_height * kVideoFramebufferScale;
	unsigned width = content_width;
	unsigned height = content_height;
	if (!m_OverscanEnabled)
	{
		width = ((source_width * kVideoOverscanScaleNum) + (kVideoOverscanScaleDen - 1u))
			/ kVideoOverscanScaleDen;
		height = ((source_height * kVideoOverscanScaleNum) + (kVideoOverscanScaleDen - 1u))
			/ kVideoOverscanScaleDen;
		if (width <= content_width) ++width;
		if (height <= content_height) ++height;
	}
	if (m_Screen.GetWidth() == width && m_Screen.GetHeight() == height)
	{
		return;
	}

	if (!m_Screen.Resize(width, height))
	{
		m_Logger.Write(FromKernel, LogWarning, "Screen resize failed");
		return;
	}

	m_BackendBoxX = 0u;
	m_BackendBoxY = 0u;
	m_BackendBoxW = 0u;
	m_BackendBoxH = 0u;
	m_BlitMapValid = FALSE;
	m_BackendVideoSeq = 0u;
	m_PauseMenu.InvalidateViewportSnapshot();
	m_PauseMenu.RequestRedraw();
	m_PauseNeedsBackgroundRedraw = TRUE;
	m_PauseNeedsFullViewportClear = TRUE;
	m_ForceViewportFullClearNextFrame = TRUE;
	ClearPresentationSurfaces();

	CString line_log;
	line_log.Format("Screen viewport 2x: src=%ux%u fb=%ux%u overscan:%u%s%s",
		source_width,
		source_height,
		width,
		height,
		m_OverscanEnabled ? 1u : 0u,
		source != 0 ? " source=" : "",
		source != 0 ? source : "");
	m_Logger.Write(FromKernel, LogNotice, line_log);
}

boolean CComboKernel::EnsureStateIoBuffer(unsigned min_capacity)
{
	if (min_capacity == 0u || min_capacity > kStateBufferMaxBytes)
	{
		return FALSE;
	}
	if (m_StateIoBuffer != 0 && m_StateIoBufferCapacity >= min_capacity)
	{
		return TRUE;
	}

	u8 *new_buffer = new u8[min_capacity];
	if (new_buffer == 0)
	{
		return FALSE;
	}
	if (m_StateIoBuffer != 0)
	{
		delete[] m_StateIoBuffer;
	}
	m_StateIoBuffer = new_buffer;
	m_StateIoBufferCapacity = min_capacity;
	return TRUE;
}

boolean CComboKernel::EnsureBackendPresentBuffer(unsigned min_pixels)
{
	static const unsigned kPresentBufferMaxPixels = kBlitMapMaxAxis * kBlitMapMaxAxis;

	if (min_pixels == 0u || min_pixels > kPresentBufferMaxPixels)
	{
		return FALSE;
	}
	if (m_BackendPresentBuffer != 0 && m_BackendPresentBufferPixels >= min_pixels)
	{
		return TRUE;
	}

	u16 *new_buffer = new u16[min_pixels];
	if (new_buffer == 0)
	{
		return FALSE;
	}
	if (m_BackendPresentBuffer != 0)
	{
		delete[] m_BackendPresentBuffer;
	}
	m_BackendPresentBuffer = new_buffer;
	m_BackendPresentBufferPixels = min_pixels;
	return TRUE;
}

boolean CComboKernel::EnsureStateStorageDirectory(void)
{
	if (!m_SettingsFatFsMounted)
	{
		return FALSE;
	}

	FILINFO file_info;
	const FRESULT stat_result = f_stat(kStateDirPath, &file_info);
	if (stat_result == FR_OK)
	{
		return ((file_info.fattrib & AM_DIR) != 0u) ? TRUE : FALSE;
	}
	if (stat_result != FR_NO_FILE)
	{
		UpdateLoadDiagTelemetry(7190u + (ClampFatFsCode(stat_result) % 10u));
		LogSdOpResult(m_Logger, "STATE:STATDIR", stat_result, kStateDirPath, TRUE);
		return FALSE;
	}

	const FRESULT mkdir_result = f_mkdir(kStateDirPath);
	if (!(mkdir_result == FR_OK || mkdir_result == FR_EXIST))
	{
		UpdateLoadDiagTelemetry(7290u + (ClampFatFsCode(mkdir_result) % 10u));
		LogSdOpResult(m_Logger, "STATE:MKDIR", mkdir_result, kStateDirPath, TRUE);
	}
	return (mkdir_result == FR_OK || mkdir_result == FR_EXIST) ? TRUE : FALSE;
}

void CComboKernel::SaveEmulatorStateToStorage(void)
{
	UpdateSettingsSaveTelemetry(32u, 0u, 0u);
	if (!m_BackendReady)
	{
		UpdateSettingsSaveTelemetry(93u, 2u, 0u);
		m_Logger.Write(FromKernel, LogWarning, "Save State falhou: runtime backend inativo");
		return;
	}
	if (!EnsureStateStorageDirectory())
	{
		UpdateSettingsSaveTelemetry(93u, 3u, 0u);
		m_Logger.Write(FromKernel, LogWarning, "Save State falhou: armazenamento SD indisponivel");
		return;
	}
	unsigned state_bytes = 0u;
	static const unsigned kTryCaps[] = { 512u * 1024u, 1024u * 1024u, 1536u * 1024u, 2u * 1024u * 1024u };
	boolean serialized = FALSE;
	for (unsigned i = 0u; i < (unsigned) (sizeof(kTryCaps) / sizeof(kTryCaps[0])); ++i)
	{
		const unsigned cap = kTryCaps[i];
		if (!EnsureStateIoBuffer(cap))
		{
			continue;
		}
		if (backend_runtime_save_state(m_StateIoBuffer, cap, &state_bytes) && state_bytes > 0u)
		{
			serialized = TRUE;
			break;
		}
	}
	if (!serialized)
	{
		UpdateSettingsSaveTelemetry(93u, 5u, 0u);
		m_Logger.Write(FromKernel, LogWarning, "Save State falhou: serializacao do estado");
		return;
	}
	UpdateSettingsSaveTelemetry(33u, state_bytes > 999u ? 999u : state_bytes, 0u);

	FIL file;
	const FRESULT open_result = f_open(&file, kStatePath, FA_WRITE | FA_CREATE_ALWAYS);
	if (open_result != FR_OK)
	{
		UpdateSettingsSaveTelemetry(93u, 6u, open_result);
		UpdateLoadDiagTelemetry(7390u + (ClampFatFsCode(open_result) % 10u));
		LogSdOpResult(m_Logger, "STATE:OPENW", open_result, kStatePath, TRUE);
		m_Logger.Write(FromKernel, LogWarning, "Save State falhou: nao foi possivel criar arquivo");
		return;
	}

	UINT bytes_written = 0u;
	const FRESULT write_result = f_write(&file, m_StateIoBuffer, state_bytes, &bytes_written);
	const FRESULT close_result = f_close(&file);
	if (write_result == FR_OK && close_result == FR_OK && bytes_written == state_bytes)
	{
		UpdateSettingsSaveTelemetry(34u, bytes_written > 999u ? 999u : bytes_written, 0u);
		CString line;
		line.Format("Save State OK: %u bytes em %s", state_bytes, kStatePath);
		m_Logger.Write(FromKernel, LogNotice, line);
	}
	else
	{
		UpdateSettingsSaveTelemetry(93u, 7u, bytes_written > 999u ? 999u : bytes_written);
		if (write_result != FR_OK)
		{
			UpdateLoadDiagTelemetry(7490u + (ClampFatFsCode(write_result) % 10u));
			LogSdOpResult(m_Logger, "STATE:WRITE", write_result, kStatePath, TRUE);
		}
		else if (close_result != FR_OK)
		{
			UpdateLoadDiagTelemetry(7590u + (ClampFatFsCode(close_result) % 10u));
			LogSdOpResult(m_Logger, "STATE:CLOSE", close_result, kStatePath, TRUE);
		}
		m_Logger.Write(FromKernel, LogWarning, "Save State falhou: erro de escrita no SD");
	}
}

void CComboKernel::LoadEmulatorStateFromStorage(void)
{
	UpdateSettingsSaveTelemetry(42u, 0u, 0u);
	if (!m_BackendReady)
	{
		UpdateSettingsSaveTelemetry(94u, 2u, 0u);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: runtime backend inativo");
		return;
	}
	if (!m_SettingsFatFsMounted)
	{
		UpdateSettingsSaveTelemetry(94u, 3u, 0u);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: armazenamento SD indisponivel");
		return;
	}

	FILINFO file_info;
	const FRESULT stat_result = f_stat(kStatePath, &file_info);
	if (stat_result == FR_NO_FILE)
	{
		UpdateSettingsSaveTelemetry(94u, 5u, 0u);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: arquivo inexistente");
		return;
	}
	if (stat_result != FR_OK)
	{
		UpdateSettingsSaveTelemetry(94u, 6u, stat_result);
		UpdateLoadDiagTelemetry(8190u + (ClampFatFsCode(stat_result) % 10u));
		LogSdOpResult(m_Logger, "STATE:STAT", stat_result, kStatePath, TRUE);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: erro ao consultar arquivo");
		return;
	}

	FIL file;
	const FRESULT open_result = f_open(&file, kStatePath, FA_READ);
	if (open_result != FR_OK)
	{
		UpdateSettingsSaveTelemetry(94u, 7u, open_result);
		UpdateLoadDiagTelemetry(8290u + (ClampFatFsCode(open_result) % 10u));
		LogSdOpResult(m_Logger, "STATE:OPENR", open_result, kStatePath, TRUE);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: nao foi possivel abrir arquivo");
		return;
	}

	const unsigned file_size = (unsigned) f_size(&file);
	if (file_size == 0u || file_size > kStateBufferMaxBytes)
	{
		(void) f_close(&file);
		UpdateSettingsSaveTelemetry(94u, 8u, file_size > 999u ? 999u : file_size);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: tamanho invalido");
		return;
	}
	if (!EnsureStateIoBuffer(file_size))
	{
		(void) f_close(&file);
		UpdateSettingsSaveTelemetry(94u, 4u, file_size > 999u ? 999u : file_size);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: memoria insuficiente");
		return;
	}
	UpdateSettingsSaveTelemetry(43u, file_size > 999u ? 999u : file_size, 0u);

	UINT bytes_read = 0u;
	const FRESULT read_result = f_read(&file, m_StateIoBuffer, file_size, &bytes_read);
	const FRESULT close_result = f_close(&file);
	if (read_result != FR_OK || close_result != FR_OK || bytes_read != file_size)
	{
		UpdateSettingsSaveTelemetry(94u, 9u, bytes_read > 999u ? 999u : bytes_read);
		if (read_result != FR_OK)
		{
			UpdateLoadDiagTelemetry(8390u + (ClampFatFsCode(read_result) % 10u));
			LogSdOpResult(m_Logger, "STATE:READ", read_result, kStatePath, TRUE);
		}
		else if (close_result != FR_OK)
		{
			UpdateLoadDiagTelemetry(8490u + (ClampFatFsCode(close_result) % 10u));
			LogSdOpResult(m_Logger, "STATE:CLOSE", close_result, kStatePath, TRUE);
		}
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: erro de leitura no SD");
		return;
	}

	if (!backend_runtime_load_state(m_StateIoBuffer, file_size))
	{
		UpdateSettingsSaveTelemetry(94u, 10u, 0u);
		m_Logger.Write(FromKernel, LogWarning, "Load State falhou: arquivo invalido/corrompido");
		return;
	}

		PushKeyboardStateToRuntime();
		backend_runtime_input_push_joystick(ResolveRuntimeJoystickBits());
		if (backend_runtime_step_frame())
		{
			const BackendRuntimeStatus runtime_status = backend_runtime_get_status();
			m_BackendFrames = runtime_status.frames;
			m_BackendLastError = runtime_status.last_error;
			m_BackendLastPC = runtime_status.last_pc;
			BlitBackendFrame(TRUE);
			/* Force a fresh pause-background snapshot from the just-loaded state. */
			m_PauseNeedsBackgroundRedraw = TRUE;
			m_PauseNeedsFullViewportClear = FALSE;
		}
	else
	{
		m_PauseNeedsBackgroundRedraw = TRUE;
		m_PauseNeedsFullViewportClear = FALSE;
	}
	m_PauseMenu.RequestRedraw();
	UpdateSettingsSaveTelemetry(44u, file_size > 999u ? 999u : file_size, 0u);
	CString line;
	line.Format("Load State OK: %u bytes de %s", file_size, kStatePath);
	m_Logger.Write(FromKernel, LogNotice, line);
}

void CComboKernel::HandleMenuAction(TComboMenuAction action)
{
	switch (action)
	{
	case ComboMenuActionCartridgeA:
	case ComboMenuActionCartridgeB:
		{
			unsigned browser_slot = 0u;
			char browser_path[192];
			browser_path[0] = '\0';
			if (m_PauseMenu.ConsumePendingRomBrowserOpen(&browser_slot, browser_path, sizeof(browser_path)))
			{
				m_PauseMenu.OpenFileBrowser((unsigned) ComboFileBrowserModeRom, browser_slot, browser_path);
				UpdateLoadDiagTelemetry(2103u); /* ROM browser open deferred to main loop */
				break;
			}
		}
		if (m_BackendReady)
		{
			unsigned slot_index = 0u;
			char rom_path[192];
			rom_path[0] = '\0';
			if (m_PauseMenu.ConsumePendingRomSelection(&slot_index, rom_path, sizeof(rom_path)))
			{
				UpdateLoadDiagTelemetry(2001u); /* menu consumed selection */
				if (slot_index > 1u)
				{
					slot_index = (action == ComboMenuActionCartridgeB) ? 1u : 0u;
				}
				if (slot_index == 1u && m_SccDualCartEnabled)
				{
					UpdateLoadDiagTelemetry(9008u); /* slot B reserved by Konami SCC+ DUAL */
					m_Logger.Write(FromKernel, LogWarning, "Load ROM bloqueado: SLOT2 reservado por Konami SCC+ DUAL");
					break;
				}
				if (rom_path[0] == '\0')
				{
					if (backend_runtime_unload_cartridge(slot_index))
					{
						UpdateLoadDiagTelemetry(2005u); /* unload cart ok */
						m_PauseMenu.SetSccDualCartState(m_SccDualCartEnabled, backend_runtime_get_scc_dual_cart_available());
						CString line;
						line.Format("Unload ROM: SLOT%u", slot_index + 1u);
						m_Logger.Write(FromKernel, LogNotice, line);
					}
					else
					{
						UpdateLoadDiagTelemetry(9007u); /* unload cart fail */
						m_Logger.Write(FromKernel, LogWarning, "Unload ROM falhou");
					}
					break;
				}

					if (!backend_runtime_media_load_cartridge(slot_index, rom_path))
					{
						UpdateLoadDiagTelemetry(9001u); /* load cartridge failed */
						m_Logger.Write(FromKernel, LogWarning, "Load ROM falhou: arquivo invalido");
						break;
					}
					UpdateLoadDiagTelemetry(2002u); /* cartridge loaded */
					const boolean load_ok = PerformMenuHardReset(2003u, 2004u, 9004u);
					if (load_ok)
					{
						CString line;
						line.Format("Load ROM: SLOT%u <= %s", slot_index + 1u, rom_path);
						m_Logger.Write(FromKernel, LogNotice, line);
						ExitPauseForBackendAction();
					}
					else
					{
						m_Logger.Write(FromKernel, LogWarning, "Load ROM falhou: hard reset");
					}
				}
			}
		break;

	case ComboMenuActionDiskDriveA:
	case ComboMenuActionDiskDriveB:
		m_Logger.Write(FromKernel, LogNotice, "Load Disk indisponivel neste build");
		break;

	case ComboMenuActionCassette:
		{
			char browser_path[192];
			browser_path[0] = '\0';
			if (m_PauseMenu.ConsumePendingCassetteBrowserOpen(browser_path, sizeof(browser_path)))
			{
				m_PauseMenu.OpenFileBrowser((unsigned) ComboFileBrowserModeCassette, 0u, browser_path);
				UpdateLoadDiagTelemetry(4103u); /* CAS browser open deferred to main loop */
				break;
			}
		}
		if (m_BackendReady)
		{
			char cassette_path[192];
			cassette_path[0] = '\0';
			if (m_PauseMenu.ConsumePendingCassetteSelection(cassette_path, sizeof(cassette_path)))
			{
				UpdateLoadDiagTelemetry(4001u); /* menu consumed cassette selection */
				if (cassette_path[0] == '\0')
				{
					UpdateLoadDiagTelemetry(4002u); /* unload requested */
					if (backend_runtime_unload_cassette())
					{
						UpdateLoadDiagTelemetry(4005u); /* unload cassette ok */
						m_Logger.Write(FromKernel, LogNotice, "Unload CAS: TAPE");
					}
					else
					{
						UpdateLoadDiagTelemetry(9008u); /* unload cassette fail */
						m_Logger.Write(FromKernel, LogWarning, "Unload CAS falhou");
					}
					break;
				}
				UpdateLoadDiagTelemetry(4002u); /* load requested */
				const boolean load_ok = backend_runtime_media_load_cassette(cassette_path);
				if (load_ok)
				{
					UpdateLoadDiagTelemetry(4004u); /* cassette load ok */
					CString line;
					line.Format("Load CAS: TAPE <= %s", cassette_path);
					m_Logger.Write(FromKernel, LogNotice, line);
				}
				else
				{
					UpdateLoadDiagTelemetry(9006u); /* cassette load failed */
					m_Logger.Write(FromKernel, LogWarning, "Load CAS falhou");
				}
			}
		}
		break;

	case ComboMenuActionSoftReset:
		if (m_BackendReady)
		{
			ExitPauseForBackendAction();
			const boolean hard_reset_required = m_MenuHardResetPending;
			const boolean reset_ok = hard_reset_required
				? PerformMenuHardReset(3527u, 3528u, 9528u)
				: PerformMenuSoftReset(3503u, 3504u, 9503u);
			if (reset_ok)
			{
				m_Logger.Write(FromKernel, LogNotice,
					hard_reset_required ? "Menu: deferred hard reset emulator" : "Menu: soft reset emulator");
			}
			else
			{
				m_Logger.Write(FromKernel, LogWarning,
					hard_reset_required ? "Menu: deferred hard reset failed" : "Menu: soft reset failed");
			}
		}
		break;

	case ComboMenuActionHardReset:
		if (m_BackendReady)
		{
			ExitPauseForBackendAction();
			const boolean reset_ok = PerformMenuHardReset(3501u, 3502u, 9502u);
			if (reset_ok)
			{
				m_Logger.Write(FromKernel, LogNotice, "Menu: hard reset emulator");
			}
			else
			{
				m_Logger.Write(FromKernel, LogWarning, "Menu: hard reset failed");
			}
		}
		break;

	case ComboMenuActionCycleLanguage:
		m_Language = (m_Language >= ComboLanguageES) ? ComboLanguagePT : (m_Language + 1u);
		m_PauseMenu.SetLanguage(m_Language);
		RefreshSettingsUiAfterAction();
		{
			CString line;
			line.Format("Settings: Language %s", combo_locale_language_code(m_Language));
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		break;

	case ComboMenuActionCycleLanguagePrev:
		m_Language = (m_Language == ComboLanguagePT) ? ComboLanguageES : (m_Language - 1u);
		m_PauseMenu.SetLanguage(m_Language);
		RefreshSettingsUiAfterAction();
		{
			CString line;
			line.Format("Settings: Language %s", combo_locale_language_code(m_Language));
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		break;

	case ComboMenuActionToggleOverscan:
		m_OverscanEnabled = m_OverscanEnabled ? FALSE : TRUE;
		m_PauseMenu.SetOverscanEnabled(m_OverscanEnabled);
		RefreshSettingsUiAfterAction();
		if (m_EmulationPaused && m_BackendReady)
		{
			m_PauseNeedsBackgroundRedraw = TRUE;
			m_PauseNeedsFullViewportClear = TRUE;
		}
		m_ForceViewportFullClearNextFrame = TRUE;
		m_BackendVideoSeq = 0u;
		m_Logger.Write(FromKernel, LogNotice,
			m_OverscanEnabled ? "Settings: Overscan ON" : "Settings: Overscan OFF");
		break;

	case ComboMenuActionCycleScanlines:
		m_ScanlineMode = (m_ScanlineMode == kScanlineModeOff) ? kScanlineModeLcd : kScanlineModeOff;
		m_PauseMenu.SetScanlineMode(m_ScanlineMode);
		RefreshSettingsUiAfterAction();
		if (m_ScanlineMode == kScanlineModeOff)
		{
			m_Logger.Write(FromKernel, LogNotice, "Settings: Scanlines OFF");
		}
		else
		{
			m_Logger.Write(FromKernel, LogNotice, "Settings: Scanlines ON");
		}
		break;

	case ComboMenuActionCycleScanlinesPrev:
		m_ScanlineMode = (m_ScanlineMode == kScanlineModeOff) ? kScanlineModeLcd : kScanlineModeOff;
		m_PauseMenu.SetScanlineMode(m_ScanlineMode);
		RefreshSettingsUiAfterAction();
		if (m_ScanlineMode == kScanlineModeOff)
		{
			m_Logger.Write(FromKernel, LogNotice, "Settings: Scanlines OFF");
		}
		else
		{
			m_Logger.Write(FromKernel, LogNotice, "Settings: Scanlines ON");
		}
		break;

	case ComboMenuActionToggleColorArtifacts:
		m_ColorArtifactsEnabled = m_ColorArtifactsEnabled ? FALSE : TRUE;
		m_PauseMenu.SetColorArtifactsEnabled(m_ColorArtifactsEnabled);
		RefreshSettingsUiAfterAction();
		backend_runtime_set_color_artifacts_enabled(m_ColorArtifactsEnabled);
		m_Logger.Write(FromKernel, LogNotice,
			m_ColorArtifactsEnabled ? "Settings: Color Artifacts ON" : "Settings: Color Artifacts OFF");
		break;

	case ComboMenuActionToggleGfx9000:
		m_Gfx9000Enabled = FALSE;
		m_PauseMenu.SetGfx9000Enabled(FALSE);
		RefreshSettingsUiAfterAction();
		m_Logger.Write(FromKernel, LogNotice, "Settings: GFX9000 indisponivel neste build");
		break;

	case ComboMenuActionToggleDiskRom:
		m_Logger.Write(FromKernel, LogNotice, "Disk ROM indisponivel neste build");
		break;

	case ComboMenuActionCycleCore:
	case ComboMenuActionCycleCorePrev:
		CycleCoreSelection(action == ComboMenuActionCycleCorePrev ? TRUE : FALSE);
		RefreshSettingsUiAfterAction();
		{
			CString line;
			line.Format("Settings: Core %s", CurrentCoreName());
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		break;

	case ComboMenuActionCycleMachine:
		{
			m_MachineProfile = CycleMachineProfileForBuild(m_MachineProfile, FALSE);
		}
		m_RamMapperKb = ClampRamMapperKb(m_RamMapperKb);
		m_PauseMenu.SetMachineProfile(m_MachineProfile);
		m_PauseMenu.SetRamMapperKb(m_RamMapperKb);
		RefreshSettingsUiAfterAction();
		QueueMenuHardReset();
		{
			CString line;
			line.Format("Settings: Machine %s", MachineLabelFromProfile(m_MachineProfile));
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		break;

	case ComboMenuActionCycleMachinePrev:
		{
			m_MachineProfile = CycleMachineProfileForBuild(m_MachineProfile, TRUE);
		}
		m_RamMapperKb = ClampRamMapperKb(m_RamMapperKb);
		m_PauseMenu.SetMachineProfile(m_MachineProfile);
		m_PauseMenu.SetRamMapperKb(m_RamMapperKb);
		RefreshSettingsUiAfterAction();
		QueueMenuHardReset();
		{
			CString line;
			line.Format("Settings: Machine %s", MachineLabelFromProfile(m_MachineProfile));
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		break;

	case ComboMenuActionCycleProcessor:
	case ComboMenuActionCycleProcessorPrev:
		m_ProcessorMode = CycleProcessorMode(m_ProcessorMode);
		m_PauseMenu.SetProcessorMode(m_ProcessorMode);
		RefreshSettingsUiAfterAction();
		QueueMenuHardReset();
		{
			CString line;
			line.Format("Settings: Processor %s", ProcessorModeLabel(m_ProcessorMode));
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		break;

	case ComboMenuActionToggleAutofire:
		m_AutofireEnabled = m_AutofireEnabled ? FALSE : TRUE;
		m_AutofireButton2Active = FALSE;
		m_AutofireButton2OutputOn = TRUE;
		m_AutofireButton2LastToggleUs = 0u;
		m_PauseMenu.SetAutofireEnabled(m_AutofireEnabled);
		RefreshSettingsUiAfterAction();
		m_Logger.Write(FromKernel, LogNotice,
			m_AutofireEnabled ? "Settings: Autofire ON" : "Settings: Autofire OFF");
		break;

	case ComboMenuActionCycleRamMapper:
		if (MachineProfileSupportsAllocRam(m_MachineProfile))
		{
			m_RamMapperKb = CycleRamMapperKb(m_RamMapperKb, FALSE);
			m_PauseMenu.SetRamMapperKb(m_RamMapperKb);
			RefreshSettingsUiAfterAction();
			QueueMenuHardReset();
		}
		break;

	case ComboMenuActionCycleRamMapperPrev:
		if (MachineProfileSupportsAllocRam(m_MachineProfile))
		{
			m_RamMapperKb = CycleRamMapperKb(m_RamMapperKb, TRUE);
			m_PauseMenu.SetRamMapperKb(m_RamMapperKb);
			RefreshSettingsUiAfterAction();
			QueueMenuHardReset();
		}
		break;

	case ComboMenuActionCycleMegaRam:
		m_Logger.Write(FromKernel, LogNotice, "MegaRAM indisponivel neste build");
		break;

	case ComboMenuActionCycleMegaRamPrev:
		m_Logger.Write(FromKernel, LogNotice, "MegaRAM indisponivel neste build");
		break;

	case ComboMenuActionApplyJoystickMap:
	{
		u16 joy_codes[CUsbHidGamepad::MapSlotCount];
		m_PauseMenu.GetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
		m_UsbGamepad.SetMappingCodes(joy_codes, CUsbHidGamepad::MapSlotCount, TRUE);
		m_PauseMenu.SetJoystickMapCodes(joy_codes, CUsbHidGamepad::MapSlotCount);
		RefreshSettingsUiAfterAction();
		m_Logger.Write(FromKernel, LogNotice, "Settings: JOYPAD MAPPING updated");
		break;
	}

	case ComboMenuActionAudioGainChanged:
		m_AudioOutputGainPct = m_PauseMenu.GetAudioGainPercent();
		RefreshSettingsUiAfterAction();
		{
			CString line;
			line.Format("Settings: Audio gain %u (%u%%)",
				AudioGainUiLevelFromPercent(m_AudioOutputGainPct),
				m_AudioOutputGainPct);
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		break;

	case ComboMenuActionToggleSccCart:
		m_Logger.Write(FromKernel, LogNotice, "SCC indisponivel neste build");
		break;

	case ComboMenuActionToggleFmMusic:
		m_FmMusicEnabled = m_FmMusicEnabled ? FALSE : TRUE;
		m_PauseMenu.SetFmMusicEnabled(m_FmMusicEnabled);
		RefreshSettingsUiAfterAction();
		backend_runtime_set_fm_music_enabled(m_FmMusicEnabled);
		m_Logger.Write(FromKernel, LogNotice,
			m_FmMusicEnabled ? "Settings: FM Sound ON" : "Settings: FM Sound OFF");
		break;

	case ComboMenuActionSaveState:
		ExitPauseForBackendAction();
		RefreshSettingsUiAfterAction();
		m_StateSavePending = TRUE;
		UpdateSettingsSaveTelemetry(31u, 0u, 0u);
		m_Logger.Write(FromKernel, LogNotice, "Save State queued");
		break;

	case ComboMenuActionLoadState:
		ExitPauseForBackendAction();
		RefreshSettingsUiAfterAction();
		m_StateLoadPending = TRUE;
		UpdateSettingsSaveTelemetry(41u, 0u, 0u);
		m_Logger.Write(FromKernel, LogNotice, "Load State queued");
		break;

	default:
		break;
	}
}

void CComboKernel::SetDebugOverlayMode(unsigned mode)
{
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	const unsigned old_mode = ClampDebugOverlayMode(m_DebugOverlayMode);
	const unsigned new_mode = ClampDebugOverlayMode(mode);
	m_DebugOverlayMode = new_mode;
	if (old_mode != new_mode)
	{
		ClearPresentationSurfaces();
		if (m_ScreenReady)
		{
			const unsigned screen_width = m_Screen.GetWidth();
			const unsigned screen_height = m_Screen.GetHeight();
			memset(m_DebugOverlayLineLengths, 0, sizeof(m_DebugOverlayLineLengths));
			WriteScreenText("\x1b[2J\x1b[H\x1b[?25l");
			if (screen_width > 0u && screen_height > 0u)
			{
				m_Screen.MarkDirtyRect(0u, 0u, screen_width, screen_height);
				(void) m_Screen.PresentIfDirty(m_DCacheEnabled, TRUE);
			}
		}
		m_PauseMenu.InvalidateViewportSnapshot();
		if (m_EmulationPaused && m_BackendReady)
		{
			m_PauseNeedsBackgroundRedraw = TRUE;
			m_PauseNeedsFullViewportClear = TRUE;
		}
		m_ForceViewportFullClearNextFrame = TRUE;
	}
	m_PauseMenu.RequestRedraw();
	if (m_BackendReady)
	{
		backend_runtime_set_overscan(m_OverscanEnabled);
	}
	if (m_ScreenReady && m_DebugOverlayUnlocked)
	{
		ClearDebugOverlay();
	}
#else
	(void) mode;
#endif
}

void CComboKernel::SetUartTelemetryEnabled(boolean enabled)
{
#if SMSBARE_ENABLE_DEBUG_OVERLAY
	m_UartTelemetryEnabled = enabled ? TRUE : FALSE;
#else
	(void) enabled;
	m_UartTelemetryEnabled = FALSE;
#endif
	ResetUartPerfTelemetry();
	/* UART perf telemetry is intentionally quiesced in this phase. */
	backend_runtime_set_frame_perf_telemetry_enabled(FALSE);
	backend_runtime_reset_audio_mix_debug_window();
}

void CComboKernel::ClearDebugOverlay(void)
{
#if !SMSBARE_ENABLE_DEBUG_OVERLAY
	return;
#else
	if (!m_ScreenReady)
	{
		return;
	}

	for (unsigned row = 1u; row <= 20u; ++row)
	{
		WriteDebugOverlayLine(row, 0);
	}
	WriteScreenText("\x1b[0m");
#endif
}

void CComboKernel::WriteDebugOverlayLine(unsigned row, const char *text)
{
#if !SMSBARE_ENABLE_DEBUG_OVERLAY
	(void) row;
	(void) text;
	return;
#else
	if (!m_ScreenReady || row == 0u || row > 20u)
	{
		return;
	}

	const unsigned index = row - 1u;
	const unsigned old_len = m_DebugOverlayLineLengths[index];
	const unsigned new_len = text != 0 ? (unsigned) strlen(text) : 0u;

	if (old_len > 0u)
	{
		CString clear;
		clear.Format("\x1b[97m\x1b[49m\x1b[%u;1H%*s", row, (int) old_len, "");
		WriteScreenText(clear);
	}

	if (new_len > 0u)
	{
		CString line;
		line.Format("\x1b[97m\x1b[49m\x1b[%u;1H%s", row, text);
		WriteScreenText(line);
	}

	m_DebugOverlayLineLengths[index] = new_len;
#endif
}

void CComboKernel::WriteScreenText(const char *text)
{
	if (text == 0 || !m_ScreenReady)
	{
		return;
	}

	m_Screen.Write(text, strlen(text));
}

void CComboKernel::SystemThrottledHandler(TSystemThrottledState state, void *param)
{
	if (param == 0)
	{
		return;
	}

	((CComboKernel *) param)->HandleSystemThrottled(state);
}

void CComboKernel::HandleSystemThrottled(TSystemThrottledState state)
{
	CString line;
	line.Format("Throttle event: %s state=0x%08X",
		ThrottleStateLabel(state),
		(unsigned) state);
	if (!BackendQueueKernelLogDuringGfx9000Trace("KTHR: ", (const char *) line))
	{
		m_Logger.Write(FromKernel, LogWarning, line);
	}
	LogCpuClockStatus("THR");
}

void CComboKernel::LogCpuClockStatus(const char *phase)
{
	const unsigned arm_hz = m_CPUThrottle.GetClockRate();
	const unsigned core_hz = CMachineInfo::Get()->GetClockRate(CLOCK_ID_CORE);
	const unsigned temp_c = m_CPUThrottle.GetTemperature();
	const unsigned min_hz = m_CPUThrottle.GetMinClockRate();
	const unsigned max_hz = m_CPUThrottle.GetMaxClockRate();
	CBcmPropertyTags tags;
	TPropertyTagSimple throttled;
	throttled.nValue = 0u;
	const boolean throttled_ok = tags.GetTag(PROPTAG_GET_THROTTLED, &throttled, sizeof throttled, 4);

	CString line;
	line.Format("CLK %s ARM:%u CORE:%u TEMP:%u DYN:%u RNG:%u-%u THR:%08X",
		phase != 0 ? phase : "-",
		arm_hz / 1000000u,
		core_hz / 1000000u,
		temp_c,
		m_CPUThrottle.IsDynamic() ? 1u : 0u,
		min_hz / 1000000u,
		max_hz / 1000000u,
		throttled_ok ? throttled.nValue : 0u);
	if (!BackendQueueKernelLogDuringGfx9000Trace("KCLK: ", (const char *) line))
	{
		m_Logger.Write(FromKernel, LogNotice, line);
	}
}

void CComboKernel::BootTrace(const char *tag)
{
	if (kBootQuiet)
	{
		return;
	}

	if (tag == 0)
	{
		return;
	}

	CString line;
	line.Format("BT:%s", tag);
	m_Logger.Write(FromKernel, LogNotice, line);
}

void CComboKernel::PushKeyboardStateToRuntime(void)
{
	u8 modifiers = 0u;
	unsigned char raw_keys[6];
	boolean consumed_report = FALSE;
	unsigned drained_reports = 0u;

	while (m_UsbKeyboard.ConsumeForRuntime(&modifiers, raw_keys))
	{
		consumed_report = TRUE;
		++drained_reports;
		m_RuntimeKeyboardModifiers = modifiers;
		for (unsigned i = 0u; i < 6u; ++i)
		{
			m_RuntimeKeyboardRawKeys[i] = raw_keys[i];
		}

		/*
		 * The runtime must observe every queued HID edge before the next
		 * emulation frame. Some titles only poll input in tight windows; if
		 * press/release transitions backlog in the queue, the backend sees
		 * stale state on hardware even though the host-side harness (which
		 * injects directly) behaves correctly.
		 */
		backend_runtime_input_push_keyboard(modifiers, raw_keys);
	}

	if (consumed_report)
	{
		return;
	}

	backend_runtime_input_push_keyboard(m_RuntimeKeyboardModifiers, m_RuntimeKeyboardRawKeys);
}

u8 CComboKernel::ResolveRuntimeJoystickBits(void)
{
	u8 bits = (u8) (m_JoyBridgeBits | m_UsbGamepad.GetBridgeBits());

	if ((bits & (kJoyBridgeLeft | kJoyBridgeRight)) == (kJoyBridgeLeft | kJoyBridgeRight))
	{
		bits &= (u8) ~(kJoyBridgeLeft | kJoyBridgeRight);
	}
	if ((bits & (kJoyBridgeUp | kJoyBridgeDown)) == (kJoyBridgeUp | kJoyBridgeDown))
	{
		bits &= (u8) ~(kJoyBridgeUp | kJoyBridgeDown);
	}

	if (m_AutofireEnabled && (bits & kJoyBridgeFireB) != 0u)
	{
		const u64 now_us = CTimer::GetClockTicks64();
		if (!m_AutofireButton2Active)
		{
			m_AutofireButton2Active = TRUE;
			m_AutofireButton2OutputOn = TRUE;
			m_AutofireButton2LastToggleUs = now_us;
		}
		while ((now_us - m_AutofireButton2LastToggleUs) >= kAutofireHalfPeriodUs)
		{
			m_AutofireButton2LastToggleUs += kAutofireHalfPeriodUs;
			m_AutofireButton2OutputOn = m_AutofireButton2OutputOn ? FALSE : TRUE;
		}
		if (!m_AutofireButton2OutputOn)
		{
			bits &= (u8) ~kJoyBridgeFireB;
		}
	}
	else
	{
		m_AutofireButton2Active = FALSE;
		m_AutofireButton2OutputOn = TRUE;
		m_AutofireButton2LastToggleUs = 0u;
	}

	return bits;
}

void CComboKernel::UpdateJoystickFromRaw(unsigned char modifiers, const unsigned char raw_keys[6])
{
	const u64 now_us = CTimer::GetClockTicks64();
	boolean same_report = (m_LastKeyboardModifiers == modifiers) ? TRUE : FALSE;
	for (unsigned i = 0u; same_report && i < 6u; ++i)
	{
		if (m_LastKeyboardRawKeys[i] != raw_keys[i])
		{
			same_report = FALSE;
		}
	}
	if (same_report)
	{
		if (!m_EmulationPaused)
		{
			return;
		}
		/* Guard against over-fast repeats when raw callback and frame poll both fire. */
		if (m_LastMenuInputPollUs != 0u && (now_us - m_LastMenuInputPollUs) < 12000ull)
		{
			return;
		}
	}

	TComboKeyboardInputState key_state;
	ComboDecodeKeyboardRaw(modifiers, raw_keys, &key_state);
	const u8 menu_joy_bits = m_UsbGamepad.GetMenuBits();
	if ((menu_joy_bits & CUsbHidGamepad::MenuBitPause) != 0u)
	{
		key_state.pause_pressed = TRUE;
	}
	if (m_EmulationPaused && !m_PauseMenu.IsCapturingJoystickMap())
	{
		if ((menu_joy_bits & CUsbHidGamepad::MenuBitUp) != 0u)
		{
			key_state.menu_up_pressed = TRUE;
		}
		if ((menu_joy_bits & CUsbHidGamepad::MenuBitDown) != 0u)
		{
			key_state.menu_down_pressed = TRUE;
		}
		if ((menu_joy_bits & CUsbHidGamepad::MenuBitLeft) != 0u)
		{
			key_state.menu_left_pressed = TRUE;
		}
		if ((menu_joy_bits & CUsbHidGamepad::MenuBitRight) != 0u)
		{
			key_state.menu_right_pressed = TRUE;
		}
		if ((menu_joy_bits & CUsbHidGamepad::MenuBitSelect) != 0u)
		{
			key_state.menu_enter_pressed = TRUE;
		}
		if ((menu_joy_bits & CUsbHidGamepad::MenuBitBack) != 0u)
		{
			key_state.menu_back_pressed = TRUE;
		}
	}
	if (same_report)
	{
		/* While paused, keep polling so joystick mapping can capture without a fresh keyboard edge. */
	}

	if (key_state.f1_pressed && !m_LastF12Pressed)
	{
		#if SMSBARE_ENABLE_DEBUG_OVERLAY
		SetDebugOverlayMode(NextDebugOverlayMode(m_DebugOverlayMode));
		{
			CString line;
			line.Format("Overlay %s", DebugOverlayModeLogLabel(m_DebugOverlayMode));
			m_Logger.Write(FromKernel, LogNotice, line);
		}
		#endif
	}
	m_LastF12Pressed = key_state.f1_pressed;

	const boolean pause_edge = (key_state.pause_pressed && !m_LastPausePressed) ? TRUE : FALSE;
	const boolean esc_resume_edge =
		(key_state.menu_back_pressed && !m_LastMenuBackPressed && m_EmulationPaused && !m_PauseMenu.IsInSettings()) ? TRUE : FALSE;
	boolean pause_toggled = FALSE;
	if (pause_edge || esc_resume_edge)
	{
			if (m_LastPauseToggleUs == 0u || (now_us - m_LastPauseToggleUs) >= 200000ull)
			{
				m_EmulationPaused = m_EmulationPaused ? FALSE : TRUE;
				ClearRuntimeKeyboardState(TRUE);
				if (!m_EmulationPaused)
				{
					HandleSettingsMenuClosed();
					m_LastPauseWasInSettings = FALSE;
					m_PauseNeedsBackgroundRedraw = FALSE;
					m_PauseNeedsFullViewportClear = FALSE;
					m_PauseRenderSuppressed = TRUE;
#if SMSBARE_V9990_UART_TRACE
					if (m_Gfx9000Enabled)
					{
						m_Gfx9000ResumeTracePending = TRUE;
						if (!BackendQueueKernelLogDuringGfx9000Trace(0, "G9R: armed"))
						{
							m_Logger.Write(FromKernel, LogNotice, "G9R: armed");
						}
					}
#endif
				}
				else
				{
					/* Reset only on pause-enter to avoid one-frame root-layout flash on pause-exit. */
					m_PauseMenu.Reset();
					m_LastPauseWasInSettings = FALSE;
					m_PauseNeedsBackgroundRedraw = TRUE;
					m_PauseNeedsFullViewportClear = FALSE;
					m_PauseRenderSuppressed = FALSE;
					m_Gfx9000ResumeTracePending = FALSE;
				}
			if (!BackendQueueKernelLogDuringGfx9000Trace(0, m_EmulationPaused ? "Emulation PAUSED" : "Emulation RESUMED"))
			{
				m_Logger.Write(FromKernel, LogNotice, m_EmulationPaused ? "Emulation PAUSED" : "Emulation RESUMED");
			}
			m_LastPauseToggleUs = now_us;
			pause_toggled = TRUE;
		}
	}
	m_LastPausePressed = key_state.pause_pressed;
	m_LastMenuBackPressed = key_state.menu_back_pressed;
	if (pause_toggled)
	{
		/* Do not process menu navigation on the same edge that toggles pause state. */
		m_JoyBridgeBits = key_state.bridge_bits;
		m_JoyMappedBits = (u8) (~key_state.bridge_bits) & ComboKeyboardJoyMask();
		m_LastMenuInputPollUs = now_us;
		m_LastKeyboardModifiers = modifiers;
		for (unsigned i = 0u; i < 6u; ++i)
		{
			m_LastKeyboardRawKeys[i] = raw_keys[i];
		}
		return;
	}

	const TComboMenuAction menu_action = m_PauseMenu.ProcessInput(
		m_EmulationPaused,
		key_state.menu_up_pressed,
		key_state.menu_down_pressed,
		key_state.menu_page_up_pressed,
		key_state.menu_page_down_pressed,
		key_state.menu_left_pressed,
		key_state.menu_right_pressed,
		key_state.menu_enter_pressed,
		key_state.menu_back_pressed,
		key_state.menu_delete_pressed,
		(!same_report ? key_state.menu_jump_letter : '\0'),
		m_UsbGamepad.GetActiveControlCode());
	const boolean now_in_settings = m_PauseMenu.IsInSettings() ? TRUE : FALSE;
	if (m_EmulationPaused && now_in_settings && !m_LastPauseWasInSettings)
	{
		RefreshCoreListFromStorage();
		m_PauseMenu.RequestRedraw();
	}
	else if (m_EmulationPaused && !now_in_settings && m_LastPauseWasInSettings)
	{
		HandleSettingsMenuClosed();
	}
	m_LastPauseWasInSettings = now_in_settings;
	const boolean defer_heavy_action =
		(menu_action == ComboMenuActionCartridgeA)
		|| (menu_action == ComboMenuActionCartridgeB)
		|| (menu_action == ComboMenuActionDiskDriveA)
		|| (menu_action == ComboMenuActionDiskDriveB)
		|| (menu_action == ComboMenuActionCassette)
		|| (menu_action == ComboMenuActionToggleGfx9000)
		|| (menu_action == ComboMenuActionSoftReset)
		|| (menu_action == ComboMenuActionHardReset);
	if (menu_action == ComboMenuActionCartridgeA || menu_action == ComboMenuActionCartridgeB)
	{
		UpdateLoadDiagTelemetry(1991u); /* menu action propagated to kernel */
	}
	else if (menu_action == ComboMenuActionDiskDriveA || menu_action == ComboMenuActionDiskDriveB)
	{
		UpdateLoadDiagTelemetry(2991u); /* disk menu action propagated to kernel */
	}
	else if (menu_action == ComboMenuActionCassette)
	{
		UpdateLoadDiagTelemetry(3991u); /* cassette menu action propagated to kernel */
	}
	const boolean video_affecting_action =
		(menu_action == ComboMenuActionToggleOverscan)
		|| (menu_action == ComboMenuActionCycleScanlines)
		|| (menu_action == ComboMenuActionCycleScanlinesPrev)
		|| (menu_action == ComboMenuActionToggleColorArtifacts);
	if (m_PauseMenu.ConsumeBackgroundRestoreRequest())
	{
		/* Only viewport-affecting settings trigger fresh snapshot recapture. */
		if (m_EmulationPaused && m_BackendReady && video_affecting_action)
		{
			m_PauseNeedsBackgroundRedraw = TRUE;
			m_PauseNeedsFullViewportClear = TRUE;
		}
		m_PauseMenu.RequestRedraw();
	}
	if (defer_heavy_action)
	{
		/* Heavy media load path runs in main loop to avoid USB callback hard-locks. */
		if (menu_action == ComboMenuActionSoftReset || menu_action == ComboMenuActionHardReset)
		{
			/* High-priority actions must not be blocked by stale deferred browser ops. */
			m_PendingMenuAction = (unsigned) menu_action;
		}
		else if (m_PendingMenuAction == (unsigned) ComboMenuActionNone)
		{
			m_PendingMenuAction = (unsigned) menu_action;
		}
	}
	else
	{
		/* Keep settings toggles/cycles immediate so state never gets stale/overwritten. */
		HandleMenuAction(menu_action);
	}

	const boolean menu_key_pressed =
		key_state.menu_up_pressed
		|| key_state.menu_down_pressed
		|| key_state.menu_left_pressed
		|| key_state.menu_right_pressed
		|| key_state.menu_page_up_pressed
		|| key_state.menu_page_down_pressed
		|| key_state.menu_enter_pressed
		|| key_state.menu_delete_pressed
		|| (key_state.menu_jump_letter >= 'A' && key_state.menu_jump_letter <= 'Z');
	const boolean directional_key_pressed =
		key_state.menu_up_pressed
		|| key_state.menu_down_pressed
		|| key_state.menu_left_pressed
		|| key_state.menu_right_pressed;
	const boolean menu_key_event = (menu_key_pressed && !same_report) ? TRUE : FALSE;
	if (m_EmulationPaused && m_BackendReady
		&& (video_affecting_action || directional_key_pressed))
	{
		m_PauseNeedsBackgroundRedraw = TRUE;
		/* Full clear only for video parameter changes; directional keys need fresh snapshot but no full clear. */
		m_PauseNeedsFullViewportClear = video_affecting_action ? TRUE : FALSE;
		m_PauseMenu.RequestRedraw();
	}
	else if (m_EmulationPaused && (menu_key_event || menu_action != ComboMenuActionNone))
	{
		/* For non-video actions, reuse current snapshot and redraw only the menu. */
		m_PauseMenu.RequestRedraw();
	}

	m_JoyBridgeBits = key_state.bridge_bits;
	m_JoyMappedBits = (u8) (~key_state.bridge_bits) & ComboKeyboardJoyMask();
	m_LastMenuInputPollUs = now_us;
	m_LastKeyboardModifiers = modifiers;
	for (unsigned i = 0u; i < 6u; ++i)
	{
		m_LastKeyboardRawKeys[i] = raw_keys[i];
	}
}
