#ifndef SMSBARE_CIRCLE_SMSBARE_KERNEL_H
#define SMSBARE_CIRCLE_SMSBARE_KERNEL_H

#include "combo_screen_device.h"
#include "backend/runtime/runtime.h"
#include "tools/usb_hid_keyboard.h"
#include "tools/usb_hid_gamepad.h"
#include "gfx_textbox.h"
#include "combo_menu.h"
#include "media_catalog.h"
#include "tools/debug/debug_runtime_gate.h"

#include <circle/actled.h>
#include <circle/cputhrottle.h>
#include <circle/devicenameservice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/koptions.h>
#include <circle/nulldevice.h>
#include <circle/sched/scheduler.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
extern "C" {
#include <fatfs/ff.h>
}

enum TComboShutdownMode {
    ComboShutdownNone,
    ComboShutdownHalt,
    ComboShutdownReboot
};

class CComboKernel
{
public:
    CComboKernel(void);
    ~CComboKernel(void);

    boolean Initialize(void);
    TComboShutdownMode Run(void);
    static unsigned DebugRepairTimerIrqBindingCurrent(unsigned stage);

private:
    void RefillAudioQueue(void);
    void PrimeAudioSilenceBeforeReset(void);
    boolean PerformMenuSoftReset(unsigned requested_diag, unsigned success_diag, unsigned fail_diag);
    boolean PerformMenuHardReset(unsigned requested_diag, unsigned success_diag, unsigned fail_diag);
    void QueueMenuHardReset(void);
    void ProcessPendingMenuHardReset(void);
    void InitializeAudioOutput(boolean backend_audio_enabled);
    void GenerateSamples(s16 *buffer, unsigned frames);
    void DrawDebugOverlay(unsigned tick);
    void ClearDebugOverlay(void);
    void WriteDebugOverlayLine(unsigned row, const char *text);
    void BootTrace(const char *tag);
    void WriteScreenText(const char *text);
    void LogVdpTraceUart(void);
    void LogAudioPortTraceUart(void);
    void LogAudioQueueUart(void);
    void LogVideoPacingUart(void);
    void LogVideoModeTransitionUart(void);
    void LogVideoHiresHostDebugUart(void);
    void LogCpuClockStatus(const char *phase);
    void HandleSystemThrottled(TSystemThrottledState state);
    void LogKmgTraceUart(void);
    void ResetUartPerfTelemetry(void);
    void ResolveViewportBox(unsigned src_width, unsigned src_height,
                            const BackendVideoPresentation *presentation,
                            unsigned *out_x, unsigned *out_y,
                            unsigned *out_w, unsigned *out_h);
    void ComputeViewportBox(unsigned src_width, unsigned src_height,
                            const BackendVideoPresentation *presentation,
                            unsigned *out_x, unsigned *out_y,
                            unsigned *out_w, unsigned *out_h) const;
    void ClearPresentationSurfaces(void);
    void BlitBackendFrame(boolean force_redraw);
    void HandleMenuAction(TComboMenuAction action);
    void ApplyRuntimeSettingsToBackend(void);
    boolean ApplyGfx9000AutoForActiveCartridge(const char *source, boolean reset_backend);
    void SyncRefreshRateStateFromRuntime(void);
    void RefreshSettingsUiAfterAction(void);
    void ResetEmulationRuntimeState(void);
    void ExitPauseForBackendAction(void);
    void ClearRuntimeKeyboardState(boolean drain_pending_reports);
    void UpdateJoystickFromRaw(unsigned char modifiers, const unsigned char raw_keys[6]);
    u8 ResolveRuntimeJoystickBits(void) const;
    void PushKeyboardStateToRuntime(void);
    void SetDebugOverlayMode(unsigned mode);
    void SetUartTelemetryEnabled(boolean enabled);
    void UpdateSettingsSaveTelemetry(unsigned stage, unsigned code, unsigned aux);
    void UpdateLoadDiagTelemetry(unsigned stage);
    const char *CurrentCoreName(void) const;
    void RefreshCoreListFromStorage(void);
    void CycleCoreSelection(boolean reverse);
    void HandleSettingsMenuClosed(void);
    boolean PersistSelectedCoreToBootConfig(void);
    void ApplyFixedScreenMode(const char *source);
    boolean InitializeSettingsStorage(void);
    boolean MountSettingsFileSystem(const char *phase);
    unsigned BuildSettingsPayload(char *buffer, unsigned buffer_size);
    void InitializeDebugRuntime(void);
    void ProcessDeferredBootTasks(void);
    void LoadSettingsFromStorage(void);
    void SaveSettingsToStorage(void);
    boolean EnsureStateIoBuffer(unsigned min_capacity);
    boolean EnsureBackendPresentBuffer(unsigned min_pixels);
    boolean EnsureStateStorageDirectory(void);
    void SaveEmulatorStateToStorage(void);
    void LoadEmulatorStateFromStorage(void);
    void NormalizeSettingsForBuild(void);
    unsigned DebugRepairTimerIrqBinding(unsigned stage);

