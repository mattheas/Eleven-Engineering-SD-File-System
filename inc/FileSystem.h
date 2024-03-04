/**
 * @file FileSystem.h
 * @author Mattheas Jamieson (mattheas@ualberta.ca)
 * @brief Declaration of File System class
 * @version 0.1
 * @date 2024-03-03
 */

#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#include "../inc/SDCard.h"

namespace file_system
{

/**
 * @brief File System class, provides high level API for interacting
 * with formatted a SD card
 */
class FileSystem
{
  public:
    enum class file_system_t; // Forward declaration

    FileSystem(sd_driver::SDCard &_sd_card, const file_system_t &_file_system_type);

    ~FileSystem();

    enum class file_system_t
    {
        FAT12 = 0, /**< un-supported */
        FAT16,     /**< un-supported */
        FAT32      /**< supported */
    };

    enum class media_type_t
    {
        REMOVABLE_DISK = 0xF0,
        FIXED_DISK = 0xF8
    };

    struct PrimaryPartition
    {
        uint16_t boot_flag;    // ignore
        uint16_t chs_begin[2]; // ignore
        uint16_t type_code;    // check to make sure its 0x0B or 0x0C
        uint16_t chs_end[2];   // ignore
        uint16_t lba_begin[2]; // adress of start of FAT32 file system, i.e., VolumeID
        uint16_t number_of_sectors[2]; // ignore
    };

    struct FAT32MasterBootRecord
    {
        // 446 bytes of Boot Code ignored
        PrimaryPartition primary_partition_1;
        PrimaryPartition primary_partition_2;
        PrimaryPartition primary_partition_3;
        PrimaryPartition primary_partition_4;
        uint16_t mbr_signature; // should be 0x55AA, always check this
    };

    struct FAT32VolumeID
    {
        uint16_t jmp_to_boot_code;
        uint16_t oem_name_ascii[8];
        uint16_t bytes_per_sector; // always 512 in FAT32?
        uint16_t sectors_per_cluster;
        uint16_t size_of_reserved_area_sectors;
        uint16_t number_of_fats; // usually 2
        uint16_t max_num_files_in_root_dir; // 0 for FAT32
        uint16_t number_of_sectors_in_file_system; // if 0 then see 4 bytes in bytes 32-25
        media_type_t media_type;
        uint16_t size_of_each_fat_in_sectors; // 0 for FAT32?
        uint16_t sectors_per_track_in_storage_device;
        uint16_t num_heads_in_storage_device;
        uint16_t num_of_sectors_before_start_partition[2];
        uint16_t num_of_sectors_in_file_system_extended[2]; // 0 if 2B filed above is non zero

        uint16_t volume_id_signature; // ?????????
    };

    FAT32MasterBootRecord get_fat_32_master_boot_record() const;

    FAT32VolumeID get_fat_32_volume_id() const;

  private:
    bool read_fat32_master_boot_record();

    bool read_fat_32_volume_id();

    sd_driver::SDCard &sd_card;

    const file_system_t &file_system_type;

    FAT32MasterBootRecord fat_32_master_boot_record;

    FAT32VolumeID fat_32_volume_id;
};
} // namespace file_system

#endif // _FILESYSTEM_H_