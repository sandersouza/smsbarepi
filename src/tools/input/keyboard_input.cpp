#include "keyboard_input.h"

static const u8 kHidKeyRight = 0x4F;
static const u8 kHidKeyLeft = 0x50;
static const u8 kHidKeyDown = 0x51;
static const u8 kHidKeyUp = 0x52;
static const u8 kHidKeyPageUp = 0x4B;
static const u8 kHidKeyPageDown = 0x4E;
static const u8 kHidKeyEnter = 0x28;
static const u8 kHidKeyKeypadEnter = 0x58;
static const u8 kHidKeyEsc = 0x29;
static const u8 kHidKeyDel = 0x4C;
static const u8 kHidKeyF11 = 0x44;
static const u8 kHidKeyF12 = 0x45;

u8 ComboKeyboardJoyMask(void)
{
	return 0u;
}

void ComboDecodeKeyboardRaw(unsigned char modifiers,
				      const unsigned char raw_keys[6],
				      TComboKeyboardInputState *out_state)
{
	(void) modifiers;

	if (out_state == 0)
	{
		return;
	}

	out_state->bridge_bits = 0;
	out_state->f1_pressed = FALSE;
	out_state->pause_pressed = FALSE;
	out_state->menu_up_pressed = FALSE;
	out_state->menu_down_pressed = FALSE;
	out_state->menu_left_pressed = FALSE;
	out_state->menu_right_pressed = FALSE;
	out_state->menu_page_up_pressed = FALSE;
	out_state->menu_page_down_pressed = FALSE;
	out_state->menu_enter_pressed = FALSE;
	out_state->menu_back_pressed = FALSE;
	out_state->menu_delete_pressed = FALSE;
	out_state->menu_jump_letter = '\0';

	if (raw_keys == 0)
	{
		return;
	}

	for (unsigned i = 0; i < 6; ++i)
	{
		const u8 key = raw_keys[i];
		if (key == kHidKeyF11)
		{
			out_state->f1_pressed = TRUE;
		}
		if (key == kHidKeyF12)
		{
			out_state->pause_pressed = TRUE;
		}
		if (key == kHidKeyUp)
		{
			out_state->menu_up_pressed = TRUE;
		}
		if (key == kHidKeyDown)
		{
			out_state->menu_down_pressed = TRUE;
		}
		if (key == kHidKeyLeft)
		{
			out_state->menu_left_pressed = TRUE;
		}
		if (key == kHidKeyRight)
		{
			out_state->menu_right_pressed = TRUE;
		}
		if (key == kHidKeyPageUp)
		{
			out_state->menu_page_up_pressed = TRUE;
		}
		if (key == kHidKeyPageDown)
		{
			out_state->menu_page_down_pressed = TRUE;
		}
		if (key == kHidKeyEnter || key == kHidKeyKeypadEnter)
		{
			out_state->menu_enter_pressed = TRUE;
		}
		if (key == kHidKeyEsc)
		{
			out_state->menu_back_pressed = TRUE;
		}
		if (key == kHidKeyDel)
		{
			out_state->menu_delete_pressed = TRUE;
		}
		if (key >= 0x04u && key <= 0x1Du)
		{
			out_state->menu_jump_letter = (char) ('A' + (key - 0x04u));
		}

	}
}
