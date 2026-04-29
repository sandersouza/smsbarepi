#include "../../../../../third_party/smsplus/shared.h"

#define SMSBARE_PSG_NOISE_INITIAL_STATE 0x8000

static const int SmsBarePsgVolumeValues[2][16] = {
    {892, 892, 892, 760, 623, 497, 404, 323, 257, 198, 159, 123, 96, 75, 60, 0},
    {892, 774, 669, 575, 492, 417, 351, 292, 239, 192, 150, 113, 80, 50, 24, 0}
};

static SN76489_Context g_smsbare_psg[MAX_SN76489];

static int smsbare_psg_valid_index(int which)
{
    return which >= 0 && which < MAX_SN76489;
}

static int smsbare_psg_period_from_register(UINT16 value)
{
    const int period = (int) (value & 0x03FF);
    return period <= 0 ? 1 : period;
}

static int smsbare_psg_noise_period(const SN76489_Context *p)
{
    const int mode = p->Registers[6] & 0x03;
    if (mode == 3)
    {
        return smsbare_psg_period_from_register(p->Registers[4]);
    }
    return 0x10 << mode;
}

static int smsbare_psg_feedback_parity(UINT16 value, int feedback_mask)
{
    unsigned bits = (unsigned) value & (unsigned) feedback_mask;
    bits ^= bits >> 8;
    bits ^= bits >> 4;
    bits ^= bits >> 2;
    bits ^= bits >> 1;
    return (int) (bits & 1u);
}

static int smsbare_psg_noise_feedback(SN76489_Context *p)
{
    if ((p->Registers[6] & 0x04) == 0)
    {
        return p->NoiseShiftRegister & 1;
    }
    return smsbare_psg_feedback_parity(p->NoiseShiftRegister, p->WhiteNoiseFeedback);
}

static int smsbare_psg_clip16(int value)
{
    if (value > 32767)
    {
        return 32767;
    }
    if (value < -32768)
    {
        return -32768;
    }
    return value;
}

static void smsbare_psg_reset_noise(SN76489_Context *p)
{
    p->NoiseShiftRegister = SMSBARE_PSG_NOISE_INITIAL_STATE;
    p->NoiseFreq = (INT16) smsbare_psg_noise_period(p);
    p->ToneFreqVals[3] = p->NoiseFreq;
    p->ToneFreqPos[3] = 1;
}

void SN76489_Init(int which, int PSGClockValue, int SamplingRate)
{
    SN76489_Context *p;

    if (!smsbare_psg_valid_index(which))
    {
        return;
    }

    p = &g_smsbare_psg[which];
    memset(p, 0, sizeof(*p));
    if (SamplingRate <= 0)
    {
        SamplingRate = 48000;
    }
    p->dClock = (float) PSGClockValue / 16.0f / (float) SamplingRate;
    SN76489_Config(which, MUTE_ALLON, BOOST_ON, VOL_FULL, FB_SEGAVDP);
    SN76489_Reset(which);
}

void SN76489_Reset(int which)
{
    SN76489_Context *p;
    int i;

    if (!smsbare_psg_valid_index(which))
    {
        return;
    }

    p = &g_smsbare_psg[which];
    p->PSGStereo = 0xFF;
    p->Clock = 0.0f;
    p->NumClocksForSample = 0;
    p->LatchedRegister = 0;

    for (i = 0; i < 4; ++i)
    {
        p->Registers[i * 2] = 1;
        p->Registers[i * 2 + 1] = 0x0F;
        p->ToneFreqVals[i] = 1;
        p->ToneFreqPos[i] = 1;
        p->Channels[i] = 0;
        p->IntermediatePos[i] = LONG_MIN;
    }

    p->Registers[6] = 0x01;
    p->Registers[7] = 0x0F;
    smsbare_psg_reset_noise(p);
}

void SN76489_Shutdown(void)
{
}

void SN76489_Config(int which, int mute, int boost, int volume, int feedback)
{
    SN76489_Context *p;

    if (!smsbare_psg_valid_index(which))
    {
        return;
    }

    p = &g_smsbare_psg[which];
    p->Mute = mute;
    p->BoostNoise = boost;
    p->VolumeArray = volume == VOL_TRUNC ? VOL_TRUNC : VOL_FULL;
    p->WhiteNoiseFeedback = feedback;
}

void SN76489_SetContext(int which, uint8 *data)
{
    if (!smsbare_psg_valid_index(which) || data == 0)
    {
        return;
    }
    memcpy(&g_smsbare_psg[which], data, sizeof(SN76489_Context));
}

void SN76489_GetContext(int which, uint8 *data)
{
    if (!smsbare_psg_valid_index(which) || data == 0)
    {
        return;
    }
    memcpy(data, &g_smsbare_psg[which], sizeof(SN76489_Context));
}

