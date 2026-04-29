#include "usb_hid_gamepad.h"

#include <string.h>

static const char FromUsbHidGamepad[] = "usb-hid-pad";
static const u8 kJoyBridgeRight = (u8) (1u << 0);
static const u8 kJoyBridgeLeft = (u8) (1u << 1);
static const u8 kJoyBridgeDown = (u8) (1u << 2);
static const u8 kJoyBridgeUp = (u8) (1u << 3);
static const u8 kJoyBridgeFireA = (u8) (1u << 4);
static const u8 kJoyBridgeFireB = (u8) (1u << 5);

CUsbHidGamepad *CUsbHidGamepad::s_Instance = 0;

typedef struct
{
	unsigned bit_index;
	const char *name;
} TKnownButtonName;

static const TKnownButtonName kKnownButtonNames[] = {
	{ 0u,  "HOME"  },
	{ 3u,  "L2"    },
	{ 4u,  "R2"    },
	{ 5u,  "L1"    },
	{ 6u,  "R1"    },
	{ 7u,  "Y"     },
	{ 8u,  "B"     },
	{ 9u,  "A"     },
	{ 10u, "X"     },
	{ 11u, "SEL"   },
	{ 12u, "L3"    },
	{ 13u, "R3"    },
	{ 14u, "START" },
	{ 15u, "UP"    },
	{ 16u, "RIGHT" },
	{ 17u, "DOWN"  },
	{ 18u, "LEFT"  },
	{ 19u, "PLUS"  },
	{ 20u, "MINUS" },
	{ 21u, "TPAD"  }
};

static const unsigned kKnownCapturePriority[] = {
	15u, 17u, 18u, 16u,
	9u, 8u, 10u, 7u,
	5u, 6u, 3u, 4u,
	11u, 14u, 12u, 13u,
	0u, 19u, 20u, 21u
};

static const char *FindKnownButtonName(unsigned bit_index)
{
	for (unsigned i = 0u; i < sizeof(kKnownButtonNames) / sizeof(kKnownButtonNames[0]); ++i)
	{
		if (kKnownButtonNames[i].bit_index == bit_index)
		{
			return kKnownButtonNames[i].name;
		}
	}

	return 0;
}

static void CopySmallText(char *dst, unsigned dst_size, const char *src)
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

static void ClearGamePadState(TGamePadState *state)
{
	if (state == 0)
	{
		return;
	}

	state->naxes = 0;
	for (unsigned i = 0u; i < MAX_AXIS; ++i)
	{
		state->axes[i].value = 0;
		state->axes[i].minimum = 0;
		state->axes[i].maximum = 0;
	}

	state->nhats = 0;
	for (unsigned i = 0u; i < MAX_HATS; ++i)
	{
		state->hats[i] = 0;
	}

	state->nbuttons = 0;
	state->buttons = 0u;
	for (unsigned i = 0u; i < 3u; ++i)
	{
		state->acceleration[i] = 0;
		state->gyroscope[i] = 0;
	}
}

static void CopyGamePadState(TGamePadState *dst, const TGamePadState *src)
{
	if (dst == 0 || src == 0)
	{
		return;
	}

	dst->naxes = src->naxes;
	for (unsigned i = 0u; i < MAX_AXIS; ++i)
	{
		dst->axes[i].value = src->axes[i].value;
		dst->axes[i].minimum = src->axes[i].minimum;
		dst->axes[i].maximum = src->axes[i].maximum;
	}

	dst->nhats = src->nhats;
	for (unsigned i = 0u; i < MAX_HATS; ++i)
	{
		dst->hats[i] = src->hats[i];
	}

	dst->nbuttons = src->nbuttons;
	dst->buttons = src->buttons;
	for (unsigned i = 0u; i < 3u; ++i)
	{
		dst->acceleration[i] = src->acceleration[i];
		dst->gyroscope[i] = src->gyroscope[i];
	}
}

static u8 NormalizeBridgeBits(u8 bits)
{
	if ((bits & (kJoyBridgeLeft | kJoyBridgeRight)) == (kJoyBridgeLeft | kJoyBridgeRight))
	{
		bits &= (u8) ~(kJoyBridgeLeft | kJoyBridgeRight);
	}
	if ((bits & (kJoyBridgeUp | kJoyBridgeDown)) == (kJoyBridgeUp | kJoyBridgeDown))
	{
		bits &= (u8) ~(kJoyBridgeUp | kJoyBridgeDown);
	}
	return bits;
}

