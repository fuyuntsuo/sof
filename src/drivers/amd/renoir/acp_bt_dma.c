// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Anup Kulkarni <anup.kulkarni@amd.com>
//		Bala Kishore <balakishore.pati@amd.com>

#include <sof/atomic.h>
#include <sof/audio/component.h>
#include <sof/bit.h>
#include <sof/drivers/acp_dai_dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/spinlock.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/fw_scratch_mem.h>
#include <platform/chip_registers.h>

/* ee12fa71-4579-45d7-bde2-b32c6893a122 */
DECLARE_SOF_UUID("acp-bt-dma", acp_bt_dma_uuid, 0xab01db67, 0x84b0, 0x4d2d,
		0x93, 0xd3, 0x0e, 0x61, 0x96, 0x80, 0x57, 0x6e);

DECLARE_TR_CTX(acp_bt_dma_tr, SOF_UUID(acp_bt_dma_uuid), LOG_LEVEL_INFO);

/* DMA number of buffer periods */
#define ACP_BT_FIFO_BUFFER_SIZE	768

/* ACP DMA transfer size */
#define ACP_BT_DMA_TRANS_SIZE	64

static uint64_t prev_tx_pos;
static uint64_t prev_rx_pos;

/* Allocate requested DMA channel if it is free */
static struct dma_chan_data *acp_dai_bt_dma_channel_get(struct dma *dma,
						   unsigned int req_chan)
{
	uint32_t flags;
	struct dma_chan_data *channel;

	spin_lock_irq(&dma->lock, flags);
	if (req_chan >= dma->plat_data.channels) {
		spin_unlock_irq(&dma->lock, flags);
		tr_err(&acp_bt_dma_tr, "DMA: Channel %d not in range", req_chan);
		return NULL;
	}
	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		spin_unlock_irq(&dma->lock, flags);
		tr_err(&acp_bt_dma_tr, "DMA: channel already in use %d", req_chan);
		return NULL;
	}
	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	spin_unlock_irq(&dma->lock, flags);

	return channel;
}

/* channel must not be running when this is called */
static void acp_dai_bt_dma_channel_put(struct dma_chan_data *channel)
{
	uint32_t flags;

	notifier_unregister_all(NULL, channel);
	spin_lock_irq(&channel->dma->lock, flags);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	spin_unlock_irq(&channel->dma->lock, flags);
}

static int acp_dai_bt_dma_start(struct dma_chan_data *channel)
{
	acp_bttdm_ier_t         bt_ier;
	acp_bttdm_iter_t        bt_tdm_iter;
	acp_bttdm_irer_t        bt_tdm_irer;

	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		channel->status = COMP_STATE_ACTIVE;
		prev_tx_pos = 0;
		bt_ier = (acp_bttdm_ier_t)io_reg_read((PU_REGISTER_BASE + ACP_BTTDM_IER));
		bt_ier.bits.bttdm_ien = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_IER), bt_ier.u32all);
		bt_tdm_iter.u32all = 0;
		bt_tdm_iter.bits.bttdm_txen = 1;
		bt_tdm_iter.bits.bttdm_tx_protocol_mode  = 0;
		bt_tdm_iter.bits.bttdm_tx_data_path_mode = 1;
		bt_tdm_iter.bits.bttdm_tx_samp_len = 2;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_ITER), bt_tdm_iter.u32all);
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		channel->status = COMP_STATE_ACTIVE;
		bt_ier = (acp_bttdm_ier_t)io_reg_read((PU_REGISTER_BASE + ACP_BTTDM_IER));
		bt_ier.bits.bttdm_ien = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_IER), bt_ier.u32all);
		bt_tdm_irer.u32all = 0;
		bt_tdm_irer.bits.bttdm_rx_en = 1;
		bt_tdm_irer.bits.bttdm_rx_protocol_mode  = 0;
		bt_tdm_irer.bits.bttdm_rx_data_path_mode = 1;
		bt_tdm_irer.bits.bttdm_rx_samplen = 2;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_IRER), bt_tdm_irer.u32all);
	} else {
		tr_err(&acp_bt_dma_tr, " ACP:Start direction not defined %d", channel->direction);
		return -EINVAL;
	}
	return 0;
}


