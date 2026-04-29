#include "kernel.h"
#include <circle/startup.h>

static CComboKernel g_Kernel;

int main(void)
{
	if (!g_Kernel.Initialize())
	{
		halt();
		return EXIT_HALT;
	}

	const TComboShutdownMode shutdown_mode = g_Kernel.Run();

	if (shutdown_mode == ComboShutdownReboot)
	{
		reboot();
		return EXIT_REBOOT;
	}

	halt();
	return EXIT_HALT;
}