static int AxisCenter(int minimum, int maximum)
{
	return minimum + ((maximum - minimum) / 2);
}

static int AxisDeadzone(int minimum, int maximum)
{
	int range = maximum - minimum;
	if (range < 0)
	{
		range = -range;
	}

	int deadzone = range / 5;
	if (deadzone < 8)
	{
		deadzone = 8;
	}

	return deadzone;
}

static boolean IsAxisNegative(const TGamePadState *state, unsigned axis_index)
{
	if (state == 0 || axis_index >= (unsigned) state->naxes || axis_index >= MAX_AXIS)
	{
		return FALSE;
	}

	const int center = AxisCenter(state->axes[axis_index].minimum, state->axes[axis_index].maximum);
	const int deadzone = AxisDeadzone(state->axes[axis_index].minimum, state->axes[axis_index].maximum);
	return (state->axes[axis_index].value - center) <= -deadzone ? TRUE : FALSE;
}

static boolean IsAxisPositive(const TGamePadState *state, unsigned axis_index)
{
	if (state == 0 || axis_index >= (unsigned) state->naxes || axis_index >= MAX_AXIS)
	{
		return FALSE;
	}

	const int center = AxisCenter(state->axes[axis_index].minimum, state->axes[axis_index].maximum);
	const int deadzone = AxisDeadzone(state->axes[axis_index].minimum, state->axes[axis_index].maximum);
	return (state->axes[axis_index].value - center) >= deadzone ? TRUE : FALSE;
}

static boolean IsRawButtonPressed(const TGamePadState *state, unsigned button_index)
{
	if (state == 0 || button_index >= 32u || button_index >= (unsigned) state->nbuttons)
	{
		return FALSE;
	}

	return (state->buttons & (1u << button_index)) != 0u ? TRUE : FALSE;
}

static void AppendUnsigned(char *dst, unsigned dst_size, unsigned *cursor, unsigned value)
{
	char digits[12];
	unsigned count = 0u;

	if (dst == 0 || dst_size == 0u || cursor == 0)
	{
		return;
	}

	do
	{
		digits[count++] = (char) ('0' + (value % 10u));
		value /= 10u;
	} while (value != 0u && count < sizeof(digits));

	while (count > 0u && *cursor + 1u < dst_size)
	{
		dst[(*cursor)++] = digits[--count];
	}
	dst[*cursor] = '\0';
}

CUsbHidGamepad::CUsbHidGamepad(void)
:	m_GamePad(0),
	m_GamePadRemovedHandle(0),
	m_RemovedEvent(FALSE),
	m_StateSeq(0u),
	m_Properties(0u),
	m_State(),
	m_MappingCodes{0u},
	m_HasUserMapping(FALSE)
{
	ClearGamePadState(&m_State);
	LoadDefaultMappings(0, 0u);
	s_Instance = this;
}

u16 CUsbHidGamepad::EncodeControlCode(unsigned type, unsigned payload)
{
	return (u16) (((type & 0x0Fu) << 12) | (payload & 0x0FFFu));
}

unsigned CUsbHidGamepad::DecodeControlType(u16 code)
{
	return (unsigned) ((code >> 12) & 0x0Fu);
}

unsigned CUsbHidGamepad::DecodeControlPayload(u16 code)
{
	return (unsigned) (code & 0x0FFFu);
}

u16 CUsbHidGamepad::ClampControlCode(u16 code)
{
	const unsigned type = DecodeControlType(code);
	const unsigned payload = DecodeControlPayload(code);

	switch (type)
	{
	case ControlTypeNone:
		return 0u;

	case ControlTypeKnownButton:
	case ControlTypeRawButton:
		return payload < 32u ? code : 0u;

	case ControlTypeHat:
		return ((payload / 8u) < MAX_HATS && (payload % 8u) < 8u) ? code : 0u;

	case ControlTypeAxisNegative:
	case ControlTypeAxisPositive:
		return payload < MAX_AXIS ? code : 0u;

	default:
		return 0u;
	}
}

