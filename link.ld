/* Script for -z combreloc */
/* Copyright (C) 2014-2024 Free Software Foundation, Inc.
   Copying and distribution of this script, with or without modification,
   are permitted in any medium without royalty provided the copyright
   notice and this notice are preserved.  */
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm",
	      "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SEARCH_DIR("=/usr/arm-none-eabi/lib"); SEARCH_DIR("=/usr/local/lib"); SEARCH_DIR("=/lib"); SEARCH_DIR("=/usr/lib");

MEMORY
{
    /* Keep 2kB for application */
    sram (rwx) : ORIGIN = 0x20000000, LENGTH = 0x20005800

    /* Assign the first 64kB to ROM, leave out 64kB for application */
    flash_rom (rx) : ORIGIN = 0x08000000, LENGTH = 0x10000
}

SECTIONS
{
  . = ORIGIN(flash_rom);
  .vector :
  {
      KEEP(*.vector)
  } > flash_rom

  . = ORIGIN(sram);
  __bss_start = .;
  __bss_start__ = .;
  .bss (NOLOAD) :
  {
   *(.dynbss)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)
   . = ALIGN(4);
  } > sram
  _bss_end__ = .; __bss_end__ = .;
  /* This section contains data that is not initialized during load,
     or during the application's initialization sequence.  */
  .noinit   (NOLOAD) : ALIGN(32 / 8)
  {
    PROVIDE (__noinit_start = .);
    *(.noinit .noinit.* .gnu.linkonce.n.*)
    . = ALIGN(32 / 8);
    PROVIDE (__noinit_end = .);
  } > sram
  . = ALIGN(32 / 8);
  __end__ = .;
  _end = .; PROVIDE (end = .);

  /* Exception handling  */
  /*.eh_frame       : ONLY_IF_RW { KEEP (*(.eh_frame)) *(.eh_frame.*) } > sram*/

  . = ALIGN(4);
  __data_start__ = .;
  .data :
  {
      KEEP(*(.data .data.*));
      . = ALIGN(4);
  } > sram AT > flash_rom
  __data_end__ = .;
  __data_loadaddr_start__ = LOADADDR(.data);
  _edata = .; PROVIDE (edata = .);

  .init           :
  {
    KEEP (*(SORT_NONE(.init)))
  } > flash_rom
  .text           :
  {
    *(.text.unlikely .text.*_unlikely .text.unlikely.*)
    *(.text.exit .text.exit.*)
    *(.text.startup .text.startup.*)
    *(.text.hot .text.hot.*)
    *(SORT(.text.sorted.*))
    *(.text .stub .text.* .gnu.linkonce.t.*)
    /* .gnu.warning sections are handled specially by elf.em.  */
    *(.gnu.warning)
    *(.glue_7t) *(.glue_7) *(.vfp11_veneer) *(.v4_bx)
  } > flash_rom
  .fini           :
  {
    KEEP (*(SORT_NONE(.fini)))
  } > flash_rom
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  .rodata         : { *(.rodata .rodata.* .gnu.linkonce.r.*) } > flash_rom /**/
  .rodata1        : { *(.rodata1) } > flash_rom /**/
  /*.eh_frame_hdr   : { *(.eh_frame_hdr) *(.eh_frame_entry .eh_frame_entry.*) }*/
  .eh_frame       : ONLY_IF_RO { KEEP (*(.eh_frame)) *(.eh_frame.*) } > flash_rom /**/
  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  . = ALIGN(CONSTANT (MAXPAGESIZE)) + (. & (CONSTANT (MAXPAGESIZE) - 1));
  /* This section contains data that is initialized during load,
     but not during the application's initialization sequence.  */
  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) } > flash_rom
  .stabstr       0 : { *(.stabstr) } > flash_rom
  .stab.excl     0 : { *(.stab.excl) } > flash_rom
  .stab.exclstr  0 : { *(.stab.exclstr) } > flash_rom
  .stab.index    0 : { *(.stab.index) } > flash_rom
  .stab.indexstr 0 : { *(.stab.indexstr) } > flash_rom
  .comment 0 (INFO) : { *(.comment); LINKER_VERSION; } > flash_rom
  .gnu.build.attributes : { *(.gnu.build.attributes .gnu.build.attributes.*) } > flash_rom
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1.  */
  .debug          0 : { *(.debug) } > flash_rom
  .line           0 : { *(.line) } > flash_rom
  /* GNU DWARF 1 extensions.  */
  .debug_srcinfo  0 : { *(.debug_srcinfo) } > flash_rom
  .debug_sfnames  0 : { *(.debug_sfnames) } > flash_rom
  /* DWARF 1.1 and DWARF 2.  */
  .debug_aranges  0 : { *(.debug_aranges) } > flash_rom
  .debug_pubnames 0 : { *(.debug_pubnames) } > flash_rom
  /* DWARF 2.  */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) } > flash_rom
  .debug_abbrev   0 : { *(.debug_abbrev) } > flash_rom
  .debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) } > flash_rom
  .debug_frame    0 : { *(.debug_frame) } > flash_rom
  .debug_str      0 : { *(.debug_str) } > flash_rom
  .debug_loc      0 : { *(.debug_loc) } > flash_rom
  .debug_macinfo  0 : { *(.debug_macinfo) } > flash_rom
  /* SGI/MIPS DWARF 2 extensions.  */
  .debug_weaknames 0 : { *(.debug_weaknames) } > flash_rom
  .debug_funcnames 0 : { *(.debug_funcnames) } > flash_rom
  .debug_typenames 0 : { *(.debug_typenames) } > flash_rom
  .debug_varnames  0 : { *(.debug_varnames) } > flash_rom
  /* DWARF 3.  */
  .debug_pubtypes 0 : { *(.debug_pubtypes) } > flash_rom
  .debug_ranges   0 : { *(.debug_ranges) } > flash_rom
  /* DWARF 5.  */
  .debug_addr     0 : { *(.debug_addr) } > flash_rom
  .debug_line_str 0 : { *(.debug_line_str) } > flash_rom
  .debug_loclists 0 : { *(.debug_loclists) } > flash_rom
  .debug_macro    0 : { *(.debug_macro) } > flash_rom
  .debug_names    0 : { *(.debug_names) } > flash_rom
  .debug_rnglists 0 : { *(.debug_rnglists) } > flash_rom
  .debug_str_offsets 0 : { *(.debug_str_offsets) } > flash_rom
  .debug_sup      0 : { *(.debug_sup) } > flash_rom
  .ARM.attributes 0 : { KEEP (*(.ARM.attributes)) KEEP (*(.gnu.attributes)) } > flash_rom
  .note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) } > flash_rom
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }
}
