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
        INIT_FAILED_ON_CMD58, /**< initialization failed on CMD58 */
        INIT_FAILED_ON_CMD55, /**< initialization failed on CMD55 */
        INIT_FAILED_ON_ACMD41, /**< initialization failed on ACMD41 */
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
        SDSC = 0,       /**< Capacity of <2GB */
        SDHC_OR_SDXC,   /**< Capacity between 2GB and 32GB, or 32GB and 2TB */
        SDNA            /**< SD standard not available */
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

        /**
         * @brief Contents of SD card OCR register. Follows Big Endian format, i.e., 
         * MSB is at index 0, LSB is at index 3.
         */
        uint16_t ocr_register_contents[4] = {0x0, 0x0, 0x0, 0x0};
    };

    /**
     * @brief Get the initialization result status
     *
     * @return initialization_result_t is a success, not available or failed
     */
    initialization_result_t get_initialization_result() const;

    /**
     * @brief Get the boot sector information struct. The data is only valid after
     * the information has been read from the boot sector (this only applies to 
     * formatted SD cards) via the read_boot_sector_information() method. 
     *
     * @return BootSectorInformation struct
     */
    BootSectorInformation get_boot_sector_information() const;

    /**
     * @brief Get the sd card information struct. The data is only valid after
     * the SD card has been succesfully intializied via the initialize_sd_card()
     * method. Before that the data will be 0x0 or NA.
     * 
     * @return SDCardInformation struct
     */
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

        SD_CARD_CHECK_PATTERN_ERROR = 0xFC,
        SD_CARD_UNSUPPORTED_VOLTAGE = 0xFD,
        SD_CARD_RESPONSE_ACCEPTED = 0xFE,
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
     */
    sd_card_command_response_t send_cmd0() const;

    /**
     * @brief Sends CMD8 to the SD card, this is an interface condition that defines the
     * supported voltages (2.7-3.6V) along with a check pattern and asks the card if it 
     * can operate in that supplied voltage range. If the SD card does support the voltage 
     * range it should repeat back the command argument (voltages+pattern) in its response
     * 
     * @return sd_card_command_response_t SD cards response to the command
     */
    sd_card_command_response_t send_cmd8() const;

    /**
     * @brief Sends CMD58 to the SD card, this command reads the contents of the OCR 
     * register, saves it into an array, verifies that all voltages supported to 
     * return that that the response was accepted. If the command is viewed as invalid
     * by the SD card OR the OCR register contents does not support all voltages this 
     * information is returned
     * 
     * @return sd_card_command_response_t SD cards response to the command
     * 
     * @param expected_in_idle when this command is issued if the SD card is expected
     * to be in idle this should be set as the cards R1 response should be 0x1, compared
     * to if the card is not expected to be in idle its response should be 0x0
     */
    sd_card_command_response_t send_cmd58(const bool &expected_in_idle);

    /**
     * @brief Sends CMD55 to the SD card, this command notifies the card that the next
     * command it receives is an application specific command rather than a specific 
     * command. 
     * 
     * @return sd_card_command_response_t SD cards response to the command
     */
    sd_card_command_response_t send_cmd55() const;

    sd_card_command_response_t send_acmd41() const;

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

    /**
     * @brief Information about the SD card that is agnostic to the formatting 
     * of the SD card
     */
    SDCardInformation sd_card_information;
};
} // namespace sd_driver

#endif // _SDCARD_H_