static int acp_dai_bt_dma_release(struct dma_chan_data *channel)
{
	/* nothing to do on renoir */
	return 0;
}

static int acp_dai_bt_dma_pause(struct dma_chan_data *channel)
{
	/* nothing to do on renoir */
	return 0;
}

static int acp_dai_bt_dma_stop(struct dma_chan_data *channel)
{
	acp_bttdm_iter_t        bt_tdm_iter;
	acp_bttdm_irer_t        bt_tdm_irer;

	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		switch (channel->status) {
		case COMP_STATE_READY:
		case COMP_STATE_PREPARE:
			return 0;
		case COMP_STATE_PAUSED:
		case COMP_STATE_ACTIVE:
			break;
		default:
			return -EINVAL;
		}
		channel->status = COMP_STATE_READY;
		bt_tdm_iter = (acp_bttdm_iter_t)io_reg_read(PU_REGISTER_BASE + ACP_BTTDM_ITER);
		bt_tdm_iter.bits.bttdm_txen = 0;
		io_reg_write(PU_REGISTER_BASE + ACP_BTTDM_ITER, bt_tdm_iter.u32all);
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		switch (channel->status) {
		case COMP_STATE_READY:
		case COMP_STATE_PREPARE:
			return 0;
		case COMP_STATE_PAUSED:
		case COMP_STATE_ACTIVE:
			break;
		default:
			return -EINVAL;
		}
		channel->status = COMP_STATE_READY;
		bt_tdm_irer = (acp_bttdm_irer_t)io_reg_read(PU_REGISTER_BASE + ACP_BTTDM_IRER);
		bt_tdm_irer.bits.bttdm_rx_en = 0;
		io_reg_write(PU_REGISTER_BASE + ACP_BTTDM_IRER, bt_tdm_irer.u32all);
	} else {
		tr_err(&acp_bt_dma_tr, "ACP:direction not defined %d", channel->direction);
		return -EINVAL;
	}
	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int acp_dai_bt_dma_status(struct dma_chan_data *channel,
		struct dma_chan_status *status,
		uint8_t direction)
{
	/* nothing to do on renoir */
	return 0;
}

/* set the DMA channel configuration
 * source/target address
 * DMA transfer sizes
 */
