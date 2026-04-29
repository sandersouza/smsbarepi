#ifndef SMSBARE_CIRCLE_SMSBARE_KEYBOARD_INPUT_H
#define SMSBARE_CIRCLE_SMSBARE_KEYBOARD_INPUT_H

#include <circle/types.h>

struct TComboKeyboardInputState
{
	u8 bridge_bits;
	boolean f1_pressed;
	boolean pause_pressed;
	boolean menu_up_pressed;
	boolean menu_down_pressed;
	boolean menu_left_pressed;
	boolean menu_right_pressed;
	boolean menu_page_up_pressed;
	boolean menu_page_down_pressed;
	boolean menu_enter_pressed;
	boolean menu_back_pressed;
	boolean menu_delete_pressed;
	char menu_jump_letter;
};

u8 ComboKeyboardJoyMask(void);
void ComboDecodeKeyboardRaw(unsigned char modifiers,
				      const unsigned char raw_keys[6],
				      TComboKeyboardInputState *out_state);

#endif