uint8 *SN76489_GetContextPtr(int which)
{
    if (!smsbare_psg_valid_index(which))
    {
        return 0;
    }
    return (uint8 *) &g_smsbare_psg[which];
}

int SN76489_GetContextSize(void)
{
    return sizeof(SN76489_Context);
}

void SN76489_Write(int which, int data)
{
    SN76489_Context *p;

    if (!smsbare_psg_valid_index(which))
    {
        return;
    }

    p = &g_smsbare_psg[which];
    if ((data & 0x80) != 0)
    {
        p->LatchedRegister = (data >> 4) & 0x07;
        p->Registers[p->LatchedRegister] =
            (UINT16) ((p->Registers[p->LatchedRegister] & 0x03F0) | (data & 0x0F));
    }
    else if ((p->LatchedRegister % 2) == 0 && p->LatchedRegister < 5)
    {
        p->Registers[p->LatchedRegister] =
            (UINT16) ((p->Registers[p->LatchedRegister] & 0x000F) | ((data & 0x3F) << 4));
    }
    else
    {
        p->Registers[p->LatchedRegister] = (UINT16) (data & 0x0F);
    }

    switch (p->LatchedRegister)
    {
    case 0:
    case 2:
    case 4:
        if ((p->Registers[p->LatchedRegister] & 0x03FF) == 0)
        {
            p->Registers[p->LatchedRegister] = 1;
        }
        break;
    case 6:
        smsbare_psg_reset_noise(p);
        break;
    default:
        break;
    }
}

void SN76489_GGStereoWrite(int which, int data)
{
    if (!smsbare_psg_valid_index(which))
    {
        return;
    }
    g_smsbare_psg[which].PSGStereo = data & 0xFF;
}

void SN76489_Update(int which, INT16 **buffer, int length)
{
    SN76489_Context *p;
    int j;

    if (!smsbare_psg_valid_index(which) || buffer == 0 || buffer[0] == 0 || buffer[1] == 0 || length <= 0)
    {
        return;
    }

    p = &g_smsbare_psg[which];
    for (j = 0; j < length; ++j)
    {
        int i;
        int left = 0;
        int right = 0;

        p->Clock += p->dClock;
        p->NumClocksForSample = (int) p->Clock;
        if (p->NumClocksForSample <= 0)
        {
            p->NumClocksForSample = 1;
        }
        p->Clock -= (float) p->NumClocksForSample;

        for (i = 0; i < 3; ++i)
        {
            const int period = smsbare_psg_period_from_register(p->Registers[i * 2]);
            p->ToneFreqVals[i] -= (INT16) p->NumClocksForSample;
            while (p->ToneFreqVals[i] <= 0)
            {
                p->ToneFreqVals[i] += (INT16) period;
                if (period > 1)
                {
                    p->ToneFreqPos[i] = (INT8) -p->ToneFreqPos[i];
                }
                else
                {
                    p->ToneFreqPos[i] = 1;
                }
            }
            p->Channels[i] = (INT16) (((p->Mute >> i) & 1) *
                SmsBarePsgVolumeValues[p->VolumeArray][p->Registers[i * 2 + 1] & 0x0F] *
                (int) p->ToneFreqPos[i]);
            p->IntermediatePos[i] = LONG_MIN;
        }

        p->NoiseFreq = (INT16) smsbare_psg_noise_period(p);
        p->ToneFreqVals[3] -= (INT16) p->NumClocksForSample;
        while (p->ToneFreqVals[3] <= 0)
        {
            const int feedback = smsbare_psg_noise_feedback(p);
            p->ToneFreqVals[3] += p->NoiseFreq > 0 ? p->NoiseFreq : 1;
            p->ToneFreqPos[3] = (INT8) -p->ToneFreqPos[3];
            if (p->ToneFreqPos[3] > 0)
            {
                p->NoiseShiftRegister = (UINT16) ((p->NoiseShiftRegister >> 1) | (feedback << 15));
            }
        }

        p->Channels[3] = (INT16) (((p->Mute >> 3) & 1) *
            SmsBarePsgVolumeValues[p->VolumeArray][p->Registers[7] & 0x0F] *
            ((p->NoiseShiftRegister & 1) != 0 ? 1 : -1));
        if (p->BoostNoise)
        {
            p->Channels[3] = (INT16) (p->Channels[3] << 1);
        }
        p->IntermediatePos[3] = LONG_MIN;

        for (i = 0; i < 4; ++i)
        {
            if (((p->PSGStereo >> (i + 4)) & 1) != 0)
            {
                left += p->Channels[i];
            }
            if (((p->PSGStereo >> i) & 1) != 0)
            {
                right += p->Channels[i];
            }
        }

        buffer[0][j] = (INT16) smsbare_psg_clip16(left);
        buffer[1][j] = (INT16) smsbare_psg_clip16(right);
    }
}