static int acp_dai_bt_dma_set_config(struct dma_chan_data *channel,
				struct dma_sg_config *config)
{
	uint32_t tx_ringbuff_addr;
	uint32_t rx_ringbuff_addr;
	uint32_t fifo_addr;
	acp_bt_tx_ringbufaddr_t         tx_rbuff;
	acp_bt_tx_fifoaddr_t            tx_fifo;
	acp_bt_tx_fifosize_t            tx_fifo_size;
	acp_bt_tx_dmasize_t             tx_dma_size;
	acp_bt_tx_ringbufsize_t         tx_rbuff_size;
	acp_bt_tx_ringbufaddr_t         rx_rbuff;
	acp_bt_tx_fifoaddr_t            rx_fifo_addr;
	acp_bt_tx_fifosize_t            rx_fifo_size;
	acp_bt_tx_dmasize_t             rx_dma_size;
	acp_bt_tx_ringbufsize_t         rx_rbuff_size;
	acp_bt_tx_intr_watermark_size_t tx_wtrmrk;
	acp_bt_rx_intr_watermark_size_t rx_wtrmrk;

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *) (PU_REGISTER_BASE + SCRATCH_REG_OFFSET);
	channel->is_scheduling_source = true;
	channel->direction = config->direction;
	switch (config->direction) {
	case DMA_DIR_MEM_TO_DEV:
		tx_ringbuff_addr = config->elem_array.elems[0].src & ACP_DRAM_ADDRESS_MASK;
		/* BT Transmit FIFO Address and FIFO Size */
		fifo_addr = (uint32_t)(&pscratch_mem_cfg->acp_transmit_fifo_buffer);

		tx_fifo.u32all =  fifo_addr;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_FIFOADDR), tx_fifo.u32all);
		tx_fifo_size.u32all = ACP_BT_FIFO_BUFFER_SIZE;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_FIFOSIZE), tx_fifo_size.u32all);
		/* Transmit RINGBUFFER Address and size */
		tx_rbuff.u32all = tx_ringbuff_addr;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_RINGBUFADDR), tx_rbuff.u32all);
		tx_rbuff_size.u32all = ACP_BT_FIFO_BUFFER_SIZE;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_RINGBUFSIZE),
				tx_rbuff_size.u32all);
		/* Transmit DMA transfer size in bytes */
		tx_dma_size.u32all = ACP_DMA_TRANS_SIZE;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_DMA_SIZE), tx_dma_size.u32all);
		/* Watermark size for BT transfer fifo */
		tx_wtrmrk.bits.bt_tx_intr_watermark_size = ACP_BT_FIFO_BUFFER_SIZE/2;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_INTR_WATERMARK_SIZE),
				tx_wtrmrk.u32all);
		break;
	case DMA_DIR_DEV_TO_MEM:
		rx_ringbuff_addr = config->elem_array.elems[0].dest & ACP_DRAM_ADDRESS_MASK;
		fifo_addr = (uint32_t)(&pscratch_mem_cfg->acp_receive_fifo_buffer);
		/* BT Receive FIFO Address and FIFO Size*/
		rx_fifo_addr.u32all =  fifo_addr;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_FIFOADDR),
				rx_fifo_addr.u32all);
		rx_fifo_size.u32all = ACP_BT_FIFO_BUFFER_SIZE;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_FIFOSIZE),
				rx_fifo_size.u32all);
		/* Receive RINGBUFFER Address and size */
		rx_rbuff.u32all = rx_ringbuff_addr;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_RINGBUFADDR),
				rx_rbuff.u32all);
		rx_rbuff_size.u32all = ACP_BT_FIFO_BUFFER_SIZE;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_RINGBUFSIZE),
				rx_rbuff_size.u32all);
		/* Receive DMA transfer size in bytes */
		rx_dma_size.u32all = ACP_BT_DMA_TRANS_SIZE;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_DMA_SIZE),
				rx_dma_size.u32all);
		/* Watermark size for BT receive fifo */
		rx_wtrmrk.bits.bt_rx_intr_watermark_size = ACP_BT_FIFO_BUFFER_SIZE/2;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_INTR_WATERMARK_SIZE),
				rx_wtrmrk.u32all);
		break;
	default:
		tr_err(&acp_bt_dma_tr, "bt_dma_set_config() unsupported config direction");
		return -EINVAL;
	}
	if (!config->cyclic) {
		tr_err(&acp_bt_dma_tr, "BT_DMA: cyclic configurations only supported");
		return -EINVAL;
	}
	if (config->scatter) {
		tr_err(&acp_bt_dma_tr, "BT_DMA: scatter is not supported for now!");
		return -EINVAL;
	}
	return 0;
}

static int acp_dai_bt_dma_copy(struct dma_chan_data *channel, int bytes,
			  uint32_t flags)
{
	struct dma_cb_data next = {
		.channel = channel,
		.elem.size = bytes,
	};
	notifier_event(channel, NOTIFIER_ID_DMA_COPY,
			NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));
	return 0;
}

static int acp_dai_bt_dma_probe(struct dma *dma)
{
	int channel;

	if (dma->chan) {
		tr_err(&acp_bt_dma_tr, "ACP I2S DMA: Repeated probe");
		return -EEXIST;
	}
	dma->chan = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0,
			    SOF_MEM_CAPS_RAM, dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&acp_bt_dma_tr, "I2S DMA: Probe failure, unable to allocate channel descriptors");
		return -ENOMEM;
	}
	for (channel = 0; channel < dma->plat_data.channels; channel++) {
		dma->chan[channel].dma = dma;
		dma->chan[channel].index = channel;
		dma->chan[channel].status = COMP_STATE_INIT;
	}
	atomic_init(&dma->num_channels_busy, 0);
	return 0;
}

static int acp_dai_bt_dma_remove(struct dma *dma)
{
	if (!dma->chan) {
		tr_err(&acp_bt_dma_tr, "DMA: remove called without probe, it's a no-op");
		return 0;
	}
	rfree(dma->chan);
	dma->chan = NULL;
	return 0;
}

