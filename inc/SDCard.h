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
        INIT_FAILED,      /**< initialzation failed */
        INIT_RESULT_NA    /**< initialization status is not available (NA)*/
    };

    /**
     * @brief Enumerates the different SD card versions a card can have
     */
    enum class sd_card_version_t
    {
        VER_1 = 0, /**< SD version specification 1.X */
        VER_2      /**< SD version specification 2.00 or later */
    };

    /**
     * @brief Enumerates the different SD card standards (a.k.a capacity)
     */
    enum class sd_card_standard_t
    {
        SDSC = 0, /**< Capacity of <2GB */
        SDHC,     /**< Capacity between 2GB and 32GB */
        SDXC,     /**< Capacity between 32GB and 2TB */
        SDUC      /**< Capacity between 2TB and 128 TB */
    };

    /**
     * @brief Struct representing the boot sector information of the SD card.
     */
    struct BootSectorInformation
    {
    };

    /**
     * @brief Get the initialization result status
     *
     * @return initialization_result_t is a success, not available or failed
     */
    initialization_result_t get_initialization_result();

    /**
     * @brief Get the boot sector information struct. Is only updated after the
     * SD card has been succesfully initialized, else all the info will be
     * 0xFFFF
     *
     * @return BootSectorInformation struct
     */
    BootSectorInformation get_boot_sector_information();

    /**
     * @brief Attmpets initialization of communication with SD card to put it in
     * a state such that its ready to receive commands
     *
     * @return initialization_result_t result of
     */
    initialization_result_t initialize_sd_card();

  private:
    /**
     * @brief Reads the boot sector information from the SD card,
     *
     * @return success status of reading the boot sector
     */
    bool read_boot_sector_information();

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
};
} // namespace sd_driver

#endif // _SDCARD_H_
