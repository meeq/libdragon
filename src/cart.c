/**
 * @file cart.c
 * @brief Cartridge
 * @ingroup cart
 */

#include "libdragon.h"

/**
 * @defgroup cart Cartridge interface
 * @ingroup libdragon
 * @brief Routines for interacting with the cartridge and hardware attached to it.
 *
 * The cartridge contains the ROM (up to 64 megabytes), as well as optional writable
 * memory in the form of SRAM or FlashRAM (up to 128 kilobytes). There may also be
 * an EEPROM or Real-Time Clock, which are handled by the Joybus interface.
 * 
 * In general, the best way to access ROM or RAM is through DMA transfers. The functions
 * here are mostly convenience helpers on top of the Peripheral Interface, which manages
 * DMA transfers.
 * 
 * SRAM is the simplest save type, allowing direct access using DMA reads/writes. If your
 * storage needs are less than 256 kilobits (32 kilobytes), you should probably use SRAM.
 * If you need more space, you can still consider SRAM, but there are trade-offs. The only
 * practical reason why it might not make sense to use SRAM is if you want compatibility
 * with emulators that do not support SRAM larger than 256 kilobits, or wish to make
 * a reproduction cartridge (which typically only support up to 256 kilobit SRAM).
 * 
 * For ROMs that need to store up to 1 megabit (128 kilobytes), the most widely-compatible
 * save type is FlashRAM. Unfortunately, it is significantly more complicated to write
 * data to FlashRAM. At this time, libdragon does not offer convenience functions to
 * abstract the complexities of the various FlashRAM chips that could be on the cartridge.
 * 
 * If your ROM does not need to store more than 16 kilobits (2 kilobytes), you could use
 * EEPROM. In the age of emulators and flash carts, there is no real advantage to EEPROM
 * over SRAM. EEPROM is lower-capacity, slower to write, must be accessed in 8-byte blocks,
 * and cannot co-exist with the Real-Time Clock (on real hardware). The biggest reason why
 * you might consider using EEPROM is if you wanted to make a reproduction cartridge, which
 * may support EEPROM saving without needing any "donor chips" from another N64 cartridge.
 *
 * @{
 */

#define CART_DOM1_ADDR2_BASE (0x10000000)
#define CART_DOM1_ADDR2_MASK (0x03FFFFFF)
#define CART_DOM1_ADDR2_END  (CART_DOM1_ADDR2_BASE | CART_DOM1_ADDR2_MASK)
#define CART_DOM2_ADDR2_BASE (0x08000000)
#define CART_DOM2_ADDR2_MASK (0x0001FFFF)
#define CART_DOM2_ADDR2_END  (CART_DOM2_ADDR2_BASE | CART_DOM2_ADDR2_MASK)

#define SRAM_TEST_VALUE  0xFEDCBA98
#define SRAM_256K_OFFSET 0x00007FFF
#define SRAM_768K_OFFSET 0x00017FFF
#define SRAM_1M_OFFSET   0x0001FFFF

#define FLASHRAM_IDENTIFIER                0x11118001
#define FLASHRAM_OFFSET_COMMAND            0x00010000
#define FLASHRAM_OFFSET_MASK               0x0000FFFF
#define FLASHRAM_COMMAND_SET_ERASE_OFFSET  0x4B000000
#define FLASHRAM_COMMAND_SET_ERASE_MODE    0x78000000
#define FLASHRAM_COMMAND_SET_WRITE_OFFSET  0xA5000000
#define FLASHRAM_COMMAND_SET_WRITE_MODE    0xB4000000
#define FLASHRAM_COMMAND_EXECUTE           0xD2000000
#define FLASHRAM_COMMAND_SET_IDENTIFY_MODE 0xE1000000
#define FLASHRAM_COMMAND_SET_READ_MODE     0xF0000000

/**
 * @brief Clamp length from a start address so that it does not go past an end address.
 *
 * Used by DMA functions to ensure the read/writes stay in defined ranges.
 *
 * @param[in] len
 *            Length to clamp (in bytes).
 * @param[in] start
 *            Starting address to add length to.
 * @param[in] end
 *            Ending address that start+len cannot exceed.
 *
 * @return Clamped length.
 */
static uint32_t clamp(uint32_t len, uint32_t start, uint32_t end)
{
    int32_t overage = (int32_t)(start + len) - (int32_t)(end + 1);
    return ( overage > 0 ) ? len - overage : len;
}

/**
 * @brief Determine which save type is available on the cartridge.
 *
 * This function checks for EEPROM, then FlashRAM, then SRAM.
 *
 * On real N64 hardware it is not possible for there to be more than one
 * save type available, although some emulators do not enfore this
 * limitation and may allow EEPROM to co-exist with SRAM or FlashRAM.
 * It is not possible for SRAM and FlashRAM to co-exist.
 *
 * @return the detected save type on the cartridge.
 */
cart_save_type_t cart_detect_save_type( void )
{
    eeprom_type_t eeprom = eeprom_present();
    if( eeprom == EEPROM_4K ) return SAVE_TYPE_EEPROM_4KBIT;
    if( eeprom == EEPROM_16K ) return SAVE_TYPE_EEPROM_16KBIT;

    flashram_type_t flashram = cart_detect_flashram();
    if( flashram != FLASHRAM_TYPE_NONE ) return SAVE_TYPE_FLASHRAM_1MBIT;

    if( cart_detect_sram(SRAM_1M_OFFSET) ) return SAVE_TYPE_SRAM_1MBIT;
    if( cart_detect_sram(SRAM_768K_OFFSET) ) return SAVE_TYPE_SRAM_768KBIT;
    if( cart_detect_sram(SRAM_256K_OFFSET) ) return SAVE_TYPE_SRAM_256KBIT;

    return SAVE_TYPE_NONE;
}