void CUsbHidGamepad::FormatControlCode(u16 code, char *dst, unsigned dst_size)
{
	unsigned cursor = 0u;
	code = ClampControlCode(code);

	if (dst == 0 || dst_size == 0u)
	{
		return;
	}

	dst[0] = '\0';
	if (code == 0u)
	{
		CopySmallText(dst, dst_size, "NONE");
		return;
	}

	const unsigned type = DecodeControlType(code);
	const unsigned payload = DecodeControlPayload(code);

	switch (type)
	{
	case ControlTypeKnownButton:
	{
		const char *name = FindKnownButtonName(payload);
		if (name != 0)
		{
			CopySmallText(dst, dst_size, name);
		}
		else
		{
			CopySmallText(dst, dst_size, "K");
			cursor = 1u;
			AppendUnsigned(dst, dst_size, &cursor, payload);
		}
		break;
	}

	case ControlTypeRawButton:
		CopySmallText(dst, dst_size, "B");
		cursor = 1u;
		AppendUnsigned(dst, dst_size, &cursor, payload);
		break;

	case ControlTypeHat:
	{
		static const char *kHatDirNames[8] = { "U", "UR", "R", "DR", "D", "DL", "L", "UL" };
		const unsigned hat_index = payload / 8u;
		const unsigned hat_dir = payload % 8u;
		CopySmallText(dst, dst_size, "H");
		cursor = 1u;
		AppendUnsigned(dst, dst_size, &cursor, hat_index);
		if (cursor + 1u < dst_size)
		{
			dst[cursor++] = '-';
			dst[cursor] = '\0';
		}
		CopySmallText(dst + cursor, (cursor < dst_size) ? (dst_size - cursor) : 0u, kHatDirNames[hat_dir]);
		break;
	}

	case ControlTypeAxisNegative:
	case ControlTypeAxisPositive:
		CopySmallText(dst, dst_size, "A");
		cursor = 1u;
		AppendUnsigned(dst, dst_size, &cursor, payload);
		if (cursor + 1u < dst_size)
		{
			dst[cursor++] = (type == ControlTypeAxisNegative) ? '-' : '+';
			dst[cursor] = '\0';
		}
		break;

	default:
		CopySmallText(dst, dst_size, "NONE");
		break;
	}
}

boolean CUsbHidGamepad::AttachIfAvailable(CDeviceNameService &device_name_service, CLogger &logger)
{
	if (m_GamePad != 0)
	{
		return FALSE;
	}

	CUSBGamePadDevice *gamepad =
		(CUSBGamePadDevice *) device_name_service.GetDevice("upad1", FALSE);
	if (gamepad == 0)
	{
		return FALSE;
	}

	m_GamePad = gamepad;
	m_GamePadRemovedHandle = m_GamePad->RegisterRemovedHandler(GamePadRemovedHandler, this);
	m_Properties = m_GamePad->GetProperties();

	const TGamePadState *initial_state = m_GamePad->GetInitialState();
	if (initial_state != 0)
	{
		UpdateState(initial_state);
	}
	if (!m_HasUserMapping)
	{
		LoadDefaultMappings(initial_state, m_Properties);
	}

	m_GamePad->RegisterStatusHandler(GamePadStatusHandler);

	if (HasKnownMapping())
	{
		logger.Write(FromUsbHidGamepad, LogNotice, "USB gamepad attached (known mapping)");
	}
	else
	{
		logger.Write(FromUsbHidGamepad, LogNotice, "USB gamepad attached (generic fallback)");
	}

	return TRUE;
}

boolean CUsbHidGamepad::IsAttached(void) const
{
	return m_GamePad != 0 ? TRUE : FALSE;
}

boolean CUsbHidGamepad::HasKnownMapping(void) const
{
	return (m_Properties & GamePadPropertyIsKnown) != 0u ? TRUE : FALSE;
}

boolean CUsbHidGamepad::ConsumeRemovedEvent(void)
{
	if (!m_RemovedEvent)
	{
		return FALSE;
	}

	m_RemovedEvent = FALSE;
	return TRUE;
}

u16 CUsbHidGamepad::GetMappingCode(unsigned slot) const
{
	if (slot >= MapSlotCount)
	{
		return 0u;
	}

	return m_MappingCodes[slot];
}

