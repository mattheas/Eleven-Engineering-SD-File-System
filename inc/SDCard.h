/**
 * @file SDCard.h
 * @author Mattheas Jamieson (mattheas@ualberta.ca)
 * @brief Declaration of SD card driver
 * @version 0.1
 * @date 2024-03-01
 */

#ifndef _SDCARD_H_
#define _SDCARD_H_

#include <SPI.h>
#include <GPIO.h>

namespace sd_driver
{

/**
 * @brief Wrapper class for an SD card. Provides functionality for interacting
 * with an SD card
 */

class SDCard
{
  public:
    /**
     * @brief Constructs a new SDCard object
     *
     * @param configure_spi1 configures SPI1 for you if true
     */
    SDCard(const bool &configure_spi1);

    /**
     * @brief Destroys SDCard object
     */
    ~SDCard();

    /**
     * @brief Enumerates the different results of the initialize_sd_card()
     * method. Intended to be informative as to the reason an SD cards
     * initialization may fail
     */
    enum class initialization_result_t
    {
        INIT_SUCCESS = 0, /**< initalization status was succesful, SD card is
                             ready to receive commands now */
        INIT_FAILED_ON_CMD0,      /**< initialization failed on CMD0 */
        INIT_FAILED_ON_CMD8, /**< initialization failed on CMD8 */
        INIT_RESULT_NA    /**< initialization status is not available (NA)*/
    };

    /**
     * @brief Enumerates the different SD card versions a card can have
     */
    enum class sd_card_version_t
    {
        VER_1 = 0, /**< SD version specification 1.X */
        VER_2,     /**< SD version specification 2.00 or later */
        VER_NA     /**< SD version not available */
    };

    /**
     * @brief Enumerates the different SD card standards (a.k.a capacity)
     */
    enum class sd_card_standard_t
    {
        SDSC = 0, /**< Capacity of <2GB */
        SDHC,     /**< Capacity between 2GB and 32GB */
        SDXC,     /**< Capacity between 32GB and 2TB */
        SDUC,     /**< Capacity between 2TB and 128 TB */
        SDNA      /**< SD standard not available */
    };

    /**
     * @brief Struct representing the boot sector information of the SD card.
     */
    struct BootSectorInformation
    {
    };

    struct SDCardInformation
    {
        sd_card_version_t sd_card_version = sd_card_version_t::VER_NA;
        sd_card_standard_t sd_card_standard = sd_card_standard_t::SDNA;
    };

    /**
     * @brief Get the initialization result status
     *
     * @return initialization_result_t is a success, not available or failed
     */
    initialization_result_t get_initialization_result() const;

    /**
     * @brief Get the boot sector information struct. Is only updated after the
     * SD card has been succesfully initialized, else all the info will be
     * 0xFFFF
     *
     * @return BootSectorInformation struct
     */
    BootSectorInformation get_boot_sector_information() const;

    SDCardInformation get_sd_card_information() const;

    /**
     * @brief Attmpets initialization of communication with SD card to put it in
     * a state such that its ready to receive commands
     *
     * @return initialization_result_t result of
     */
    initialization_result_t initialize_sd_card();

  private:
    enum class sd_card_command_response_t
    {
        /**
         * @brief A valid response from the SD card that it has received the issued 
         * command, it verifies/ acknowledges it, and it is NOT in the idle state 
         * (which means its finished the initialization process succesfully)
         */
        SD_CARD_NOT_IN_IDLE_MODE_RESPONSE = 0x0,

        /**
         * @brief A valid response from the SD card that it has received the issued 
         * command, it verifies/ acknowledges it, and it is currently in idle state 
         * (which means its running the initialization process)
         */
        SD_CARD_IN_IDLE_MODE_RESPONSE = 0x1,

        SD_CARD_ILLEGAL_COMMAND = 0x5,

        SD_CARD_ILLEGAL_COMMAND_AND_CRC_ERROR = 0xD,

        SD_CARD_CHECK_PATTERN_ERROR = 0xFD,
        SD_CARD_UNSUPPORTED_VOLTAGE = 0xFE,
        SD_CARD_NO_RESPONSE = 0xFF

    };

    /**
     * @brief Chip Select (C3) inactive high for pin PD3, this disables 
     * communication over SPI1 for the SD card. This magic number comes from 
     * 0x1 << 3, where 3 is the pin number in PD3.
     */
    const uint16_t CS_INACTIVE_HIGH = 0x8;

    /**
     * @brief Chip Select (C3) active low for pin PD3, this enables 
     * communication over SPI1 for the SD card.
     */
    const uint16_t CS_ACTIVE_LOW = 0x0;

    /**
     * @brief After issuing a command an SD card can take 0-8 bytes to respond, 
     * or 0-8 ticks of the SPI clock. This is that limit +2. If a valid response 
     * for a command is not received within this limit it is assumed the SD card 
     * will not return a valid response
     */
    const uint16_t NUM_INVALID_RESPONSE_LIMIT_SPI_READ = 10U;

    /**
     * @brief Reads the boot sector information from the SD card,
     *
     * @return success status of reading the boot sector
     */
    bool read_boot_sector_information();

    /**
     * @brief Sends CMD0 to the SD card, after the command is sent it awaits a valid 
     * response for a resposne limit amount of reads. CMD0 or GO_IDLE_STATE resets the 
     * SD card and attempts to put the card in SPI mode.
     * 
     * @param num_invalid_response_limit num of SPI_reads()'s to wait for a valid 
     * response
     */
    sd_card_command_response_t send_cmd0(const uint16_t &num_invalid_response_limit) const;

    sd_card_command_response_t send_cmd8(const uint16_t &num_invalid_response_limit) const;

    /**
     * @brief Stores the result of the initialize_sd_card() method, initial
     * value before method is called is INIT_RESULT_NA indicating the result is
     * unknown.
     */
    initialization_result_t initialization_result =
        initialization_result_t::INIT_RESULT_NA;

    /**
     * @brief Stores boot sector information. Is only updated after the SD card
     * has been succesfully initialized, else all the info will be 0xFFFF
     */
    BootSectorInformation boot_sector_information;

    SDCardInformation sd_card_information;
};
} // namespace sd_driver

#endif // _SDCARD_H_
