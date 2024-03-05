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

    /**
     * @brief Primary Partition stores a primary partition from the Master Boot Sector (MBR)
     * 
     * @details For simplicity every byte is stored in its own uint16_t, while not efficient
     * it's more readable.
     */
    struct PrimaryPartition
    {
        uint16_t boot_flag;    // ignore (0x80 active?, 0x00 inactive?)
        uint16_t chs_begin[3]; // ignore
        uint16_t type_code;    // check to make sure its 0x0B or 0x0C
        uint16_t chs_end[3];   // ignore

        /**
         * @brief 4 byte address of the Volume ID of the FAT32 file system.
         * 
         * @details Multi byte values (such as this address) are stored in Little Endian 
         * format on SD cards, meaning the LSB comes first. However when passing an address
         * to the SD card via a command it should be passed in Big Endian format. Meaning 
         * if this Little Endian byte ordered address would be passed back to the SD card
         * the byte ordering should be reversed to put it in Big Endian format
         * 
         */
        uint16_t lba_begin[4]; // adress of start of FAT32 file system, i.e., VolumeID
        uint16_t number_of_sectors[4]; // ignore
    };

    struct FAT32MasterBootRecord
    {
        // 446 bytes of Boot Code ignored

        // TODO make an array of partitions, also read them all in read_fat32_master_boot_record()
        PrimaryPartition primary_partition_1;
        PrimaryPartition primary_partition_2;
        PrimaryPartition primary_partition_3;
        PrimaryPartition primary_partition_4;
        uint16_t mbr_signature[2]; // should be 0x55AA, always check this
    };

    /**
     * @brief Volume ID that stores information about the FAT32 file system
     * 
     * @details For simplicity every byte is stored in its own uint16_t, while not efficient
     * it's more readable. REMEMBER reading multibyte values from SD card are in Little Endian,
     * i.e., 
     */
    struct FAT32VolumeID
    {
        uint16_t jmp_to_boot_code[3];
        uint16_t oem_name_ascii[8];
        uint16_t bytes_per_sector[2]; // always 512 in FAT32?
        uint16_t sectors_per_cluster;
        uint16_t size_of_reserved_area_sectors[2];
        uint16_t number_of_fats; // usually 2
        uint16_t max_num_files_in_root_dir[2]; // 0 for FAT32
        uint16_t number_of_sectors_in_file_system[2]; // if 0 then see 4 bytes in bytes 32-25
        media_type_t media_type;
        uint16_t size_of_each_fat_in_sectors[2]; // 0 for FAT32?
        uint16_t sectors_per_track_in_storage_device[2];
        uint16_t num_heads_in_storage_device[2];
        uint16_t num_of_sectors_before_start_partition[4];
        uint16_t num_of_sectors_in_file_system_extended[4]; // 0 if 2B filed above is non zero
        uint16_t sectors_per_fat[4];
        uint16_t root_directory_first_cluster[4]; // usually 2

        uint16_t volume_id_signature[2]; // should be 0x55AA or 0xAA55
    };

    FAT32MasterBootRecord get_fat_32_master_boot_record() const;

    FAT32VolumeID get_fat_32_volume_id() const;

  private:
    bool read_fat32_master_boot_record();

    /**
     * @brief reads the volume id, which should be the first sector of the file system
     * 
     * @details The bytes should be passed in Big Endian format, i.e., how you would normally
     * read a number from left to right
     * 
     * @param lba_begin_byte1 Most Significant Byte(MSB) of address
     * @param lba_begin_byte2 
     * @param lba_begin_byte3 
     * @param lba_begin_byte4 Least Significant Byte(LSB) of address
     * @return true 
     * @return false 
     */
    bool read_fat_32_volume_id(const uint16_t lba_begin_byte1, const uint16_t lba_begin_byte2, const uint16_t lba_begin_byte3, const uint16_t lba_begin_byte4);

    sd_driver::SDCard &sd_card;

    const file_system_t &file_system_type;

    FAT32MasterBootRecord fat_32_master_boot_record;

    FAT32VolumeID fat_32_volume_id;
};
} // namespace file_system

#endif // _FILESYSTEM_H_