static int acp_dai_bt_dma_get_data_size(struct dma_chan_data *channel,
					uint32_t *avail, uint32_t *free)
{
	uint64_t tx_low, curr_tx_pos, tx_high;
	uint64_t rx_low, curr_rx_pos, rx_high;

	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		tx_low = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_TX_LINEARPOSITIONCNTR_LOW);
		tx_high = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_TX_LINEARPOSITIONCNTR_HIGH);
		curr_tx_pos = (uint64_t)((tx_high << 32) | tx_low);
		*free = (curr_tx_pos - prev_tx_pos) > ACP_BT_FIFO_BUFFER_SIZE ?
			(curr_tx_pos - prev_tx_pos) % ACP_BT_FIFO_BUFFER_SIZE :
			curr_tx_pos - prev_tx_pos;
		*avail = ACP_BT_FIFO_BUFFER_SIZE - *free;
		prev_tx_pos = curr_tx_pos;
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		rx_low = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_RX_LINEARPOSITIONCNTR_LOW);
		rx_high = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_RX_LINEARPOSITIONCNTR_HIGH);
		curr_rx_pos = (uint64_t)((rx_high << 32) | rx_low);
		*free = (curr_rx_pos - prev_rx_pos) > ACP_BT_FIFO_BUFFER_SIZE ?
			(curr_rx_pos - prev_rx_pos) % ACP_BT_FIFO_BUFFER_SIZE :
			(curr_rx_pos - prev_rx_pos);
		*avail = ACP_BT_FIFO_BUFFER_SIZE - *free;
		prev_rx_pos = curr_rx_pos;
	} else {
		tr_err(&acp_bt_dma_tr, "ERROR: Channel direction Not defined %d",
		       channel->direction);
		return -EINVAL;
	}
	return 0;
}

static int acp_dai_bt_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = ACP_DMA_BUFFER_ALIGN;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = PLATFORM_DCACHE_ALIGN;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = ACP_DAI_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -ENOENT; /* Attribute not found */
	}
	return 0;
}

static int acp_dai_bt_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	uint32_t status;
	acp_dsp0_intr_stat_t acp_intr_stat;
	acp_dsp0_intr_cntl_t acp_intr_cntl;

	if (channel->status == COMP_STATE_INIT)
		return 0;
	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		acp_intr_stat =  (acp_dsp0_intr_stat_t)
			(dma_reg_read(channel->dma, ACP_DSP0_INTR_STAT));
		status = acp_intr_stat.bits.audio_buffer_int_stat;
		return (status & (1<<channel->index));
	case DMA_IRQ_CLEAR:
		acp_intr_stat.u32all = 0;
		acp_intr_stat.bits.audio_buffer_int_stat = (1 << channel->index);
		status  = acp_intr_stat.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_STAT, status);
		return 0;
	case DMA_IRQ_MASK:
		acp_intr_cntl =  (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.audio_buffer_int_mask &= (~(1 << channel->index));
		status  = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	case DMA_IRQ_UNMASK:
		acp_intr_cntl =  (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.audio_buffer_int_mask  |=  (1 << channel->index);
		status = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	default:
		return -EINVAL;
	}
}

const struct dma_ops acp_dai_bt_dma_ops = {
	.channel_get		= acp_dai_bt_dma_channel_get,
	.channel_put		= acp_dai_bt_dma_channel_put,
	.start			= acp_dai_bt_dma_start,
	.stop			= acp_dai_bt_dma_stop,
	.pause			= acp_dai_bt_dma_pause,
	.release		= acp_dai_bt_dma_release,
	.copy			= acp_dai_bt_dma_copy,
	.status			= acp_dai_bt_dma_status,
	.set_config		= acp_dai_bt_dma_set_config,
	.interrupt		= acp_dai_bt_dma_interrupt,
	.probe			= acp_dai_bt_dma_probe,
	.remove			= acp_dai_bt_dma_remove,
	.get_data_size		= acp_dai_bt_dma_get_data_size,
	.get_attribute		= acp_dai_bt_dma_get_attribute,
};
