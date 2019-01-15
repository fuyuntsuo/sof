/* SRC conversions */
#include <sof/audio/coefficients/src/src_tiny_int16_1_3_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_1_6_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_2_3_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_3_1_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_3_2_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_6_1_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_7_8_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_8_7_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_20_21_1667_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_21_20_1667_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_24_25_1814_5000.h>
#include <sof/audio/coefficients/src/src_tiny_int16_25_24_1814_5000.h>

/* SRC table */
int16_t fir_one = 16384;
struct src_stage src_int16_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
struct src_stage src_int16_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
int src_in_fs[6] = { 8000, 16000, 32000, 44100, 48000, 50000};
int src_out_fs[6] = { 8000, 16000, 32000, 44100, 48000, 50000};
struct src_stage *src_table1[6][6] = {
	{ &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_6_1814_5000, &src_int16_0_0_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_3_1814_5000, &src_int16_0_0_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_2_3_1814_5000, &src_int16_0_0_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_21_20_1667_5000, &src_int16_0_0_0_0
	},
	{ &src_int16_6_1_1814_5000, &src_int16_3_1_1814_5000,
	 &src_int16_3_2_1814_5000, &src_int16_8_7_1814_5000,
	 &src_int16_1_1_0_0, &src_int16_24_25_1814_5000
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_25_24_1814_5000, &src_int16_1_1_0_0
	}
};
struct src_stage *src_table2[6][6] = {
	{ &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_0_0_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_0_0_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_0_0_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_1_1_0_0,
	 &src_int16_7_8_1814_5000, &src_int16_0_0_0_0
	},
	{ &src_int16_1_1_0_0, &src_int16_1_1_0_0,
	 &src_int16_1_1_0_0, &src_int16_20_21_1667_5000,
	 &src_int16_1_1_0_0, &src_int16_1_1_0_0
	},
	{ &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_0_0_0_0, &src_int16_0_0_0_0,
	 &src_int16_1_1_0_0, &src_int16_1_1_0_0
	}
};
