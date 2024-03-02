/**
 * @file SDCard.cpp
 * @author Mattheas Jamieson (mattheas@ualberta.ca)
 * @brief Implementation of SD card driver
 * @version 0.1
 * @date 2024-03-01
 */

#include "../inc/SDCard.h"

using namespace sd_driver;

SDCard::SDCard(const bool &configure_spi1)
{
    if (configure_spi1 == true)
    {
        SPI_set_config_optimal(_49_152_MHz, SPI1);
    }
}

SDCard::~SDCard()
{
}

initialization_result_t SDCard::get_initialization_result() const
{
    return initialization_result;
}

BootSectorInformation SDCard::get_boot_sector_information() const
{
    return boot_sector_information;
}

initialization_result_t SDCard::initialize_sd_card()
{
    return initialization_result_t::INIT_RESULT_NA;
}

bool SDCard::read_boot_sector_information()
{
    return false;
}
