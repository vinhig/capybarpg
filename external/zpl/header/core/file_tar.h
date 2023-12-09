// file: header/core/file_tar.h

/** @file file_tar.c
@brief Tar archiving module
@defgroup fileio Tar module

Allows to easily pack/unpack files.
Based on: https://github.com/rxi/microtar/

Disclaimer: The pack method does not support file permissions nor GID/UID information. Only regular files are supported.
Use zpl_tar_pack_dir to pack an entire directory recursively. Empty folders are ignored.

@{
*/


ZPL_BEGIN_C_DECLS

typedef enum {
    ZPL_TAR_ERROR_NONE,
    ZPL_TAR_ERROR_INTERRUPTED,
    ZPL_TAR_ERROR_IO_ERROR,
    ZPL_TAR_ERROR_BAD_CHECKSUM,
    ZPL_TAR_ERROR_FILE_NOT_FOUND,
    ZPL_TAR_ERROR_INVALID_INPUT,
} zpl_tar_errors;

typedef enum {
    ZPL_TAR_TYPE_REGULAR    = '0',
    ZPL_TAR_TYPE_LINK       = '1',
    ZPL_TAR_TYPE_SYMBOL     = '2',
    ZPL_TAR_TYPE_CHR        = '3',
    ZPL_TAR_TYPE_BLK        = '4',
    ZPL_TAR_TYPE_DIR        = '5',
    ZPL_TAR_TYPE_FIFO       = '6'
} zpl_tar_file_type;

typedef struct {
    char type;
    char *path;
    zpl_i64 offset;
    zpl_i64 length;
    zpl_isize error;
} zpl_tar_record;

#define ZPL_TAR_UNPACK_PROC(name) zpl_isize name(zpl_file *archive, zpl_tar_record *file, void* user_data)
typedef ZPL_TAR_UNPACK_PROC(zpl_tar_unpack_proc);

/**
 * @brief Packs a list of files
 * Packs a list of provided files. Note that this method only supports regular files
 * and does not provide extended info such as GID/UID or permissions.
 * @param archive archive we pack files into
 * @param paths list of files
 * @param paths_len number of files provided
 * @return error
 */
ZPL_DEF zpl_isize zpl_tar_pack(zpl_file *archive, char const **paths, zpl_isize paths_len);

/**
 * @brief Packs an entire directory
 * Packs an entire directory of files recursively.
 * @param archive archive we pack files to
 * @param path folder to pack
 * @param alloc memory allocator to use (ex. zpl_heap())
 * @return error
 */
ZPL_DEF zpl_isize zpl_tar_pack_dir(zpl_file *archive, char const *path, zpl_allocator alloc);

/**
 * @brief Unpacks an existing archive
 * Unpacks an existing archive. Users provide a callback in which information about file is provided.
 * Library does not unpack files to the filesystem nor reads any file data.
 * @param archive archive we unpack files from
 * @param unpack_proc callback we call per each file parsed
 * @param user_data user provided data
 * @return error
 */
ZPL_DEF zpl_isize zpl_tar_unpack(zpl_file *archive, zpl_tar_unpack_proc *unpack_proc, void *user_data);

/**
 * @brief Unpacks an existing archive into directory
 * Unpacks an existing archive into directory. The folder structure will be re-created automatically.
 * @param archive archive we unpack files from
 * @param dest directory to unpack files to
 * @return error
 */
ZPL_DEF_INLINE zpl_isize zpl_tar_unpack_dir(zpl_file *archive, char const *dest);

ZPL_DEF ZPL_TAR_UNPACK_PROC(zpl_tar_default_list_file);
ZPL_DEF ZPL_TAR_UNPACK_PROC(zpl_tar_default_unpack_file);

//! @}

ZPL_IMPL_INLINE zpl_isize zpl_tar_unpack_dir(zpl_file *archive, char const *dest) {
    return zpl_tar_unpack(archive, zpl_tar_default_unpack_file, cast(void*)dest);
}

ZPL_END_C_DECLS