void CUsbHidGamepad::GetMappingCodes(u16 *codes, unsigned count) const
{
	if (codes == 0 || count == 0u)
	{
		return;
	}

	for (unsigned i = 0u; i < count; ++i)
	{
		codes[i] = (i < MapSlotCount) ? m_MappingCodes[i] : 0u;
	}
}

void CUsbHidGamepad::SetMappingCodes(const u16 *codes, unsigned count, boolean user_defined)
{
	u16 next_codes[MapSlotCount];

	for (unsigned i = 0u; i < MapSlotCount; ++i)
	{
		next_codes[i] = (codes != 0 && i < count) ? ClampControlCode(codes[i]) : 0u;
		for (unsigned j = 0u; j < i; ++j)
		{
			if (next_codes[i] != 0u && next_codes[i] == next_codes[j])
			{
				next_codes[i] = 0u;
				break;
			}
		}
	}

	for (unsigned i = 0u; i < MapSlotCount; ++i)
	{
		m_MappingCodes[i] = next_codes[i];
	}

	m_HasUserMapping = user_defined ? TRUE : FALSE;
}

void CUsbHidGamepad::LoadDefaultMappings(const TGamePadState *state, unsigned properties)
{
	u16 defaults[MapSlotCount];

	if ((properties & GamePadPropertyIsKnown) != 0u)
	{
		defaults[MapSlotUp] = EncodeControlCode(ControlTypeKnownButton, 15u);
		defaults[MapSlotDown] = EncodeControlCode(ControlTypeKnownButton, 17u);
		defaults[MapSlotLeft] = EncodeControlCode(ControlTypeKnownButton, 18u);
		defaults[MapSlotRight] = EncodeControlCode(ControlTypeKnownButton, 16u);
		defaults[MapSlotButton1] = EncodeControlCode(ControlTypeKnownButton, 9u);
		defaults[MapSlotButton2] = EncodeControlCode(ControlTypeKnownButton, 8u);
	}
	else if (state != 0 && state->nhats > 0)
	{
		defaults[MapSlotUp] = EncodeControlCode(ControlTypeHat, 0u * 8u + 0u);
		defaults[MapSlotDown] = EncodeControlCode(ControlTypeHat, 0u * 8u + 4u);
		defaults[MapSlotLeft] = EncodeControlCode(ControlTypeHat, 0u * 8u + 6u);
		defaults[MapSlotRight] = EncodeControlCode(ControlTypeHat, 0u * 8u + 2u);
		defaults[MapSlotButton1] = EncodeControlCode(ControlTypeRawButton, 0u);
		defaults[MapSlotButton2] = EncodeControlCode(ControlTypeRawButton, 1u);
	}
	else
	{
		defaults[MapSlotUp] = EncodeControlCode(ControlTypeAxisNegative, GamePadAxisLeftY);
		defaults[MapSlotDown] = EncodeControlCode(ControlTypeAxisPositive, GamePadAxisLeftY);
		defaults[MapSlotLeft] = EncodeControlCode(ControlTypeAxisNegative, GamePadAxisLeftX);
		defaults[MapSlotRight] = EncodeControlCode(ControlTypeAxisPositive, GamePadAxisLeftX);
		defaults[MapSlotButton1] = EncodeControlCode(ControlTypeRawButton, 0u);
		defaults[MapSlotButton2] = EncodeControlCode(ControlTypeRawButton, 1u);
	}

	SetMappingCodes(defaults, MapSlotCount, FALSE);
}

boolean CUsbHidGamepad::IsControlActive(u16 code, const TGamePadState *state, unsigned properties) const
{
	code = ClampControlCode(code);
	if (code == 0u || state == 0)
	{
		return FALSE;
	}

	const unsigned type = DecodeControlType(code);
	const unsigned payload = DecodeControlPayload(code);

	switch (type)
	{
	case ControlTypeKnownButton:
		if ((properties & GamePadPropertyIsKnown) == 0u || payload >= 32u)
		{
			return FALSE;
		}
		return (state->buttons & (1u << payload)) != 0u ? TRUE : FALSE;

	case ControlTypeRawButton:
		return IsRawButtonPressed(state, payload);

	case ControlTypeHat:
	{
		const unsigned hat_index = payload / 8u;
		const unsigned hat_dir = payload % 8u;
		if (hat_index >= (unsigned) state->nhats || hat_index >= MAX_HATS)
		{
			return FALSE;
		}
		return state->hats[hat_index] == (int) hat_dir ? TRUE : FALSE;
	}

	case ControlTypeAxisNegative:
		return IsAxisNegative(state, payload);

	case ControlTypeAxisPositive:
		return IsAxisPositive(state, payload);

	default:
		return FALSE;
	}
}

