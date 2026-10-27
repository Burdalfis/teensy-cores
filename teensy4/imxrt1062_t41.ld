MEMORY
{
	ITCM (rwx):  ORIGIN = 0x00000000, LENGTH = 512K
	DTCM (rwx):  ORIGIN = 0x20000000, LENGTH = 512K
	RAM (rwx):   ORIGIN = 0x20200000, LENGTH = 512K
	FLASH (rwx): ORIGIN = 0x60000000, LENGTH = 7936K
	ERAM (rwx):  ORIGIN = 0x70000000, LENGTH = 16384K
	SDRAM (rwx): ORIGIN = 0x90000000, LENGTH = 32768K
}

ENTRY(ImageVectorTable)

SECTIONS
{
	.text.headers : ALIGN(4) {
		FILL(0xFF)
		KEEP(*(.flashconfig))
		. = 0x1000;
		KEEP(*(.ivt))
		. = 0x1020;
		KEEP(*(.bootdata))
		. = ALIGN(1024);
	} > FLASH

	.text.code : ALIGN(4) {
		/* MSC-only code/constants do not consume map-mode FlexRAM. */
		*libTinyUSB.a:*(.text* .rodata*)
		*(.text._ZN7usb_msc*)
		*(.text.tud_*)
		*(.rodata._ZN7usb_msc* .rodata._ZZN7usb_msc*)
		*(.rodata.tud_*)
		*(.rodata._ZN12_GLOBAL__N_115storage_command*)
		*(.rodata._ZN12_GLOBAL__N_14emit*)
		*(.rodata._ZN12_GLOBAL__N_111driver_name*)
		/* Cold diagnostic decimal formatting, not arithmetic/SD/RTOS code. */
		*(.text._dtoa_r .text.quorem .text._printf_float .text.__cvt)
		/* Cold navigation uses flash; named .fastrun kernels remain for ITCM. */
		/* Cold indexed-file identity helpers; seek/read/SDIO remain in ITCM. */
		*libSdFat.a:*(.text._ZN7FatFile8getName8EPcj .text._ZN9ExFatFile8getName8EPcj)
		*libSdFat.a:*(.text._ZN7FatFile6getSFNEPcj .text._ZN5FsUtf6cpToMbEmPcS0_)
		__navigation_flash_start = .;
		*/src/navigation/*.cpp.o(.text* .rodata*)
		*libNavigation.a:*(.text* .rodata*)
		__navigation_flash_end = .;
		/* Reviewed cold code executes directly from cached flash. */
		__cold_itcm_flash_start = .;
		/* Map publication wake helper; scheduler/IRQ kernels stay resident. */
		*(.text.xTaskAbortDelay)
		*libSdFat.a:ExFatFile.cpp.o(.text*)
		*libSdFat.a:ExFatFilePrint.cpp.o(.text*)
		*libSdFat.a:ExFatFileWrite.cpp.o(.text*)
		*libSdFat.a:ExFatName.cpp.o(.text*)
		*libSdFat.a:ExFatPartition.cpp.o(.text*)
		*libSdFat.a:*(.text._ZN13SharedSpiCard* .text._ZNK13SharedSpiCard*)
		*libSdFat.a:*(.text._ZN16DedicatedSpiCard* .text._ZNK16DedicatedSpiCard*)
		*libSdFat.a:*(.text._ZN18SdSpiArduinoDriver* .text._ZNK18SdSpiArduinoDriver*)
		*libFrameworkArduino.a:sdram.c.o(.text*)
		*libm_nano.a:libm_a-w_atan2.o(.text*)
		*libm_nano.a:libm_a-w_cosh.o(.text*)
		*libm_nano.a:libm_a-w_fmod.o(.text*)
		*libm_nano.a:libm_a-w_sqrt.o(.text*)
		*libm_nano.a:libm_a-s_cos.o(.text*)
		*libm_nano.a:libm_a-s_sin.o(.text*)
		*libm_nano.a:libm_a-s_tan.o(.text*)
		*libm_nano.a:libm_a-wf_atan2.o(.text*)
		*libm_nano.a:libm_a-wf_fmod.o(.text*)
		*libm_nano.a:libm_a-log.o(.text*)
		*libm_nano.a:libm_a-math_errf.o(.text*)
		*libm_nano.a:libm_a-e_sqrt.o(.text*)
		*libm_nano.a:libm_a-k_cos.o(.text*)
		*libm_nano.a:libm_a-k_sin.o(.text*)
		*libm_nano.a:libm_a-k_tan.o(.text*)
		*libm_nano.a:libm_a-e_atan2.o(.text*)
		*libm_nano.a:libm_a-e_cosh.o(.text*)
		*libm_nano.a:libm_a-e_fmod.o(.text*)
		*libm_nano.a:libm_a-e_rem_pio2.o(.text*)
		*libm_nano.a:libm_a-s_atan.o(.text*)
		*libm_nano.a:libm_a-ef_atan2.o(.text*)
		*libm_nano.a:libm_a-sf_atan.o(.text*)
		*libm_nano.a:libm_a-sf_fabs.o(.text*)
		*libm_nano.a:libm_a-s_expm1.o(.text*)
		*libm_nano.a:libm_a-exp.o(.text*)
		*libm_nano.a:libm_a-math_err.o(.text*)
		*libm_nano.a:libm_a-k_rem_pio2.o(.text*)
		*libm_nano.a:libm_a-s_scalbn.o(.text*)
		*libm_nano.a:libm_a-s_floor.o(.text*)
		/* Cache-friendly task controllers, tile setup, metadata and diagnostics.
		 * Raster/projection kernels, payload I/O, ISRs and unwind stay resident. */
		*(.text._Z8dbg_warnPKcS0_z)
		*(.text._Z9dbg_errorPKcS0_z)
		*(.text._Z9dbg_eventPKcS0_z)
		*(.text._ZN11DmamemAlloc13dmamem_mallocEj.constprop.0)
		*(.text._ZN12_GLOBAL__N_112control_taskEPv)
		*(.text._ZN12_GLOBAL__N_112control_taskEPv.lto_priv.[0-9]*)
		*(.text._ZN7mcu_map10TileLoader12release_blobERNS_8TileBlobE)
		*(.text._ZN7mcu_map12_GLOBAL__N_110valid_binsEPKhRKNS_5mtile19SectionDescriptorV2ES6_mm.lto_priv.0)
		*(.text._ZN7mcu_map12_GLOBAL__N_120begin_replay_segmentEjm)
		*(.text._ZN7mcu_map12_GLOBAL__N_121reset_object_in_placeINS_24SharedRenderableSnapshotEEEvRT_.lto_priv.0)
		*(.text._ZN7mcu_map12_GLOBAL__N_127interpolate_replay_positionEm)
		*(.text._ZN7mcu_map16CameraController6updateERKNS_12VehicleStateEllf)
		*(.text._ZN7mcu_map19VisibilityCollector11reset_stateERNS0_19TileGenerationStateERKNS_8TileSlotE)
		*(.text._ZN7mcu_map19feature_select_taskEPv)
		*(.text._ZN7mcu_map27AdaptiveLookaheadController6updateEff)
		*(.text._ZN7mcu_map27make_top_down_view_coverageERKNS_22ProjectionFrameContextEttt.constprop.0)
		*(.text._ZN7mcu_map8map_taskEPv)
		*(.text._ZN7mcu_map9TileCache12end_snapshotERNS_17TileCacheSnapshotE.constprop.0)
		*(.text._ZN7mcu_map9TileCache14ensure_storageEv.constprop.0)
		*(.text._ZN7mcu_map9TileCacheD2Ev)
		*(.text._ZNK11DmamemAlloc13getFreeMemoryEv.constprop.0)
		*(.text._ZNK11DmamemAlloc13getUsedMemoryEv.constprop.0)
		*(.text._ZNK11DmamemAlloc19getLargestFreeBlockEv.constprop.0)
		*(.text._ZNK7mcu_map9TileCache27renderable_ready_slot_countEv.constprop.0)
		*(.text._ZSt13__adjust_heapIPN7mcu_map12_GLOBAL__N_119ScoredTileCandidateEiS2_N9__gnu_cxx5__ops15_Iter_comp_iterIZNS1_28build_scored_tile_candidatesERKNS0_7TileKeyElRKNS0_15RenderTelemetryERKNS0_19TopDownViewCoverageERSt5arrayIS2_Lj289EEEUlRKS2_SK_E_EEEvT_T0_SO_T1_T2_.isra.0)
		*(.text._ZSt16__insertion_sortIPN7mcu_map12_GLOBAL__N_119ScoredTileCandidateEN9__gnu_cxx5__ops15_Iter_comp_iterIZNS1_28build_scored_tile_candidatesERKNS0_7TileKeyElRKNS0_15RenderTelemetryERKNS0_19TopDownViewCoverageERSt5arrayIS2_Lj289EEEUlRKS2_SK_E_EEEvT_SN_T0_.isra.0)
		*(.text.startup._sub_I_65535_0.0)
		*libFrameworkArduino.a:pwm.c.o(.text.flexpwm_init)
		*libFrameworkArduino.a:pwm.c.o(.text.pwm_init)
		*libSPI.a:SPI.cpp.o(.text._ZN8SPIClass15initDMAChannelsEv)
		*libSPI.a:SPI.cpp.o(.text._ZN8SPIClass5beginEv)
		*libSPI.a:SPI.cpp.o(.text._ZN8SPIClassC2Ejj)
		*libSPI.a:SPI.cpp.o(.text.startup._GLOBAL__sub_I__ZN8SPIClass5beginEv)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile10addClusterEv)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile11openClusterEPS_)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile13addDirClusterEv)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile13cacheDirEntryEh)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile4openEP9FatVolumePKci)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile4peekEv)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile4syncEv)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile5closeEv)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile5mkdirEPS_P8FatLfn_t)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile5mkdirEPS_PKcb)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile5writeEPKvj)
		*libSdFat.a:FatFile.cpp.o(.text._ZN7FatFile8openRootEP9FatVolume)
		*libSdFat.a:FatFileLFN.cpp.o(.text._ZN7FatFile13makeUniqueSfnEP8FatLfn_t)
		*libSdFat.a:FatFileLFN.cpp.o(.text._ZN7FatFile9createLFNEtP8FatLfn_th)
		*libSdFat.a:FatName.cpp.o(.text._ZN7FatFile7getNameEPcj)
		*libSdFat.a:FatPartition.cpp.o(.text._ZN12FatPartition15allocateClusterEmPm)
		*libSdFat.a:FatPartition.cpp.o(.text._ZN12FatPartition4initEP22FsBlockDeviceInterfaceh)
		*libSdFat.a:FatPartition.cpp.o(.text._ZN12FatPartition6fatPutEmm)
		*libSdFat.a:FatPartition.cpp.o(.text._ZN12FatPartition9freeChainEm)
		*libSdFat.a:FatPartition.cpp.o(.text._ZN22FsBlockDeviceInterface19readSectorsCallbackEmPhjPFvmS0_PvES1_)
		*libSdFat.a:FsFile.cpp.o(.text._ZN10FsBaseFile5closeEv)
		*libSdFat.a:FsFile.cpp.o(.text._ZN10FsBaseFileaSERKS_)
		*libSdFat.a:FsVolume.cpp.o(.text._ZN8FsVolume5beginEP22FsBlockDeviceInterfacebh)
		*libSdFat.a:SdioTeensy.cpp.o(.text._ZL8cardCMD6mPh)
		*libSdFat.a:SdioTeensy.cpp.o(.text._ZL8initSDHCv)
		*libSdFat.a:SdioTeensy.cpp.o(.text._ZL8setSdclkm)
		*libSdFat.a:SdioTeensy.cpp.o(.text._ZL9readReg16mPv)
		*libSdFat.a:SdioTeensy.cpp.o(.text._ZN22FsBlockDeviceInterface20writeSectorsCallbackEmjPFPKhmPvES2_)
		*libSdFat.a:SdioTeensy.cpp.o(.text._ZN8SdioCard5beginE10SdioConfig)
		*libU8g2.a:u8g2_d_setup.c.o(.text.u8g2_Setup_ls027b7dh01_400x240_f)
		*libU8g2.a:u8g2_font.c.o(.text.u8g2_read_font_info)
		*libU8g2.a:u8g2_setup.c.o(.text.u8g2_SetupBuffer)
		*libU8g2.a:u8g2_setup.c.o(.text.u8g2_update_dimension_r0)
		*libU8g2.a:u8x8_display.c.o(.text.u8x8_SetupMemory)
		*libU8g2.a:u8x8_display.c.o(.text.u8x8_d_helper_display_init)
		*libU8g2.a:u8x8_setup.c.o(.text.u8x8_Setup)
		*libc_nano.a:libc_a-fflush.o(.text.__sflush_r)
		*libc_nano.a:libc_a-fflush.o(.text._fflush_r)
		*libc_nano.a:libc_a-makebuf.o(.text.__smakebuf_r)
		*libc_nano.a:libc_a-makebuf.o(.text.__swhatbuf_r)
		*libc_nano.a:libc_a-nano-vfprintf.o(.text.__sfputc_r)
		*libc_nano.a:libc_a-nano-vfprintf.o(.text._vfprintf_r)
		*libc_nano.a:libc_a-signal.o(.text._raise_r)
		*libc_nano.a:libc_a-vdprintf.o(.text._vdprintf_r)
		*libc_nano.a:libc_a-vsnprintf.o(.text._vsnprintf_r)
		*libc_nano.a:libc_a-wbuf.o(.text.__swbuf_r)
		*libc_nano.a:libc_a-wsetup.o(.text.__swsetup_r)
		*libm_nano.a:libm_a-s_fabs.o(.text.fabs)
		/* UI controller is cached; display transfer and IRQ workers stay in ITCM. */
		*(.text._Z11screen_taskPv)
		__cold_itcm_flash_end = .;
		*(.flashmem*)
		KEEP(*(.startup))
		. = ALIGN(4);
		KEEP(*(.init))
		__preinit_array_start = .;
		KEEP (*(.preinit_array))
		__preinit_array_end = .;
		__init_array_start = .;
		KEEP (*(.init_array))
		__init_array_end = .;
		*(.text.__libc_init_array)
		. = ALIGN(4);
	} > FLASH

	.text.progmem : ALIGN(4) {
		*(.progmem*)
		. = ALIGN(4);
	} > FLASH

	.data.func : ALIGN(4) {
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.*__func__*)))
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.*__PRETTY_FUNCTION__*)))
		. = ALIGN(4);
	} > FLASH

	.text.itcm : ALIGN(32) {
		. = . + 32; /* MPU to trap NULL pointer deref */
		*(.fastrun*)
		*(.text*)
		. = ALIGN(4);
	} > ITCM  AT> FLASH

	.data : ALIGN(4) {
		*(.endpoint_queue)
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.data*)))
		KEEP(*(.vectorsram))
		. = ALIGN(4);
	} > DTCM  AT> FLASH

	.bss : ALIGN(4) {
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(COMMON)))
		. = ALIGN(32);
		. = . + 32; /* MPU to trap stack overflow */
	} > DTCM

	.bss.dma (NOLOAD) : ALIGN(32) {
		*(.hab_log)
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.dmabuffers)))
		. = ALIGN(32);
	} > RAM

	.bss.padding (NOLOAD) : ALIGN(4) {
		_heap_start = . ;
		. = ABSOLUTE(ORIGIN(RAM) + LENGTH(RAM) - 128 - SIZEOF(.ARM.exidx));
		. = ALIGN(4);
		_heap_end = . ;
	} > RAM

	.ARM.exidx : ALIGN(4) {
		__exidx_start = .;
		*(.gnu.linkonce.armexidx.*)
		*(.ARM.exidx.fastrun*)
		*(.ARM.exidx.text*)
		*(.ARM.exidx)
		*(.ARM.exidx.flashmem*)
		*(.ARM.exidx.startup*)
		. = ALIGN(4);
		__exidx_end = .;
		*(.ARM.extab* .gnu.linkonce.armextab.*)
		. = ALIGN(4);
	} > RAM  AT> FLASH
	
	.bss.extram (NOLOAD) : {
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.externalram)))
		. = ALIGN(32);
	} > ERAM

	.bss.extsdram (NOLOAD) : {
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.externalsdram)))
		. = ALIGN(32);
	} > SDRAM

	.text.csf : ALIGN(4) {
		FILL(0xFF)
		KEEP(*(.csf))
		__text_csf_end = .;
	} > FLASH

	_stext = ADDR(.text.itcm);
	_etext = ADDR(.text.itcm) + SIZEOF(.text.itcm);
	_stextload = LOADADDR(.text.itcm);

	_sdata = ADDR(.data);
	_edata = ADDR(.data) + SIZEOF(.data);
	_sdataload = LOADADDR(.data);

	_sexidx = ADDR(.ARM.exidx);
	_eexidx = ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx);
	_sexidxload = LOADADDR(.ARM.exidx);

	_sbss = ADDR(.bss);
	_ebss = ADDR(.bss) + SIZEOF(.bss);
	
	_extram_start = ADDR(.bss.extram);
	_extram_end = ADDR(.bss.extram) + SIZEOF(.bss.extram);

	_extsdram_start = ADDR(.bss.extsdram);
	_extsdram_end = ADDR(.bss.extsdram) + SIZEOF(.bss.extsdram);

	_itcm_block_count = (SIZEOF(.text.itcm) + 0x7FFF) >> 15;
	_flexram_bank_config = 0xAAAAAAAA | ((1 << (_itcm_block_count * 2)) - 1);
	_estack = ORIGIN(DTCM) + ((16 - _itcm_block_count) << 15);

	_flashimagelen = __text_csf_end - ORIGIN(FLASH);
	_teensy_model_identifier = 0x25;

	.debug_info     0 : { *(.debug_info) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }

}
