#
# Topology for generic Cannonlake board with RT274
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Cannonlake DSP configuration
include(`platform/intel/cnl.m4')

#
# Define the pipelines
#
# PCM0 <---> volume <--->  SSP0
#
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAI configuration
#
# SSP port 0 is our only pipeline DAI
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP0 using 2 periods
# Buffers use s24le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, SSP0-Codec,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s24le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, SSP0-Codec,
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency
PCM_DUPLEX_ADD(Passthrough, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

#
# BE configurations - overrides config in ACPI if present
#
DAI_CONFIG(SSP, 0, 1, SSP0-Codec,
	   SSP_CONFIG(DSP_B, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_consumer),
		      SSP_CLOCK(fsync, 48000, codec_consumer),
		      SSP_TDM(4, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 0, 24)))

VIRTUAL_DAPM_ROUTE_OUT(codec0_out, SSP, 0, OUT, 0)
VIRTUAL_DAPM_ROUTE_OUT(codec1_out, SSP, 0, OUT, 1)
VIRTUAL_DAPM_ROUTE_OUT(ssp0 Tx, SSP, 0, OUT, 2)
VIRTUAL_DAPM_ROUTE_OUT(Capture, SSP, 0, OUT, 3)
VIRTUAL_DAPM_ROUTE_OUT(SoC DMIC, SSP, 0, OUT, 4)
VIRTUAL_DAPM_ROUTE_IN(codec0_in, SSP, 0, IN, 5)
VIRTUAL_DAPM_ROUTE_OUT(ssp2_out, SSP, 0, OUT, 6)
VIRTUAL_DAPM_ROUTE_IN(ssp2_in, SSP, 0, IN, 7)
VIRTUAL_DAPM_ROUTE_OUT(ssp1_out, SSP, 0, OUT, 8)
VIRTUAL_WIDGET(DMIC01 Rx, out_drv, 9)
VIRTUAL_WIDGET(DMic, out_drv, 10)
VIRTUAL_WIDGET(dmic01_hifi, out_drv, 11)
VIRTUAL_WIDGET(ssp0 Rx, out_drv, 12)
VIRTUAL_WIDGET(ssp1_out, out_drv, 13)
VIRTUAL_WIDGET(ssp2_out, out_drv, 14)
VIRTUAL_WIDGET(ssp2_in, out_drv, 15)
VIRTUAL_WIDGET(ssp2 Rx, out_drv, 16)
VIRTUAL_WIDGET(ssp2 Tx, out_drv, 17)
VIRTUAL_WIDGET(Dummy Playback, out_drv, 18)
VIRTUAL_WIDGET(Dummy Capture, out_drv, 19)