u16 CUsbHidGamepad::DetectActiveControlCode(const TGamePadState *state, unsigned properties) const
{
	if (state == 0)
	{
		return 0u;
	}

	if ((properties & GamePadPropertyIsKnown) != 0u)
	{
		for (unsigned i = 0u; i < sizeof(kKnownCapturePriority) / sizeof(kKnownCapturePriority[0]); ++i)
		{
			const unsigned bit_index = kKnownCapturePriority[i];
			if ((state->buttons & (1u << bit_index)) != 0u)
			{
				return EncodeControlCode(ControlTypeKnownButton, bit_index);
			}
		}
	}

	for (unsigned hat_index = 0u; hat_index < (unsigned) state->nhats && hat_index < MAX_HATS; ++hat_index)
	{
		const int hat = state->hats[hat_index];
		if (hat >= 0 && hat <= 7)
		{
			return EncodeControlCode(ControlTypeHat, hat_index * 8u + (unsigned) hat);
		}
	}

	for (unsigned axis_index = 0u; axis_index < (unsigned) state->naxes && axis_index < MAX_AXIS; ++axis_index)
	{
		if (IsAxisNegative(state, axis_index))
		{
			return EncodeControlCode(ControlTypeAxisNegative, axis_index);
		}
		if (IsAxisPositive(state, axis_index))
		{
			return EncodeControlCode(ControlTypeAxisPositive, axis_index);
		}
	}

	for (unsigned button_index = 0u; button_index < (unsigned) state->nbuttons && button_index < 32u; ++button_index)
	{
		if (IsRawButtonPressed(state, button_index))
		{
			return EncodeControlCode(ControlTypeRawButton, button_index);
		}
	}

	return 0u;
}

u16 CUsbHidGamepad::GetActiveControlCode(void) const
{
	TGamePadState state;
	unsigned properties = 0u;
	if (!SnapshotState(&state, &properties))
	{
		return 0u;
	}

	return DetectActiveControlCode(&state, properties);
}

u8 CUsbHidGamepad::GetBridgeBits(void) const
{
	TGamePadState state;
	unsigned properties = 0u;
	if (!SnapshotState(&state, &properties))
	{
		return 0u;
	}

	u8 bits = 0u;
	if (IsControlActive(m_MappingCodes[MapSlotUp], &state, properties))
	{
		bits |= kJoyBridgeUp;
	}
	if (IsControlActive(m_MappingCodes[MapSlotDown], &state, properties))
	{
		bits |= kJoyBridgeDown;
	}
	if (IsControlActive(m_MappingCodes[MapSlotLeft], &state, properties))
	{
		bits |= kJoyBridgeLeft;
	}
	if (IsControlActive(m_MappingCodes[MapSlotRight], &state, properties))
	{
		bits |= kJoyBridgeRight;
	}
	if (IsControlActive(m_MappingCodes[MapSlotButton1], &state, properties))
	{
		bits |= kJoyBridgeFireA;
	}
	if (IsControlActive(m_MappingCodes[MapSlotButton2], &state, properties))
	{
		bits |= kJoyBridgeFireB;
	}

	return NormalizeBridgeBits(bits);
}

