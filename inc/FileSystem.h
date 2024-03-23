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

    enum class directory_entry_t
    {
        VOLUME_LABEL = 0,
        DIRECTORY_ENTRY,
        FILE_ENTRY,
        LFN_ENTRY

    };

    /**
     * @brief A static constexpr evaluated at compile time to be used for declaring the
     * length of the file_system_entrys[] which is where directory entries (files, folders 
     * and volume label are stored)
     */
    constexpr static uint16_t total_directory_entries = 100U;

    /**
     * @brief Primary Partition stores a primary partition from the Master Boot Sector (MBR)
     * 
     * @details For simplicity every byte is stored in its own uint16_t, while not efficient
     * it's more readable. NOTE: all addresses (or multi byte values) are stored in Big Endian
     * format where the MSB is stored at index 0 and so forth.
     */
    struct FAT32PrimaryPartition
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
         * to the SD card via a command it should be passed in Big Endian format. THEREFORE,
         * when reading in Little Endian values I convert them to Big Endian
         */
        uint16_t lba_begin[4]; // adress of start of FAT32 file system, i.e., VolumeID
        uint16_t number_of_sectors[4]; // ignore
    };

    struct FAT32MasterBootRecord
    {
        // 446 bytes of Boot Code ignored

        // TODO make an array of partitions, also read them all in read_fat32_master_boot_record()
        FAT32PrimaryPartition primary_partition_1;
        FAT32PrimaryPartition primary_partition_2;
        FAT32PrimaryPartition primary_partition_3;
        FAT32PrimaryPartition primary_partition_4;
        uint16_t mbr_signature[2]; // should be 0x55AA, always check this
    };

    /**
     * @brief Volume ID that stores information about the FAT32 file system
     * 
     * @details For simplicity every byte is stored in its own uint16_t, while not efficient
     * it's more readable. SD cards send multi-byte data in Little Endian format. HOWEVER, 
     * since SD cards expect commands in Big Endian format (and b/c Big Endian reads left to
     * right like you'd normally read a number) I convert all these Little Endian multi-byte
     * values into Big Endian
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

    /**
     * @brief Represents a valid directory entry in the file system which is either 
     * a volume label, directory, file OR LFN entry.
     * 
     * @details A short directory entry and contains all the information needed such as the 
     * name, creation time, file size, starting cluster address, etc in a single 32 byte entry, 
     * it can exist on its own without additional information. Compared to a LFN entry that really 
     * only contains characters (and a sequence identifying byte, checksum, etc.) and is 
     * accompanied by 0 or more other LFN entries AND one short entry. A short entry
     * must always accompany a set of 1 or more LFN entries and contains both a shortened name
     * of the LFN entry as well as all the important aforementioned information. FAT32 was designed
     * in a way to be backwards compatible with systems that did not support LFN's, meaning that
     * LFN entry's can technically be ignored since all the information (including a condensed name)
     * is present in the short entry
     * 
     * 
     * E.g., of root directory with different various directory entries
     * 
     * [Volume Label]
     * [Short Entry]
     * [LFN Entry]              \
     * [LFN Entry]               | -> Together form a single long file entry
     * [Short Entry]            /  -> Alternatively the S.E contains all the data w/ shortened name
     */
    struct FAT32FileSystemEntry
    {
        /**
         * @brief Only if an entry is in use should this flag be set, it indicates that this
         * entry is used to store a short entry or LFN. If this flag is set the parent_directory
         * pointer should be set (and if its not the entry is then the root directory)
         */
        bool entry_in_use = false;

        /**
         * @brief Any entry in the file system must have a parent directory, except the root
         * directory which has no parent (therefore nullptr). This is used for storing and 
         * keeping the traversability of the tree like structure of a file system into a 
         * linear data structure (i.e, array)
         */
        FAT32FileSystemEntry *parent_directory = nullptr;

        /**
         * @brief Entrys that are deleted, i.e., the first byte of the entry or 
         * name_of_entry[0] is 0xE5 and that means they can be overwritten. It ALSO
         * means that the data contained in the entry is invalid.
         */
        bool deleted_directory_entry;

        /**
         * @brief Contains information about what type of entry, this is boiled down 
         * into the directory_entry_t 
         */
        uint16_t attribute_byte;

        /**
         * @brief Type of directory entry, determines which part of the structs data
         * members are valid
         */
        directory_entry_t entry_type; 

        // Short Directory Entry (everthing execpt a LFN)
        //==============================================================================================================================================
        /**
         * @brief Name of directory entry in ASCII, follows convential 8.3 format
         * , i.e., 8 characters for name and 3 for extension.
         * 
         * File name that falls within the 11 bytes (from myfile.txt) (spaces make up extra space)
         *      E.g., "MYFILE  TXT"
         * 
         * File name that has been shortened (from verylongname.txt)
         *      E.g., "VERYLO~1AMETXT"
         */
        char name_of_entry[11];

        /**
         * @brief Address of the first cluster of the entry, stored in Big Endian
         */
        uint16_t starting_cluster_address[4];

        /**
         * @brief Size of entry in bytes, for directories this is 0. Byte order is Big Endian
         */
        uint16_t size_of_entry_in_bytes[4];
        //==============================================================================================================================================

        // Long Directory Entry
        //==============================================================================================================================================
        /**
         * @brief This byte gives information as to where this LFN belongs in the sequence 
         * of a long file name. As a note LFN come in a reverse sequence, meaning the last
         * comes first (think little endian), the last one in the sequence (which is the 
         * first encountered when reading it from memory) is OR'ed with 0x40. E.g., an 
         * example where three LFN's come before a short entry. Another worthwhile thing
         * to mention is the use of 1 based indexing, i.e., the last LFN encountered (
         * which happens to be the first) has an index of 0x01
         * 
         * 0x43 "me.txt"
         * 0x02 "y long filena"
         * 0x01 "File with ver"
         * [Short Entry here with condensed name]
         * 
         */
        uint16_t sequence_order_index_byte;

        /**
         * @brief Checksum for ??
         */
        // TODO read into how check sum works for LFN
        uint16_t checksum;

        /**
         * @brief In a LFN entry the characters are encoded in UNICODE, meaning that each 
         * character corresponds to TWO bytes, in this case each uint16_t stores two bytes.
         * A value of 0xFFFF indicates that the character is unused, this would typically
         * be present in the last entry(if there are multiple LFN's that make total entry)
         */
        uint16_t long_name_of_entry[13];
        //==============================================================================================================================================
    };

    FAT32MasterBootRecord get_fat_32_master_boot_record() const;

    FAT32VolumeID get_fat_32_volume_id() const;

    /**
     * @brief Attempts to delete a file with the given name, at the specified absolute file path
     *              
     *              FILE NAMES/ FOLDER NAMES SHOULD BE IN ALL CAPS
     * 
     * @param file_name name of file in ASCII equivalent, if length of file name is < 11 pad with ascii spaces (i.e., 0x20) between
     *                  the filename and file extension, null terminator (i.e., 0x0) should not be included, also the last three
     *                  characters, occupying index's [8], [9] & [10] are always the file extension
     *                      e.g., myfile.txt --> MYFILE  TXT
     * @param num_enclosing_directories the depth of the file in the file system, if in root directory should be 0
     *                      e.g., if path to file is /afolder/myfile.txt  the depth is 1
     * @param enclosing_directory_names is a 2D array of directory names, that specify the absolute path from the root to the file. Index 0
     *                  must always be the directory that directly encloses the file, the next (i.e., 1) encloses that directory, and so on
     *                      e.g., if the 'normal' absolute path to a file is '/folderA/folderB/myfile.txt' then at index 0 should be stored
     *                            "FOLDERB    " and at index 1 should be "FOLDERA    "
     *                  IMPORTANT: the folders should be in the order from the one that encloses the file back up to the
     *                          folder thats stored in the root directory, so backwards to how its normally seen
     * @return true file was succesfully deleted
     * @return false file was not deleted (i.e., operation failed for some reason such as file not existing)
     */
    bool delete_file(const uint16_t (&file_name)[11], const uint16_t &num_enclosing_directories, const uint16_t (&enclosing_directory_names)[10][11]);

  private:
    bool read_fat32_master_boot_record();

    /**
     * @brief reads the volume id, which should be the first sector of the file system
     * 
     * @details The address should be passed in Big Endian format, i.e., how you would normally
     * read a number from left to right
     * 
     * @param block_address sector address of volume id
     * @return true if there is a valid signature in the last two bytes of the sector
     * @return false if there is an invalid signature in the last two bytes of the sector
     */
    bool read_fat_32_volume_id(const uint16_t (&block_address)[4]);

    /**
     * @brief Helper function to add 4 byte numbers. i.e., add together two numbers, each of 4 bytes
     * 
     * @details Assumes that every uint16_t can store at most 0xFF, anything greater introduces a carry.
     * Additionally, any two numbers when added together that produce a carry on the MSB will be discarded.
     * i.e,. the max number returned is 0x FF FF FF FF
     * 
     * Numbers are expected to be in Big Endian format where byte 1 is the MSB
     */
    void add_4_byte_numbers(const uint16_t (&number_1)[4], const uint16_t (&number_2)[4], uint16_t (&result)[4]);

    /**
     * @brief Helper function to subtract 4 byte numbers. i.e., subtract two numbers, each of 4 bytes
     * 
     * @details Assumes that num1 > num2
     * 
     * @param number_1 minuend of the expression
     * @param number_2 subtrahend of the expression
     * @param result of subtraction operation
     */
    void subtract_4_byte_numbers(const uint16_t (&number_1)[4], const uint16_t (&number_2)[4], uint16_t (&result)[4]);

    /**
     * @brief Recursively explores a directory and stores its contents (if a valid file/ directory) 
     * in file_system_entrys[]
     * 
     * @param directory_begin_sector_addr sector address of directory in big endian format
     * @param parent_directory reference/ pointer to parent directory (nullptr is root)
     * 
     * @return true indicates that recursive read operation was succesful 
     * @return false indicates that recursive read operation was unsuccesful 
     * b/c it was stopped due to excessive recursion
     */
    bool read_directory_recursive(const uint16_t (&directory_begin_sector_addr)[4], FAT32FileSystemEntry *parent_directory);

    /**
     * @brief Given a 4-byte cluster number in Big Endian format calculate the sector address it corresponds to 
     * in Big Endian format
     * 
     * @param cluster_number 
     * @param resulting_sector_address 
     */
    void calculate_sector_address_from_cluster_number(const uint16_t (&cluster_number)[4], uint16_t (&resulting_sector_address)[4]);

    /**
     * @brief Given a 4-byte cluster number in Big Endian format calculate the offset (in sectors) from the beginning of a FAT table
     * where the cluster information about the next cluster in the chain is stored
     * 
     * @param cluster_number The cluster number we wish to find FAT table
     * @param fat_sector_offset This is a returned value that contains which sector of the FAT table the cluster number is in
     * @param index This is a returned value that contains the index of the cluster number within the sector, technically this 
     * should be the cluster number % 128 
     */
    void calculate_fat_sector_offset_from_cluster_number(const uint16_t(&cluster_number)[4], uint16_t (&fat_sector_offset)[4], uint16_t &index);

    sd_driver::SDCard &sd_card;

    const file_system_t &file_system_type;

    FAT32MasterBootRecord fat_32_master_boot_record;

    FAT32VolumeID fat_32_volume_id;

    FAT32FileSystemEntry file_system_entrys[total_directory_entries];

    // tracks where we are in the file systems entry array
    uint16_t file_systems_entry_index = 0U;

    /**
     * @brief The sector address (lba) of the first cluster of the data region, normally the 
     * root directory starts here but not guaranteed
     * 
     * @details Calculated using:
     * 
     * cluster_begin_lba = Partition_LBA_Begin + Number_of_Reserved_Sectors + (Number_of_FATs * Sectors_Per_FAT);
     * 
     */
    uint16_t cluster_begin_lba[4] = {0x00, 0x00, 0x00, 0x00};
};
} // namespace file_system

#endif // _FILESYSTEM_H_
