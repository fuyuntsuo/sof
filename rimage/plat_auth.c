/*
 * Copyright (c) 2017, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 *  Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *          Keyon Jie <yang.jie@linux.intel.com>
 */

#include "rimage.h"
#include "manifest.h"
#include "plat_auth.h"

void ri_adsp_meta_data_create(struct image *image, int meta_start_offset,
	int meta_end_offset)
{
	struct sof_man_adsp_meta_file_ext *meta =
		image->fw_image + meta_start_offset;

	fprintf(stdout, " meta: completing ADSP manifest\n");

	meta->comp_desc[0].limit_offset = MAN_DESC_OFFSET_V1_8 +
		image->image_end - meta_end_offset;

	fprintf(stdout, " meta: limit is 0x%x\n",
		meta->comp_desc[0].limit_offset);
	/* now hash the AdspFwBinaryDesc -> EOF */
}

void ri_plat_ext_data_create(struct image *image)
{
	struct partition_info_ext *part = image->fw_image
		+ MAN_PART_INFO_OFFSET_V1_8;
	struct sof_man_adsp_meta_file_ext *meta =
		image->fw_image + MAN_META_EXT_OFFSET_V1_8;
	struct sof_man_fw_desc *desc = image->fw_image + MAN_DESC_OFFSET_V1_8;

	fprintf(stdout, " auth: completing authentication manifest\n");

	part->length = meta->comp_desc[0].limit_offset - MAN_DESC_OFFSET_V1_8;
	part->length += MAN_PAGE_SIZE - (part->length % MAN_PAGE_SIZE);

	/* do this here atm */
	desc->header.preload_page_count = part->length / MAN_PAGE_SIZE;
}
