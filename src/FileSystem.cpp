/**
 * @file FileSystem.cpp
 * @author Mattheas Jamieson (mattheas@ualberta.ca)
 * @brief Implementation of File System class
 * @version 0.1
 * @date 2024-03-03
 */

#include "../inc/FileSystem.h"
#include <XPD.h>

using namespace file_system;

FileSystem::FileSystem(sd_driver::SDCard &_sd_card, const file_system_t &_file_system_type) : sd_card(_sd_card), file_system_type(_file_system_type)
{
    // Initialize SD card if its not already initalized

    // read MBR
    read_fat32_master_boot_record();

    // read VolumeID
}

FileSystem::~FileSystem()
{
}

FileSystem::FAT32MasterBootRecord FileSystem::get_fat_32_master_boot_record() const
{
    return fat_32_master_boot_record;
}

FileSystem::FAT32VolumeID FileSystem::get_fat_32_volume_id() const
{
    return fat_32_volume_id;
}

bool FileSystem::read_fat32_master_boot_record()
{
    uint16_t mbr_512_byte_sector[512];

    sd_driver::SDCard::sd_card_command_response_t cmd17_response;

    cmd17_response = sd_card.send_cmd17(mbr_512_byte_sector, 0x00, 0x00, 0x00, 0x00);

    if(cmd17_response == sd_driver::SDCard::sd_card_command_response_t::SD_CARD_RESPONSE_ACCEPTED)
    {
        // const uint16_t partition_1_first_byte_index = 446U;
        fat_32_master_boot_record.primary_partition_1.boot_flag = mbr_512_byte_sector[446];

        fat_32_master_boot_record.primary_partition_1.chs_begin[0] = mbr_512_byte_sector[447];
        fat_32_master_boot_record.primary_partition_1.chs_begin[1] = mbr_512_byte_sector[448];
        fat_32_master_boot_record.primary_partition_1.chs_begin[2] = mbr_512_byte_sector[449];

        fat_32_master_boot_record.primary_partition_1.type_code = mbr_512_byte_sector[450];

        fat_32_master_boot_record.primary_partition_1.chs_end[0] = mbr_512_byte_sector[451];
        fat_32_master_boot_record.primary_partition_1.chs_end[1] = mbr_512_byte_sector[452];
        fat_32_master_boot_record.primary_partition_1.chs_end[2] = mbr_512_byte_sector[453];

        fat_32_master_boot_record.primary_partition_1.lba_begin[0] = mbr_512_byte_sector[454];
        fat_32_master_boot_record.primary_partition_1.lba_begin[1] = mbr_512_byte_sector[455];
        fat_32_master_boot_record.primary_partition_1.lba_begin[2] = mbr_512_byte_sector[456];
        fat_32_master_boot_record.primary_partition_1.lba_begin[3] = mbr_512_byte_sector[457];

        fat_32_master_boot_record.primary_partition_1.number_of_sectors[0] = mbr_512_byte_sector[458];
        fat_32_master_boot_record.primary_partition_1.number_of_sectors[1] = mbr_512_byte_sector[459];
        fat_32_master_boot_record.primary_partition_1.number_of_sectors[2] = mbr_512_byte_sector[460];
        fat_32_master_boot_record.primary_partition_1.number_of_sectors[3] = mbr_512_byte_sector[461];

        fat_32_master_boot_record.mbr_signature[0] = mbr_512_byte_sector[510];
        fat_32_master_boot_record.mbr_signature[1] = mbr_512_byte_sector[511];

        // TODO read other 3 partitions

        // Check that type code and signature are valid
        if ((fat_32_master_boot_record.primary_partition_1.type_code == 0xB ||
            fat_32_master_boot_record.primary_partition_1.type_code == 0xC) && 
            fat_32_master_boot_record.mbr_signature[0] == 0x55 && 
            fat_32_master_boot_record.mbr_signature[1] == 0xAA)
        {
            return true;    
        }

    }

    return false;
}

bool FileSystem::read_fat_32_volume_id()
{
    return false;
}
