#ifndef SMSBARE_DEBUG_RUNTIME_GATE_H
#define SMSBARE_DEBUG_RUNTIME_GATE_H

#ifndef SMSBARE_ENABLE_DEBUG_OVERLAY
#define SMSBARE_ENABLE_DEBUG_OVERLAY 0
#endif

#if SMSBARE_ENABLE_DEBUG_OVERLAY

#include <circle/logger.h>
#include <circle/serial.h>

#else

#ifndef _circle_logger_h
#define _circle_logger_h
#endif

#ifndef _circle_serial_h
#define _circle_serial_h
#endif

#include <circle/device.h>
#include <circle/types.h>

enum TLogSeverity
{
    LogNotice = 0,
    LogWarning = 1,
    LogPanic = 2
};

class CLogger
{
public:
    template <typename... TArgs>
    CLogger(TArgs...)
    {
    }

    boolean Initialize(CDevice *target)
    {
        (void) target;
        return TRUE;
    }

    template <typename TText>
    void Write(const char *source, int severity, const TText &text)
    {
        (void) source;
        (void) severity;
        (void) text;
    }
};

class CSerialDevice
{
public:
    CSerialDevice(void)
    {
    }

    boolean Initialize(unsigned baudrate)
    {
        (void) baudrate;
        return TRUE;
    }

    int Write(const void *buffer, unsigned count)
    {
        (void) buffer;
        return (int) count;
    }
};

#endif

#endif
