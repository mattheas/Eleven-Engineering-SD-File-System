/**
 * @file FileSystem.cpp
 * @author Mattheas Jamieson (mattheas@ualberta.ca)
 * @brief Implementation of File System class
 * @version 0.1
 * @date 2024-03-03
 */

#include "../inc/FileSystem.h"

using namespace file_system;

FileSystem::FileSystem(sd_driver::SDCard &_sd_card, const file_system_t &_file_system_type) : sd_card(_sd_card), file_system_type(_file_system_type)
{
    // Initialize SD card if its not already initalized

    // read MBR

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
    return false;
}

bool FileSystem::read_fat_32_volume_id()
{
    return false;
}