/**
 * @brief Determine which FlashRAM chip is installed on the cartridge.
 *
 * The various FlashRAM chips all have slightly different behaviors, so it is helpful
 * to know which one is installed.
 *
 * @return the detected FlashRAM type on the cartridge.
 */
flashram_type_t cart_detect_flashram( void )
{
    /* Tell the FlashRAM to identify itself */
    io_write(CART_DOM2_ADDR2_BASE | FLASHRAM_OFFSET_COMMAND, FLASHRAM_COMMAND_SET_IDENTIFY_MODE);

    /* Read the identifiers */
    uint32_t __attribute__((aligned(16))) silicon_id[2];
    data_cache_hit_writeback_invalidate(silicon_id, 8);
    cart_dom2_dma_read(silicon_id, 0, 8);

    /* Check for the magic "this is FlashRAM" value, followed by which chip it is. */
    if( silicon_id[0] == FLASHRAM_IDENTIFIER ) return silicon_id[1];
    return FLASHRAM_TYPE_NONE;
}

/**
 * @brief Determine whether SRAM on the cartridge is writable at a given offset.
 *
 * Unfortunately, the only way to check this is to actually perform a DMA write/read,
 * which is a destructive operation. This routine attempts to preserve the data before
 * clobbering it during the test, and writing back the original data if successful.
 *
 * @param[in] offset
 *            Offset of SRRAM in bytes to check. SRAM potentially goes up to 1 megabit.
 *
 * @return whether the SRAM was able to read and write successfully at the offset.
 */
bool cart_detect_sram(uint32_t offset)
{
    uint32_t __attribute__((aligned(16))) backup_buf;
    uint32_t __attribute__((aligned(16))) detect_buf;

    /* Offset is potentially the end of the writable space, so go back a few bytes */
    uint32_t len = sizeof(backup_buf);
    uint32_t start = offset - len;

    /* Read the current data before writing over it */
    data_cache_hit_writeback_invalidate(&backup_buf, len);
    cart_dom2_dma_read(&backup_buf, start, len);

    /* Write a test value into SRAM... */
    detect_buf = SRAM_TEST_VALUE;
    data_cache_hit_writeback_invalidate(&detect_buf, len);
    cart_dom2_dma_write(&detect_buf, start, len);

    /* Read the test value back to see if it persisted */
    detect_buf = 0;
    data_cache_hit_writeback_invalidate(&detect_buf, len);
    cart_dom2_dma_read(&detect_buf, start, len);

    if( detect_buf == SRAM_TEST_VALUE )
    {
        /* Restore the data that was overwritten to test SRAM */
        data_cache_hit_writeback_invalidate(&backup_buf, len);
        cart_dom2_dma_read(&backup_buf, start, len);
        return true;
    }
    else
    {
        /* There is no SRAM at this offset */
        return false;
    }
}

/**
 * @brief Read from Cartridge Domain 1 Address 2
 *
 * This function should be used when reading from the cartridge.
 *
 * @param[out] dest
 *             Pointer to a buffer to place read data
 * @param[in]  offset
 *             Offset in bytes from the start of Cartridge Domain 1 Address 2 to read from
 * @param[in]  len
 *             Length in bytes to read into dest
 */
void cart_dma_read(void * dest, uint32_t offset, uint32_t len)
{
    assert(len > 1);
    uint32_t pi_address = ((offset & CART_DOM1_ADDR2_MASK) | CART_DOM1_ADDR2_BASE);
    pi_dma_read(dest, pi_address, clamp(len, pi_address, CART_DOM1_ADDR2_END));
}

/**
 * @brief Write to Cartridge Domain 1 Address 2
 *
 * This function should be used when writing to the cartridge.
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 * @param[in] offset
 *            Offset in bytes from the start of Cartridge Domain 1 Address 2 to write to
 * @param[in] len
 *            Length in bytes to write from src
 */
void cart_dma_write(const void * src, uint32_t offset, uint32_t len)
{
    assert(len > 1);
    uint32_t pi_address = ((offset & CART_DOM1_ADDR2_MASK) | CART_DOM1_ADDR2_BASE);
    pi_dma_write(src, pi_address, clamp(len, pi_address, CART_DOM1_ADDR2_END));
}

/**
 * @brief Read from Cartridge Domain 2 Address 2
 *
 * This function should be used when reading from SRAM or FlashRAM.
 *
 * @param[out] dest
 *             Pointer to a buffer to place read data
 * @param[in]  offset
 *             Offset in bytes from the start of Cartridge Domain 1 Address 2 to read from
 * @param[in]  len
 *             Length in bytes to read into dest
 */
void cart_dom2_dma_read(void * dest, uint32_t offset, uint32_t len)
{
    assert(len > 1);
    uint32_t pi_address = ((offset & CART_DOM2_ADDR2_MASK) | CART_DOM2_ADDR2_BASE);
    pi_dma_read(dest, pi_address, clamp(len, pi_address, CART_DOM2_ADDR2_END));
}

/**
 * @brief Write to Cartridge Domain 2 Address 2
 *
 * This function should be used when writing to the SRAM or FlashRAM.
 *
 * @param[in] src
 *            Pointer to a buffer to read data from
 * @param[in] offset
 *            Offset in bytes from the start of Cartridge Domain 2 Address 2 to write to
 * @param[in] len
 *            Length in bytes to write from src
 */
void cart_dom2_dma_write(const void * src, uint32_t offset, uint32_t len)
{
    assert(len > 1);
    uint32_t pi_address = ((offset & CART_DOM2_ADDR2_MASK) | CART_DOM2_ADDR2_BASE);
    pi_dma_write(src, pi_address, clamp(len, pi_address, CART_DOM2_ADDR2_END));
}


/** @} */ /* cart */