u8 CUsbHidGamepad::GetMenuBits(void) const
{
	TGamePadState state;
	unsigned properties = 0u;
	if (!SnapshotState(&state, &properties))
	{
		return 0u;
	}

	u8 bits = 0u;
	if ((properties & GamePadPropertyIsKnown) != 0u)
	{
		if ((state.buttons & GamePadButtonUp) != 0u)
		{
			bits |= MenuBitUp;
		}
		if ((state.buttons & GamePadButtonDown) != 0u)
		{
			bits |= MenuBitDown;
		}
		if ((state.buttons & GamePadButtonLeft) != 0u)
		{
			bits |= MenuBitLeft;
		}
		if ((state.buttons & GamePadButtonRight) != 0u)
		{
			bits |= MenuBitRight;
		}
		if ((state.buttons & GamePadButtonA) != 0u)
		{
			bits |= MenuBitSelect;
		}
		if ((state.buttons & GamePadButtonB) != 0u)
		{
			bits |= MenuBitBack;
		}
		if ((state.buttons & GamePadButtonStart) != 0u)
		{
			bits |= MenuBitPause;
		}
	}

	if (IsAxisNegative(&state, 1u))
	{
		bits |= MenuBitUp;
	}
	if (IsAxisPositive(&state, 1u))
	{
		bits |= MenuBitDown;
	}
	if (IsAxisNegative(&state, 0u))
	{
		bits |= MenuBitLeft;
	}
	if (IsAxisPositive(&state, 0u))
	{
		bits |= MenuBitRight;
	}

	for (unsigned hat_index = 0u; hat_index < (unsigned) state.nhats && hat_index < MAX_HATS; ++hat_index)
	{
		const int hat = state.hats[hat_index];
		switch (hat)
		{
		case 0: bits |= MenuBitUp; break;
		case 1: bits |= (MenuBitUp | MenuBitRight); break;
		case 2: bits |= MenuBitRight; break;
		case 3: bits |= (MenuBitDown | MenuBitRight); break;
		case 4: bits |= MenuBitDown; break;
		case 5: bits |= (MenuBitDown | MenuBitLeft); break;
		case 6: bits |= MenuBitLeft; break;
		case 7: bits |= (MenuBitUp | MenuBitLeft); break;
		default: break;
		}
	}

	if ((properties & GamePadPropertyIsKnown) == 0u)
	{
		if (IsRawButtonPressed(&state, 0u))
		{
			bits |= MenuBitSelect;
		}
		if (IsRawButtonPressed(&state, 1u))
		{
			bits |= MenuBitBack;
		}
	}

	if ((bits & (MenuBitLeft | MenuBitRight)) == (MenuBitLeft | MenuBitRight))
	{
		bits &= (u8) ~(MenuBitLeft | MenuBitRight);
	}
	if ((bits & (MenuBitUp | MenuBitDown)) == (MenuBitUp | MenuBitDown))
	{
		bits &= (u8) ~(MenuBitUp | MenuBitDown);
	}

	return bits;
}

void CUsbHidGamepad::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *state)
{
	(void) nDeviceIndex;

	if (s_Instance == 0 || state == 0)
	{
		return;
	}

	s_Instance->UpdateState(state);
}

void CUsbHidGamepad::GamePadRemovedHandler(CDevice *device, void *context)
{
	CUsbHidGamepad *self = (CUsbHidGamepad *) context;
	if (self == 0 || device != self->m_GamePad)
	{
		return;
	}

	self->m_GamePad = 0;
	self->m_GamePadRemovedHandle = 0;
	self->m_Properties = 0u;
	self->m_StateSeq += 1u;
	ClearGamePadState(&self->m_State);
	self->m_StateSeq += 1u;
	self->m_RemovedEvent = TRUE;
}

void CUsbHidGamepad::UpdateState(const TGamePadState *state)
{
	if (state == 0)
	{
		return;
	}

	m_StateSeq += 1u;
	CopyGamePadState(&m_State, state);
	m_StateSeq += 1u;
}

boolean CUsbHidGamepad::SnapshotState(TGamePadState *state, unsigned *properties) const
{
	if (state == 0 || properties == 0 || m_GamePad == 0)
	{
		return FALSE;
	}

	for (unsigned attempt = 0u; attempt < 3u; ++attempt)
	{
		const unsigned seq_before = m_StateSeq;
		if (seq_before & 1u)
		{
			continue;
		}

		CopyGamePadState(state, &m_State);
		*properties = m_Properties;

		const unsigned seq_after = m_StateSeq;
		if (seq_before == seq_after && !(seq_after & 1u))
		{
			return TRUE;
		}
	}

	ClearGamePadState(state);
	*properties = 0u;
	return FALSE;
}