    static void KeyPressedHandler(const char *text);
    static void SystemThrottledHandler(TSystemThrottledState state, void *param);

private:
	static const unsigned kBlitMapMaxAxis = 2048u;
	static const unsigned kCoreListMax = 32u;
	static const unsigned kCoreNameMax = 64u;

    CActLED m_ActLED;
    CKernelOptions m_Options;
    CDeviceNameService m_DeviceNameService;
    CCPUThrottle m_CPUThrottle;
    CComboScreenDevice m_Screen;
    CGfxTextBoxRenderer m_GfxText;
    CSerialDevice m_Serial;
    CNullDevice m_NullLog;
    CExceptionHandler m_ExceptionHandler;
    CInterruptSystem m_Interrupt;
    CTimer m_Timer;
    CLogger m_Logger;
    CScheduler m_Scheduler;
    CUSBHCIDevice m_USBHCI;
    CEMMCDevice m_EMMC;
    FATFS m_SettingsFileSystem;

    CSoundBaseDevice *m_Sound;
    CUsbHidGamepad m_UsbGamepad;
    CUsbHidKeyboard m_UsbKeyboard;

    u32 m_AudioPhase;
    u32 m_AudioStep;

    unsigned m_PlugAndPlayCount;
    unsigned m_KeyEventCount;
    unsigned m_LastKeyCode;
    unsigned m_AudioWriteOps;
    unsigned m_AudioDropOps;
    unsigned m_AudioOverwriteSeen;
    unsigned m_AudioTrimSeen;
    unsigned m_AudioUnderrunSeen;
    unsigned m_AudioShortPullSeen;
    unsigned m_AudioPeak;
    s16 m_LastAudioSample;
    unsigned m_SoundStartAttempts;
    unsigned m_SoundStartFailures;
    unsigned m_LastQueueAvail;
    unsigned m_LastQueueFree;
    unsigned m_AudioTargetPeakRuntime;
    unsigned m_AudioClipMaxRuntime;
    unsigned m_AudioOutputGainPct;
    unsigned m_BootToneFramesRemaining;
    unsigned m_UsbInitRetryTick;
    boolean m_AudioInitAttempted;
    boolean m_UsbReady;
    u8 m_JoyBridgeBits;
    u8 m_JoyMappedBits;
    unsigned m_DebugOverlayMode;
    boolean m_DebugOverlayUnlocked;
    boolean m_ScreenReady;
    boolean m_EmulationPaused;
    boolean m_LastF12Pressed;
    boolean m_LastPauseWasInSettings;
    boolean m_LastPausePressed;
    boolean m_LastMenuBackPressed;
    boolean m_PauseNeedsBackgroundRedraw;
    boolean m_PauseNeedsFullViewportClear;
    boolean m_ForceViewportFullClearNextFrame;
    boolean m_PauseRenderSuppressed;
    volatile boolean m_MenuHardResetPending;
    boolean m_BackendEnabled;
    boolean m_BackendReady;
    const char *m_EmulatorBackendName;
    unsigned m_BackendInitFailCode;
    unsigned long m_BackendFrames;
    int m_BackendLastError;
    int m_BackendLastPC;
    unsigned m_BackendPcRepeatCount;
    boolean m_BackendPcLoopLogged;
    unsigned m_BackendVideoSeq;
    unsigned m_BackendVdpTraceSeq;
    unsigned m_BackendVdpTraceDropped;
    unsigned m_BackendAudioPortTraceSeq;
    unsigned m_BackendAudioPortTraceDropped;
    unsigned m_BackendKmgTraceSeq;
    unsigned m_BackendKmgTraceDropped;
    u64 m_LastAudioQueueLogUs;
    u64 m_LastVideoPacingLogUs;
    u64 m_BackendFpsWindowStartUs;
    u64 m_LastPauseToggleUs;
    u64 m_LastMenuInputPollUs;
    u64 m_LastOverlayUs;
    unsigned m_UartAudioQueueAvailMin;
    unsigned m_UartAudioLowWaterHits;
    unsigned m_UartAudioRefillUsLast;
    unsigned m_UartAudioRefillUsPeak;
    unsigned m_UartVideoCatchupLast;
    unsigned m_UartVideoCatchupPeak;
    unsigned m_UartVideoCatchupCapCount;
    unsigned m_UartVideoLateUsLast;
    unsigned m_UartVideoLateUsPeak;
    unsigned m_UartVideoStepUsLast;
    unsigned m_UartVideoStepUsPeak;
    unsigned m_UartVideoBlitUsLast;
    unsigned m_UartVideoBlitUsPeak;
    unsigned m_LastVideoModeScreen;
    unsigned m_LastVideoModeLogicalWidth;
    unsigned m_LastVideoModeLogicalHeight;
    unsigned m_HiresHostTraceFramesRemaining;
    unsigned long m_BackendFpsWindowStartFrames;
    unsigned m_BackendFpsX10;
    unsigned m_FramePacingErrAvgUs;
    unsigned m_FramePacingErrMaxUs;
    unsigned m_BackendBlankFrames;
    boolean m_LastVideoModeLogValid;
    boolean m_LastVideoModeHires;
    boolean m_LastVideoModeInterlaced;
    unsigned m_DebugOverlayLineLengths[20];
    unsigned m_InitramfsFiles;
    unsigned m_BackendBoxX;
    unsigned m_BackendBoxY;
    unsigned m_BackendBoxW;
    unsigned m_BackendBoxH;
    unsigned m_BlitMapSrcW;
    unsigned m_BlitMapSrcH;
    unsigned m_BlitMapDstW;
    unsigned m_BlitMapDstH;
    unsigned m_BlitRunCount;
    boolean m_BlitMapValid;
    boolean m_BlitMapWovenHiRes;
    u16 m_BlitXMap[kBlitMapMaxAxis];
    u16 m_BlitYMap[kBlitMapMaxAxis];
    u16 m_BlitRunSrcX[kBlitMapMaxAxis];
    u16 m_BlitRunDstX[kBlitMapMaxAxis];
    u16 m_BlitRunLen[kBlitMapMaxAxis];
    CComboPauseMenu m_PauseMenu;
    unsigned m_OverscanPercent;
    boolean m_ScaleMaxFitEnabled;
    unsigned m_Language;
    unsigned m_ScanlineMode;
    boolean m_ColorArtifactsEnabled;
    boolean m_Gfx9000Enabled;
    boolean m_Gfx9000AutoCfgApplied;
    unsigned m_RefreshRateHz;
    boolean m_DiskRomEnabled;
    char m_CoreNames[kCoreListMax][kCoreNameMax];
    char m_BootCoreName[kCoreNameMax];
    unsigned m_CoreCount;
    unsigned m_CoreIndex;
    boolean m_CoreListLoaded;
    boolean m_CoreSwitchPending;
    boolean m_CoreRebootRequested;
    unsigned m_BootMode;
    unsigned m_MachineProfile;
    unsigned m_ProcessorMode;
    unsigned m_RamMapperKb;
    unsigned m_MegaRamKb;
    unsigned char m_LastKeyboardModifiers;
    unsigned char m_LastKeyboardRawKeys[6];
    unsigned char m_RuntimeKeyboardModifiers;
    unsigned char m_RuntimeKeyboardRawKeys[6];
    boolean m_RuntimeKeyboardDispatchValid;
    unsigned m_RuntimeKeyboardReplayBudget;
    unsigned char m_RuntimeKeyboardDispatchedModifiers;
    unsigned char m_RuntimeKeyboardDispatchedRawKeys[6];
    boolean m_FmMusicEnabled;
    unsigned m_FmLayerGainPct;
    boolean m_SccCartEnabled;
    boolean m_SccDualCartEnabled;
    boolean m_Gfx9000ToggleTracePending;
    boolean m_Gfx9000ResumeTracePending;
    u64 m_FrameIntervalUs;
    boolean m_FramePacingResetPending;
    boolean m_BackendFallbackPattern;
    boolean m_TestToneEnabled;
    boolean m_MmuEnabled;
    boolean m_DCacheEnabled;
    boolean m_ICacheEnabled;
    boolean m_SettingsStorageReady;
    boolean m_SettingsFatFsMounted;
    boolean m_SettingsFatFsMountPending;
    boolean m_UartTelemetryEnabled;
    boolean m_SettingsLoadPending;
    boolean m_InitramfsDeferredMountPending;
    boolean m_SettingsSavePending;
    boolean m_StateSavePending;
    boolean m_StateLoadPending;
    boolean m_MediaCatalogStarted;
    boolean m_MediaCatalogSummaryLogged;
    volatile unsigned m_PendingMenuAction;
    unsigned m_SettingsSaveStage;
    unsigned m_SettingsSaveCode;
    unsigned m_SettingsSaveAux;
    u64 m_BootStartUs;
    unsigned m_BootToBackendStartMs;
    unsigned m_BootToBackendMs;
    unsigned m_BootFirstBlitMs;
    unsigned m_BootUsbReadyMs;
    unsigned m_BootBt14Ms;
    unsigned m_BootFatDeferredMs;
    u8 *m_StateIoBuffer;
    unsigned m_StateIoBufferCapacity;
    u16 *m_BackendPresentBuffer;
    unsigned m_BackendPresentBufferPixels;

    static CComboKernel *s_Instance;
};

#endif
